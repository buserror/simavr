/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <joerg@FreeBSD.ORG> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.        Joerg Wunsch
 * ----------------------------------------------------------------------------
 *
 * HD44780 LCD display driver
 *
 * The LCD controller is used in 4-bit mode with a full bi-directional
 * interface (i.e. R/~W is connected) so the busy flag can be read.
 *
 * $Id: hd44780.c 2002 2009-06-25 20:21:16Z joerg_wunsch $
 */

#include "avr_hd44780_conf.h"

#include <stdbool.h>
#include <stdint.h>

#include <avr/io.h>
#include <util/delay.h>

#include "avr_hd44780.h"

#define GLUE(a, b)     a##b

/* single-bit macros, used for control bits */
#define SET_(what, p, m) GLUE(what, p) |= (1 << (m))
#define CLR_(what, p, m) GLUE(what, p) &= ~(1 << (m))
#define GET_(/* PIN, */ p, m) GLUE(PIN, p) & (1 << (m))
#define SET(what, x) SET_(what, x)
#define CLR(what, x) CLR_(what, x)
#define GET(/* PIN, */ x) GET_(x)

/* nibble macros, used for data path */
#define ASSIGN_(what, p, m, v) GLUE(what, p) = (GLUE(what, p) & \
						~((1 << (m)) | (1 << ((m) + 1)) | \
						  (1 << ((m) + 2)) | (1 << ((m) + 3)))) | \
					        ((v) << (m))
#define READ_(what, p, m) (GLUE(what, p) & ((1 << (m)) | (1 << ((m) + 1)) | \
					    (1 << ((m) + 2)) | (1 << ((m) + 3)))) >> (m)
#define ASSIGN(what, x, v) ASSIGN_(what, x, v)
#define READ(what, x) READ_(what, x)

#define HD44780_BUSYFLAG 0x80

/*
 * Send one pulse to the E signal (enable).  Mind the timing
 * constraints.  If readback is set to true, read the HD44780 data
 * pins right before the falling edge of E, and return that value.
 */
static inline uint8_t
hd44780_pulse_e(bool readback) __attribute__((always_inline));

static inline uint8_t
hd44780_pulse_e(bool readback)
{
  uint8_t x;

  SET(PORT, HD44780_E);
  /*
   * Guarantee at least 500 ns of pulse width.  For high CPU
   * frequencies, a delay loop is used.  For lower frequencies, NOPs
   * are used, and at or below 1 MHz, the native pulse width will
   * already be 1 us or more so no additional delays are needed.
   */
#if F_CPU > 4000000UL
  _delay_us(0.5);
#else
  /*
   * When reading back, we need one additional NOP, as the value read
   * back from the input pin is sampled close to the beginning of a
   * CPU clock cycle, while the previous edge on the output pin is
   * generated towards the end of a CPU clock cycle.
   */
  if (readback)
    __asm__ volatile("nop");
#  if F_CPU > 1000000UL
  __asm__ volatile("nop");
#    if F_CPU > 2000000UL
  __asm__ volatile("nop");
  __asm__ volatile("nop");
#    endif /* F_CPU > 2000000UL */
#  endif /* F_CPU > 1000000UL */
#endif
  if (readback)
    x = READ(PIN, HD44780_D4);
  else
    x = 0;
  CLR(PORT, HD44780_E);

  return x;
}

/*
 * Send one nibble out to the LCD controller.
 */
static void
hd44780_outnibble(uint8_t n, uint8_t rs)
{
  CLR(PORT, HD44780_RW);
  if (rs)
    SET(PORT, HD44780_RS);
  else
    CLR(PORT, HD44780_RS);
  ASSIGN(PORT, HD44780_D4, n);
  (void)hd44780_pulse_e(false);
}

/*
 * Send one byte to the LCD controller.  As we are in 4-bit mode, we
 * have to send two nibbles.
 */
void
hd44780_outbyte(uint8_t b, uint8_t rs)
{
  hd44780_outnibble(b >> 4, rs);
  hd44780_outnibble(b & 0xf, rs);
}

/*
 * Read one nibble from the LCD controller.
 */
static uint8_t
hd44780_innibble(uint8_t rs)
{
  uint8_t x;

  SET(PORT, HD44780_RW);
  ASSIGN(DDR, HD44780_D4, 0x00);
  if (rs)
    SET(PORT, HD44780_RS);
  else
    CLR(PORT, HD44780_RS);
  x = hd44780_pulse_e(true);
  ASSIGN(DDR, HD44780_D4, 0x0F);
  CLR(PORT, HD44780_RW);

  return x;
}

/*
 * Read one byte (i.e. two nibbles) from the LCD controller.
 */
uint8_t
hd44780_inbyte(uint8_t rs)
{
  uint8_t x;

  x = hd44780_innibble(rs) << 4;
  x |= hd44780_innibble(rs);

  return x;
}

/*
 * Wait until the busy flag is cleared.
 */
void
hd44780_wait_ready(bool longwait)
{
#if USE_BUSY_BIT
  while (hd44780_incmd() & HD44780_BUSYFLAG) ;
#else
  if (longwait)
    _delay_ms(1.52);
  else
    _delay_us(37);
#endif
}

/*
 * Initialize the LCD controller.
 *
 * The initialization sequence has a mandatory timing so the
 * controller can safely recognize the type of interface desired.
 * This is the only area where timed waits are really needed as
 * the busy flag cannot be probed initially.
 */
void
hd44780_init(void)
{
  SET(DDR, HD44780_RS);
  SET(DDR, HD44780_RW);
  SET(DDR, HD44780_E);
  ASSIGN(DDR, HD44780_D4, 0x0F);

  _delay_ms(15);		/* 40 ms needed for Vcc = 2.7 V */
  hd44780_outnibble(HD44780_FNSET(1, 0, 0) >> 4, 0);
  _delay_ms(4.1);
  hd44780_outnibble(HD44780_FNSET(1, 0, 0) >> 4, 0);
  _delay_ms(0.1);
  hd44780_outnibble(HD44780_FNSET(1, 0, 0) >> 4, 0);
  _delay_us(40); /* 37 is too short */

  hd44780_outnibble(HD44780_FNSET(0, 1, 0) >> 4, 0);
  hd44780_wait_ready(false);
  hd44780_outcmd(HD44780_FNSET(0, 1, 0));
  hd44780_wait_ready(false);
  hd44780_outcmd(HD44780_DISPCTL(0, 0, 0));
  hd44780_wait_ready(false);
}

/*
 * Prepare the LCD controller pins for powerdown.
 */
void
hd44780_powerdown(void)
{
  ASSIGN(PORT, HD44780_D4, 0);
  CLR(PORT, HD44780_RS);
  CLR(PORT, HD44780_RW);
  CLR(PORT, HD44780_E);
}
