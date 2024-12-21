# SH1106 OLED demo (navigation menu for a motor controller)

See detailed description of the emulated system here https://github.com/jpcornil-git/Cone2Hank

- Use arrows to navigate menu
- \<enter\> to select
- \<space\> to start,pause or stop (long press) motor
- 'v' to start/stop VCD traces
- 'q' or ESC to quit

**NOTE**: Emulation may be slightly slower than realtime and a keypress has to be long enough to cope with that.

## Usage
Command line options:
```
$ sh1106demo.elf --help
Usage: sh1106demo.elf [...] <firmware>
       [--help|-h|-?]      Display this usage message and exit
       [--list-cores]      List all supported AVR cores and exit
       [-v]                Raise verbosity level
                           (can be passed more than once)
       [--freq|-f <freq>]  Sets the frequency for an .hex firmware
       [--mcu|-m <device>] Sets the MCU type for an .hex firmware
       [--gdb|-g [<port>]] Listen for gdb connection on <port> (default 1234)
       [--output|-o <file>] VCD file to save signal traces (default gtkwave_trace.vcd)
       [--start-vcd|-s     Start VCD output from reset
       [--pc-trace|-p      Add PC to VCD traces
       [--add-trace|-at <name=[portpin|irq|trace]@addr/mask or [sram8|sram16]@addr>]
                           Add signal to be included in VCD output
       <firmware>          An ELF file (can include debugging syms)
```
## Examples
### Execute firmware.elf (with no .mmcu section -> -m and -f required) on system
```
$ sh1106demo.elf -m atmega32u4 -f 16000000 firmware_no_mmcu.elf
```
        
### Start system and wait for gdb to connect, load firmware, ...
```
$ sh1106demo.elf -m atmega32u4 -f 16000000 -g
```

### Execute firmware.elf on system and trace signals in a VCD file
- .mmcu section of the firmware includes something like:
  ```
  #include "avr_mcu_section.h"
  
  extern void *__brkval;
  
  AVR_MCU (F_CPU, "atmega32u4" );
  AVR_MCU_VOLTAGES(3300, 3300, 3300);
  AVR_MCU_VCD_FILE("simavr.vcd", 10000000);
  
  const struct avr_mmcu_vcd_trace_t _mytrace[]  _MMCU_ = {
    { AVR_MCU_VCD_SYMBOL("Encoder"), .what = (void*) &PIND, .mask=(1<<PIND2), },
  	{ AVR_MCU_VCD_SRAM_16("Heap"), .what = (void*)(&__brkval), },
  };
  ```
- Options from the command line:
  - Add a 16 bits trace for Stack Pointer (SPL/SPH)  (could also be in the section above)
  - Add a (16 bits) trace for Program Counter (PC)
  - Start VCD dump (set to "simavr.vcd" in the .mmcu section) from reset
```
$ sh1106demo.elf --add-trace Stack=sram16@0x5d -p -s firmware_mmcu.elf 
```
  GTKWave ouput

![PS_SP-Heap_3x](https://github.com/jpcornil-git/simavr/assets/40644331/b4cefeb2-e33f-4c21-afa6-2b15b92eed37)
