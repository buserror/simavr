# Design Plan: Adding ATtiny3217 (modern AVR / AVRxt) support to simavr

Status: Proposal / design. Target device: **ATtiny3217** (tinyAVR 1-series).

---

## 1. Goal & scope

Add a working simavr core for the ATtiny3217 so that firmware built with a
modern avr-gcc + ATtiny_DFP toolchain can be loaded and executed with enough
peripheral fidelity to be useful (GPIO, timers, USART, basic ADC, interrupts).

This is **not** "just another core file". Every existing simavr core
(`sim_mega328.c`, `sim_tiny85.c`, …) is a classic AVR (AVRe/AVRe+). The
ATtiny3217 uses the **AVRxt** core with the **modern peripheral architecture**
(the same family as megaAVR-0, AVR DA/DB). simavr's *engine* bakes in several
classic-AVR assumptions that do not hold for modern AVR. The bulk of the work is
in the engine, not the core descriptor.

---

## 2. Device facts (from `avr/iotn3217.h`, DFP 3.4.278)

| Property | Value |
|---|---|
| Core | AVRxt |
| Flash | 32 KB, `0x0000–0x7FFF`, page 128 B; also mapped into data space at `0x8000` |
| SRAM | 2 KB at `0x3800–0x3FFF` (`RAMEND=0x3FFF`) |
| EEPROM | 256 B at `0x1400`, mapped in data space |
| Signature | `1E 95 22` |
| Fuses | `FUSE_MEMORY_SIZE = 10` (10 fuse bytes, mapped at `0x1280`) |
| CCP register | `0x0034`, signatures `0x9D` (SPM) / `0xD8` (IOREG) |
| SP | data `0x3D/0x3E`; SREG data `0x3F` |
| Interrupt vectors | 34 vectors via the **CPUINT** controller |

### Data-space memory map (the important part)
```
0x0000–0x003F  low I/O (VPORTA..C, GPIOR0..3, CCP, SP, SREG)
0x0040–0x1FFF  extended I/O — all modern peripherals live here:
               RSTCTRL 0x40, SLPCTRL 0x50, CLKCTRL 0x60, BOD 0x80, VREF 0xA0,
               WDT 0x100, CPUINT 0x110, CRCSCAN 0x120, RTC 0x140, EVSYS 0x180,
               CCL 0x1C0, PORTMUX 0x200, PORTA/B/C 0x400/0x420/0x440,
               ADC0 0x600, ADC1 0x640, AC0..2 0x680.., DAC0..2 0x6A0..,
               USART0 0x800, TWI0 0x810, SPI0 0x820,
               TCA0 0xA00, TCB0/1 0xA40/0xA50, TCD0 0xA80, SYSCFG 0xF00
0x1000–0x13FF  NVMCTRL regs, SIGROW, FUSE, USERROW
0x1400–0x14FF  EEPROM (mapped)
0x3800–0x3FFF  SRAM
0x8000–0xFFFF  Flash (mapped, for unified LD/`__flash` access)
```

---

## 3. Gap analysis — why the engine needs changes

These are the concrete blockers found in the current source.

1. **I/O callback table is far too small and wrongly addressed.**
   `MAX_IOs = 280` (`sim_avr.h:86`). The `avr->io[]` callback array is indexed
   by `AVR_DATA_TO_IO(addr) = addr-32`. Modern peripherals span data
   `0x0040–0x1FFF`, i.e. I/O indices up to ~8160. The table cannot reach them.
   The header even flags this: *"If you wanted to emulate … XMegas, this would
   need work."*

2. **The classic IO/data 0x20 offset is hardwired into the decoder.**
   `IN/OUT/SBI/CBI/SBIC/SBIS` add `+32` to the I/O operand to get the data
   address (`sim_core.c:542,575`; macros in `sim_core_declare.h:32-36`). On
   modern AVR the low-I/O space (`IN/OUT`, addresses `0x00–0x3F`) maps to data
   `0x00–0x3F` with **no** `+0x20` offset. VPORTs and `CCP`/`SP`/`SREG` are
   reached this way.

3. **SP and SREG positions are compile-time constants for the classic map.**
   `R_SPL = 32+0x3d (=0x5D)`, `R_SREG = 32+0x3f (=0x5F)` (`sim_avr.h:80-83`),
   used directly in `sim_core.c` (lines 297, 346, 351, 370…). On modern AVR
   SP=`0x3D/0x3E`, SREG=`0x3F`. The stack/SREG accessors must become
   per-core, not enum constants.

4. **Reset/IO vector & init plumbing assumes classic SFRs.**
   `sim_core_declare.h` `DEFAULT_CORE` references `MCUSR`, `LFUSE/HFUSE/EFUSE`,
   `RAMSTART`, etc. Modern AVR has none of these (reset flags live in
   `RSTCTRL.RSTFR`; 10 fuse bytes). A modern `DEFAULT_CORE` variant is needed.

5. **No Configuration Change Protection (CCP) model.** Writes to protected
   registers (CLKCTRL, WDT, many others) require writing a signature to `CCP`
   first, granting a 4-instruction unlock window. Nothing in the engine
   models this.

6. **Interrupt controller model mismatch.** simavr's `sim_interrupts.c` is built
   around per-peripheral enable/raise reg-bits and a flat priority. CPUINT adds:
   a single vector table base (`CPUINT.STATUS`/`LVL0/LVL1`/NMI), round-robin
   scheduling, and **interrupt-flag-in-peripheral** semantics (e.g. `INTFLAGS`
   registers, flag cleared by writing 1). The existing reg-bit interrupt
   primitive mostly fits, but vector dispatch and NMI need a modern path.

7. **Instruction timing differs (AVRxt vs AVRe+).** Several instructions have
   different cycle counts on AVRxt: `PUSH` 1 (was 2), `CBI/SBI` 1 (was 2),
   `LDS` 3, `ST`→2, `CALL`/`RCALL` one cycle less, etc. The decoder hardcodes
   classic cycle counts.

8. **Vector size.** With 16 K-word flash the toolchain emits 4-byte (`JMP`)
   vectors. `vector_size` must be confirmed and set accordingly (likely 4).

---

## 4. Architectural decision

Introduce an explicit **"modern AVR" mode** in the engine rather than trying to
make one code path serve both. Keep the classic path byte-for-byte unchanged to
avoid regressions across the dozens of existing cores.

Mechanism: add a small descriptor to `avr_t`, e.g.

```c
struct {
    uint8_t  io_offset;     // 0x20 classic, 0x00 modern (IN/OUT/SBI/CBI base)
    uint16_t sp_addr;       // data addr of SPL (0x5D classic, 0x3D modern)
    uint16_t sreg_addr;     // data addr of SREG (0x5F classic, 0x3F modern)
    uint8_t  timing;        // AVR_TIMING_CLASSIC | AVR_TIMING_XT
    uint8_t  has_ccp;       // enable CCP unlock model
} arch;
```

Replace the `R_SPL`/`R_SREG`/`+32` literals in `sim_core.c` with these fields.
For the classic cores the values are initialised to the current constants, so
behaviour is identical. This keeps the change surgical and testable.

---

## 5. Work breakdown (phased)

### Phase 0 — Plumbing & build (low risk)
- Add `arch` fields to `avr_t`; default-init to classic values in
  `avr_core_allocate`/`avr_init`.
- Confirm the core auto-discovery (Makefile greps `cores/*.c` for `avr_kind_t`,
  builds `sim_core_decl.h`) and the `CONFIG_*` gating pick up the new file with
  no Makefile edits — it should, by convention.
- Add a guarded include path for the ATtiny_DFP headers (they ship in
  `~/.mchp_packs/Microchip/ATtiny_DFP/.../include` and avr-gcc 16 already has
  `iotn3217.h`).

### Phase 1 — Engine: addressing model (the hard core change)
- Enlarge / restructure the I/O callback dispatch so addresses up to `0x1FFF`
  can carry read/write callbacks. Two options:
  - **(a) Grow `MAX_IOs`** to cover `0x2000` data bytes (~8 KB of callback
    structs). Simple, memory cost ~hundreds of KB per core — acceptable.
  - **(b) Sparse/segment dispatch** for the modern map only. More code, less
    memory. *Recommendation: start with (a)* behind the modern flag; optimise
    later if needed.
- Parameterise the `+32` IO offset and `R_SPL/R_SREG` accesses in `sim_core.c`
  using the `arch` fields.
- Make `data[]` allocation cover the full modern data span up to `RAMEND`
  (already driven by `ramend`; verify flash-mapping region `0x8000+` is handled
  for `LD`/`LPM`-style reads — modern code uses `LD` from mapped flash).

### Phase 2 — Engine: AVRxt instruction timing & CCP
- Add an `AVR_TIMING_XT` branch to the cycle accounting in `sim_core.c` for the
  handful of instructions that differ. (Functional behaviour is unchanged; only
  `cycle++` counts differ.)
- Implement CCP: a tiny state machine watching writes to `0x0034`; on the right
  signature, open an N-instruction window during which protected registers
  accept writes. Model as a write-callback on the CCP address plus a
  countdown decremented per instruction.

### Phase 3 — Engine: CPUINT interrupt controller
- Add a modern interrupt-dispatch path: single vector table, LVL0/LVL1/NMI,
  round-robin "last acknowledged vector" tracking.
- Keep using the existing `avr_int_vector` reg-bit primitive for
  enable/raised bits, but point "raised" at peripheral `INTFLAGS` bits and
  honour write-1-to-clear semantics.

### Phase 4 — Peripherals (incremental, prioritised)
Implement as new `avr_*` modules under `simavr/sim/` mirroring the existing
pattern (`avr_io_t` self-registration via `avr_register_io_write/read`). Modern
peripherals are register-block based, so each module takes a base address.

Priority order (most firmware needs the first tier):

1. **CLKCTRL** — needed so `frequency` is right (20/16 MHz osc + prescaler).
   Mostly a register model that recomputes `avr->frequency`.
2. **PORT + VPORT + PORTMUX** — `avr_ioport.c` analogue. VPORT is a
   bit-addressable alias of PORT `OUT/IN/DIR`; pin-change/port ISR via
   `PORTx.INTFLAGS`. Reuse the existing IOPORT IRQ/pin abstraction.
3. **CPUINT-driven SysTick sources: TCB** (simple), then **TCA0**
   (16-bit, split mode, WGM modes, compare outputs). Model on `avr_timer.c`.
4. **USART0** — modern register set (`CTRLA/B/C`, `STATUS`, `RXDATAL/H`,
   `TXDATAL`, `BAUD`). Reuse `avr_uart.c` IRQ/fifo plumbing; new register glue.
5. **RTC + PIT** — periodic interrupt, common as a tick source.
6. **NVMCTRL** — EEPROM + flash self-program via command register (replaces
   classic `SPMCSR`/`EECR`). Reuse `avr_eeprom.c`/`avr_flash.c` back-ends.
7. **SPI0**, **TWI0** — modern register glue over existing `avr_spi.c`/
   `avr_twi.c` back-ends.
8. **ADC0** — new 10/12-bit modern ADC. Reuse `avr_adc.c` IRQ model.
9. Lower priority / stubs first: **AC, DAC, CCL, EVSYS, CRCSCAN, VREF, BOD,
   SLPCTRL, RSTCTRL, WDT(new), TCD0**. Provide register-storage stubs so
   firmware that merely writes config doesn't crash; flesh out on demand.

### Phase 5 — The core descriptor
Create `simavr/cores/sim_tiny3217.c` (+ optional shared
`sim_tinyxy7.h` template for the 3216/3217/1617 family), following the
`sim_tiny85.c` pattern:
```c
#define SIM_MMCU      "attiny3217"
#define SIM_CORENAME  mcu_tiny3217
#define SIM_VECTOR_SIZE 4            // confirm via toolchain
#include "avr/iotn3217.h"
#include "sim_tinyxy7.h"            // declares struct mcu_t + peripheral wiring
avr_kind_t tiny3217 = { .names = { "attiny3217" }, .make = make };
```
A **modern `DEFAULT_CORE` variant** in a new `sim_core_declare_modern.h` fills
`ramend/flashend/e2end/signature`, the 10-byte fuse array, modern reset flags
(`RSTCTRL.RSTFR`), and the `arch` block (io_offset=0, sp=0x3D, sreg=0x3F,
timing=XT, has_ccp=1).

---

## 6. Testing strategy

- **Unit firmware** under `tests/` built with the modern toolchain
  (`-mmcu=attiny3217`), one per peripheral: blink (PORT), TCB/TCA periodic
  interrupt count, USART loopback, EEPROM write/read, RTC tick.
- Reuse simavr's existing test harness (`tests/` + `run_avr`); assert on
  GPIO IRQ traces / VCD output and on cycle counts for timing checks.
- **Cross-check cycle timing** of a known loop against the AVRxt datasheet
  numbers to validate the timing branch.
- Add a CI matrix entry only once the toolchain is reliably available
  (the DFP path is non-standard, so gate the modern tests behind a
  `HAVE_MODERN_AVR_TOOLCHAIN` make probe).

---

## 7. Risks & open questions

- **Memory cost of growing `MAX_IOs`** to ~0x2000 entries per core (struct is
  several pointers). Acceptable for one instance; revisit if many cores are
  instantiated. The sparse-dispatch fallback (option b) mitigates.
- **Vector size (2 vs 4 bytes)** for 16 KW flash — verify against the linked
  ELF (`avr-objdump -d` of a built vector table) before fixing `SIM_VECTOR_SIZE`.
- **Flash-in-data-space reads** (`0x8000+`): confirm `_avr_get_ram`/`LD`
  decode path resolves mapped flash; modern code reads `const __flash` via `LD`.
- **CCP window length / exact protected-register set** — follow the datasheet
  "Sequence for write operation to configuration change protected I/O
  registers".
- **CPUINT round-robin & NMI** corner cases (CRCSCAN NMI is vector 1) — model
  may need iteration to match priority behaviour.
- Scope creep across ~15 new peripherals — mitigate by shipping **stubs first**
  (register storage, no behaviour) so firmware boots, then deepening the
  high-value peripherals.

---

## 8. Suggested milestones

1. **M1 – Boots & blinks:** engine addressing/SP/SREG/timing + CCP + CLKCTRL +
   PORT/VPORT + CPUINT. A blink firmware runs and toggles a pin (verifiable via
   IRQ/VCD).
2. **M2 – Timers & serial:** TCB/TCA + USART0 + RTC. Periodic-interrupt and
   UART-loopback tests pass.
3. **M3 – Memory & buses:** NVMCTRL (EEPROM/selfprog) + SPI0 + TWI0 + ADC0.
4. **M4 – Breadth & polish:** remaining peripherals beyond stubs, cycle-timing
   validation, CI integration, docs.

---

## Implementation progress

### Phase 1 — addressing model — **DONE**
Architecture-variant abstraction (`avr->arch`: `flags`, `io_offset`, `sp_addr`,
`sreg_addr`); IO callback table converted to a per-core dynamically-sized
allocation (`io` pointer + `io_count`). Decoder, gdb stub and IO registration
parameterised. Classic cores unchanged (regression-tested). See audit notes.

### Phase 2 — AVRxt timing + CCP + flash-in-data-space — **DONE**
- **AVRxt instruction timing** (gated by `AVR_ARCH_F_XT_TIMING`), per the AVR
  Instruction Set Manual DS40002198 AVRe-vs-AVRxt cycle tables:
  ST/STD −1 (→1), PUSH −1 (→1), SBI/CBI −1 (→1), CALL/RCALL/ICALL −1,
  LDS +1 (→3). LD/LDD/STS/POP unchanged. `AVR_XT(avr)` macro in `sim_core.c`.
- **CCP** (`AVR_ARCH_F_CCP`): `arch.ccp_addr`/`arch.ccp_window`,
  `avr_ccp_write()` / `avr_ccp_io_write_enabled()`, an internal CCP-register
  write hook auto-installed for modern cores, and a per-instruction window
  countdown in `avr_run_one`. Window = `AVR_CCP_WINDOW` (4) instructions.
- **Flash mapped into data space** (`arch.flashmap_start`): `_avr_get_ram`
  redirects reads at/above the map base to `flash[]`; `_avr_set_ram` ignores
  writes there (self-programming goes via NVMCTRL, Phase 4).
- Verified by `tests/test_avrxt_engine.c` (26 checks, integrated into the
  `make run_tests` harness): every timing delta, CCP open/countdown, and a
  mapped-flash LDS read.

**Phase 2 audit (facts checked against DS40002198 + DS40002205A):**
- Timing baselines verified via the instruction-manual footnotes: `(1)` =
  internal-RAM assumption (simavr is internal-only ⇒ base counts hold);
  `(2)` = AVRxt adds ≥1 cycle for NVM-mapped load/store. So all AVRxt base
  numbers used (ST 1, LDS 3, …) are correct for the SRAM case. Full
  instruction sweep confirmed no AVRxt-differing opcode was missed.
- **Bug found & fixed:** CCP window granted only 3 instructions; the datasheet
  (8.5.7.1) specifies the protected write must occur "within four instructions"
  *after* the CCP write. The end-of-instruction countdown also fired on the
  CCP-writing instruction itself. Fixed by arming to `AVR_CCP_WINDOW + 1`;
  unit test now asserts exactly 4.

Deliberate simplifications (documented, permissive — only affect detection of
*buggy* firmware, never correct firmware):
- CCP window is a plain countdown; it does not close early on the first I/O/data
  write, nor on NVM access / SLEEP (datasheet says it should). Harmless because
  correct firmware writes once, immediately.
- AVRxt `(2)` NVM `+1` cycle on mapped-flash/EEPROM access is not modelled.

**Phase 3 dependency surfaced by the audit:** per DS40002205A 8.5.7, *interrupts
are ignored for the duration of the CCP period* (requests stay pending). When
the CPUINT dispatch is added in Phase 3 it must suppress servicing while
`avr->arch.ccp_window > 0`.

### Phase 3 — CPUINT interrupt controller — **DONE**
A modern interrupt-dispatch path (gated by `AVR_ARCH_F_CPUINT`) added alongside
the untouched classic path. Verified against DS40002205A §13 and Microchip
AN1982 / developer docs:
- **I-bit semantics (the big one):** on modern CPUINT the I bit is *not* cleared
  on interrupt entry and **RETI does not set it**; nesting is governed by the
  execution-level flags in `CPUINT.STATUS` (LVL0EX/LVL1EX/NMIEX). The RETI
  opcode handler and dispatch were gated accordingly.
- **NMI** (`vector.nmi`, e.g. CRCSCAN): serviced regardless of I, top priority,
  cannot be preempted; sets NMIEX.
- **LVL1** (one vector via `CPUINT.LVL1VEC`): preempts a running LVL0 handler.
- **LVL0 scheduling:** static (default lowest-vector-first), modified-static via
  `LVL0PRI`, and round-robin via `LVL0RR` (acknowledged vector becomes lowest
  priority). Implemented as a priority-rank with `LVL0PRI` wrap.
- **Sticky flags:** modern peripherals' INTFLAGS are not auto-cleared on entry —
  reuses the existing per-vector `raise_sticky` (set by the core).
- **CCP suppression:** interrupts are not serviced while `arch.ccp_window > 0`
  (the Phase 2 audit requirement).
- New engine API for the (Phase 4/5) CPUINT register block:
  `avr_cpuint_set_lvl1vec/lvl0pri/lvl0rr()`, `avr_cpuint_get_status()`, plus
  `avr->interrupts.cpuint_*` state.
- Verified by `tests/test_avrxt_engine.c` (now 41 checks): LVL0 dispatch + RETI
  (I untouched, LVL0EX toggled), NMI with I=0, LVL1 preempting LVL0, LVL0 not
  preempting LVL0, static and modified-static priority, CCP suppression. Classic
  interrupt-driven firmware (UART/timer/pin-change/IRQ) regression-clean.

Deferred (refinements, not needed by typical firmware): IVSEL vector relocation
and CVT compact vector table (vectors assumed at flash 0); the SP-write
4-instruction interrupt-hold window (DS §8.5.4); the LVL0 wrap uses the full
vector-number range rather than exact IVEC adjacency (negligible). The CPUINT
register block at 0x110 itself is wired in Phase 4/5 via the API above.

## 9. Files touched (summary)

Engine: `sim_avr.h` (arch fields, MAX_IOs), `sim_avr.c` (init defaults),
`sim_core.c` (offset/SP/SREG params, AVRxt timing), `sim_core_declare.h`
(+ new `sim_core_declare_modern.h`), `sim_interrupts.[ch]` (CPUINT path),
new `avr_ccp.[ch]`.
Peripherals (new): `avr_port_modern.[ch]`, `avr_tca.[ch]`, `avr_tcb.[ch]`,
`avr_rtc.[ch]`, `avr_usart_modern.[ch]`, `avr_nvmctrl.[ch]`, `avr_clkctrl.[ch]`,
`avr_cpuint.[ch]`, plus stubs.
Core: `simavr/cores/sim_tiny3217.c` (+ `sim_tinyxy7.h`).
Tests: `tests/attiny3217_*.c` + harness wiring.
```
