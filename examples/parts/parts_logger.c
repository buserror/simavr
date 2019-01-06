#include <stdio.h>
#include "parts_logger.h"
#include "sim_avr.h"

/*
 * Logs a message using the current logger
 */
static void
parts_std_logger(
		parts_logger_t * logger,
		const int level,
		const char * format,
		va_list ap);

static parts_logger_p _parts_global_logger = parts_std_logger;

void
parts_global_logger(
		const parts_logger_t * logger,
		const int level,
		const char * format,
		... )
{
	va_list args;
	va_start(args, format);
	if (_parts_global_logger)
		_parts_global_logger((parts_logger_t*)logger, level, format, args);
	va_end(args);
}

void
parts_global_logger_set(
		parts_logger_p logger)
{
	_parts_global_logger = logger ? logger : parts_std_logger;
}

parts_logger_p
parts_global_logger_get(void)
{
	return _parts_global_logger;
}


static void
parts_std_logger(
		parts_logger_t * logger,
		const int level,
		const char * format,
		va_list ap)
{
	if (!logger || logger->level >= level) {

		vfprintf((level > LOG_ERROR) ?  stdout : stderr , format, ap);
	}
}


