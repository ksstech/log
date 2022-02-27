/*
 * log.c - replacement for esp-idf module of same name.
 * redirects log output to syslog functionality
 */

#include	"hal_config.h"
#include	"printfx.h"
#include	"x_string_general.h"

#include	"esp_attr.h"
#include	"xtensa/hal.h"
#include	"soc/soc.h"
#include	<stdbool.h>
#include	<stdarg.h>
#include	<string.h>
#include	<stdlib.h>
#include	<stdio.h>
#include	<assert.h>
#include	<ctype.h>

#include "esp_log.h"
#include "sys/queue.h"
#include "soc/soc_memory_layout.h"

#define	debugFLAG					(0xF000)

#define	debugFORMAT					(debugFLAG & 0x0001)
#define	debugSKIP					(debugFLAG & 0x0001)

#define	debugTIMING					(debugFLAG_GLOBAL & debugFLAG & 0x1000)
#define	debugTRACK					(debugFLAG_GLOBAL & debugFLAG & 0x2000)
#define	debugPARAM					(debugFLAG_GLOBAL & debugFLAG & 0x4000)
#define	debugRESULT					(debugFLAG_GLOBAL & debugFLAG & 0x8000)

// #################################### local/static variables #####################################

esp_log_level_t esp_log_default_level = CONFIG_LOG_DEFAULT_LEVEL;

// ################################# forward function declarations #################################

void xvSyslog(int Priority, const char * MsgID, const char * format, va_list args) ;

// #################################### publi/global functions #####################################

void esp_log_level_set(const char* tag, esp_log_level_t level) { esp_log_default_level = level; }

/* As of 20200323) wifi library changed format to 3 separate printf() calls
 * #1 "format="%c (%d) %s" and prints just the level, tag & timestamp hence just discard
 * #2 prints the actual message hence display as is
 * #3 format="%s" and prints CR/LF pair hence discard
 */
void IRAM_ATTR esp_log_writev(esp_log_level_t level, const char* tag, const char * format, va_list args) {
	if (format) {
		void * pV = strstr(format, "%c (%d) %s:");
		if (pV) {
			format = pV + (sizeof("%c (%d)") - 1);
			pV = va_arg(args, void *);					// spill tag (2nd copy)
			int xTS = va_arg(args, int);				// spill the timestamp
			(void) xTS;
		}
		if ((strcmp(tag, "wifi") == 0) &&				// resolve WIFI lib anomalies
		   ((strcmp(format, " %s:") == 0) ||			// remove extra " wifi:"
			(strcmp(format, "%s") == 0))) {				// remove extra CRLF
			return;
		}
	}
	xvSyslog(level+2, tag, format, args);
}

void IRAM_ATTR esp_log_write(esp_log_level_t level, const char* tag, const char* format, ...) {
	va_list args;
	va_start(args, format);
	esp_log_writev(level, tag, format, args);
	va_end(args);
}
