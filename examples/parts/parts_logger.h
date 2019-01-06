#ifndef __PARTS_LOGGER_HEADER__
#define __PARTS_LOGGER_HEADER__

#include <stdarg.h>

typedef struct parts_logger_t {
	int level;
} parts_logger_t;

typedef void (*parts_logger_p)(parts_logger_t * logger, const int level, const char * format, va_list ap);

/*
 * Logs a message using the current logger
 */
void
parts_global_logger(
		const parts_logger_t * logger,
		const int level,
		const char * format,
		... );


/* Sets a global logging function in place of the default */
void
parts_global_logger_set(
		parts_logger_p logger);
/* Gets the current global logger function */
parts_logger_p
parts_global_logger_get(void);

#endif
