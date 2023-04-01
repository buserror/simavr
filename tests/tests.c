#include "tests.h"
#include "sim_avr.h"
#include "sim_elf.h"
#include "sim_hex.h"
#include "sim_core.h"
#include "avr_uart.h"
#include <stdio.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>

avr_cycle_count_t tests_cycle_count = 0;
int tests_disable_stdout = 1;

static char *test_name = "(uninitialized test)";
static int finished = 0;

#if defined(__GLIBC__) && !defined(__MINGW32__)
static FILE *orig_stderr = NULL;
#define restore_stderr()	{ if (orig_stderr) stderr = orig_stderr; }
#define map_stderr()		{ if (tests_disable_stdout && \
								  orig_stderr == NULL) {  \
								orig_stderr = stderr;	\
								fclose(stdout);			\
								stderr = stdout;		\
							} }
#else
#define restore_stderr()	{}
#define map_stderr()		{}
#endif

static void atexit_handler(void) {
	if (!finished)
		_fail(NULL, 0, "Test exit without indicating success.");
}

void tests_success(void) {
	restore_stderr();
	fprintf(stderr, "OK: %s\n", test_name);
	finished = 1;
}

void tests_init(int argc, char **argv) {
	test_name = strdup(argv[0]);
	atexit(atexit_handler);
}

static avr_cycle_count_t
cycle_timer_longjmp_cb(struct avr_t *avr, avr_cycle_count_t when, void *param) {
	jmp_buf *jmp = param;
	longjmp(*jmp, LJR_CYCLE_TIMER);
	return 0;	// clear warning
}

static jmp_buf *special_deinit_jmpbuf = NULL;

static void special_deinit_longjmp_cb(struct avr_t *avr, void *data) {
	if (special_deinit_jmpbuf)
		longjmp(*special_deinit_jmpbuf, LJR_SPECIAL_DEINIT);
}

static int my_avr_run(avr_t * avr)
{
	if (avr->state == cpu_Stopped)
		return avr->state;

	uint16_t new_pc = avr->pc;

	if (avr->state == cpu_Running)
		new_pc = avr_run_one(avr);

	// run the cycle timers, get the suggested sleep time
	// until the next timer is due
	avr_cycle_count_t sleep = avr_cycle_timer_process(avr);

	avr->pc = new_pc;

	if (avr->state == cpu_Sleeping) {
		if (!avr->sreg[S_I]) {
			printf("simavr: sleeping with interrupts off, quitting gracefully\n");
			avr_terminate(avr);
			fail("Test case error: special_deinit() returned?");
			exit(0);
		}
		/*
		 * try to sleep for as long as we can (?)
		 */
		// uint32_t usec = avr_cycles_to_usec(avr, sleep);
		// printf("sleep usec %d cycles %d\n", usec, sleep);
		// usleep(usec);
		avr->cycle += 1 + sleep;
	}
	// Interrupt servicing might change the PC too, during 'sleep'
	if (avr->state == cpu_Running || avr->state == cpu_Sleeping)
		avr_service_interrupts(avr);

	// if we were stepping, use this state to inform remote gdb

	return avr->state;
}

/* Set-up AVR from an initialised firmware struct.  Works with .hex files. */

static avr_t *tests_init_fw(elf_firmware_t *fwp, const char *filename) {
	tests_cycle_count = 0;
	map_stderr();

	sim_setup_firmware(filename, 0, fwp, test_name);
	avr_t *avr = avr_make_mcu_by_name(fwp->mmcu);
	if (!avr)
		fail("Creating AVR failed.");
	avr_init(avr);
	avr_load_firmware(avr, fwp);
	return avr;
}

avr_t *tests_init_avr(const char *elfname) {
	elf_firmware_t fw = {};

	map_stderr();
	return tests_init_fw(&fw, elfname);
}

int tests_run_test(avr_t *avr, unsigned long run_usec) {
	if (!avr)
		fail("Internal test error: avr == NULL in run_test()");
	// register a cycle timer to fire after 100 seconds (simulation time);
	// assert that the simulation has not finished before that.
	jmp_buf jmp;
	special_deinit_jmpbuf = &jmp;
	avr->custom.deinit = special_deinit_longjmp_cb;
	avr_cycle_timer_register_usec(avr, run_usec,
				      cycle_timer_longjmp_cb, &jmp);
	int reason = setjmp(jmp);
	tests_cycle_count = avr->cycle;
	if (reason == 0) {
		// setjmp() returned directly, run avr
		while (1)
			my_avr_run(avr);
	} else if (reason == 1) {
		// returned from longjmp(); cycle timer fired
		return reason;
	} else if (reason == 2) {
		// returned from special deinit, avr stopped
		return reason;
	}
	fail("Error in test case: Should never reach this.");
	return 0;
}

int tests_init_and_run_test(const char *elfname, unsigned long run_usec) {
	avr_t *avr = tests_init_avr(elfname);
	return tests_run_test(avr, run_usec);
}

struct output_buffer {
	char *str;
	int currlen;
	int alloclen;
	int maxlen;
};

/* Callback for receiving data via an IRQ. */

static void buf_output_cb(struct avr_irq_t *irq, uint32_t value, void *param) {
	struct output_buffer *buf = param;
	if (!buf)
		fail("Internal error: buf == NULL in buf_output_cb()");
	if (buf->currlen > buf->alloclen-1)
		fail("Internal error");
	if (buf->alloclen == 0)
		fail("Internal error");
	if (buf->currlen == buf->alloclen-1) {
		buf->alloclen *= 2;
		buf->str = realloc(buf->str, buf->alloclen);
	}
	buf->str[buf->currlen++] = value;
	buf->str[buf->currlen] = 0;
}

/* Callback for receiving data directly from a register,
 * after calling avr_register_io_write().
 */

static void reg_output_cb(struct avr_t *avr, avr_io_addr_t addr,
                          uint8_t v, void *param)
{
    buf_output_cb(NULL, v, param);
}

static void init_output_buffer(struct output_buffer *buf) {
	buf->str = malloc(128);
	buf->str[0] = 0;
	buf->currlen = 0;
	buf->alloclen = 128;
	buf->maxlen = 4096;
}

static void tests_assert_xxxx_receive_avr(avr_t                *avr,
                                          unsigned long         run_usec,
                                          struct output_buffer *buf,
                                          const char           *expected)
{
	enum tests_finish_reason reason = tests_run_test(avr, run_usec);

	if (reason == LJR_CYCLE_TIMER) {
		if (strcmp(buf->str, expected) == 0) {
			_fail(NULL, 0, "Simulation did not finish within %lu simulated usec. "
			     "Output is correct and complete.", run_usec);
		}
		_fail(NULL, 0, "Simulation did not finish within %lu simulated usec. "
		     "Output so far: \"%s\"", run_usec, buf->str);
	}
	if (strcmp(buf->str, expected) != 0)
		_fail(NULL, 0, "Outputs differ: expected \"%s\", got \"%s\"", expected, buf->str);
}

void tests_assert_uart_receive_fw(elf_firmware_t *fw,
								  const char *firmware,
								  unsigned long run_usec,
								  const char *expected,
								  char uart) {
	avr_t *avr = tests_init_fw(fw, firmware);

	tests_assert_uart_receive_avr(avr, run_usec, expected, uart);
}

void tests_assert_uart_receive_avr(avr_t *avr,
			       unsigned long run_usec,
			       const char *expected,
			       char uart) {
	struct output_buffer buf;
	init_output_buffer(&buf);

	avr_irq_register_notify(
							avr_io_getirq(avr, AVR_IOCTL_UART_GETIRQ(uart),
                              UART_IRQ_OUTPUT), buf_output_cb, &buf);
	tests_assert_xxxx_receive_avr(avr, run_usec, &buf, expected);
}

void tests_assert_uart_receive(const char *elfname,
			       unsigned long run_usec,
			       const char *expected,
			       char uart) {
	avr_t *avr = tests_init_avr(elfname);

	tests_assert_uart_receive_avr(avr, run_usec, expected, uart);
}

void tests_assert_register_receive_avr(avr_t         *avr,
                                       unsigned long  run_usec,
                                       const char    *expected,
                                       avr_io_addr_t  reg_addr)
{
	struct output_buffer buf;

	init_output_buffer(&buf);
        avr_register_io_write(avr, reg_addr, reg_output_cb, &buf);
	tests_assert_xxxx_receive_avr(avr, run_usec, &buf, expected);
}

void tests_assert_register_receive(const char    *elfname,
                                   unsigned long  run_usec,
                                   const char    *expected,
                                   avr_io_addr_t  reg_addr)
{
	avr_t *avr = tests_init_avr(elfname);

        tests_assert_register_receive_avr(avr, run_usec, expected, reg_addr);
}

void tests_assert_cycles_at_least(unsigned long n) {
	if (tests_cycle_count < n)
		_fail(NULL, 0, "Program ran for too few cycles (%"
		      PRI_avr_cycle_count " < %lu)", tests_cycle_count, n);
}

void tests_assert_cycles_at_most(unsigned long n) {
	if (tests_cycle_count > n)
		_fail(NULL, 0, "Program ran for too many cycles (%"
		      PRI_avr_cycle_count " > %lu)", tests_cycle_count, n);
}

void tests_assert_cycles_between(unsigned long min, unsigned long max) {
	tests_assert_cycles_at_least(min);
	tests_assert_cycles_at_most(max);
}

void _fail(const char *filename, int linenum, const char *fmt, ...) {
	restore_stderr();

	if (filename)
		fprintf(stderr, "%s:%d: ", filename, linenum);

	fprintf(stderr, "Test ");
	if (test_name)
		fprintf(stderr, "%s ", test_name);
	fprintf(stderr, "FAILED.\n");

	if (filename)
		fprintf(stderr, "%s:%d: ", filename, linenum);

	va_list va;
	va_start(va, fmt);
	vfprintf(stderr, fmt, va);
	putc('\n', stderr);

	finished = 1;
	_exit(1);
}
