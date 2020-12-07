/*
 * log.c - replacement for esp-idf module of same name.
 * redirects log output to syslog functionality
 */

#include	"x_string_general.h"

#include "esp_attr.h"
#include "xtensa/hal.h"
#include "soc/soc.h"
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <ctype.h>

#include "esp_log.h"

#include "sys/queue.h"
#include "soc/soc_memory_layout.h"

#define	debugFLAG					(0x0000)

#define	debugSKIP					(debugFLAG & 0x0001)

#define	debugTIMING					(debugFLAG_GLOBAL & debugFLAG & 0x1000)
#define	debugTRACK					(debugFLAG_GLOBAL & debugFLAG & 0x2000)
#define	debugPARAM					(debugFLAG_GLOBAL & debugFLAG & 0x4000)
#define	debugRESULT					(debugFLAG_GLOBAL & debugFLAG & 0x8000)

// #################################### local/static variables #####################################

static esp_log_level_t s_log_default_level = ESP_LOG_WARN ;
static const char	esp_log_xlate[6] = { 0, 3, 4, 5, 6, 7 } ;

// ################################# forward function declarations #################################

int32_t	xvSyslog(uint32_t Priority, const char * MsgID, const char * format, va_list args) ;

// #################################### publi/global functions #####################################

void esp_log_level_set(const char* tag, esp_log_level_t level) { s_log_default_level = level ; }

void IRAM_ATTR esp_log_writev(esp_log_level_t level, const char* tag, const char * format, va_list args) {
	if (level <= s_log_default_level) {
		/* As of 20200323) wifi library changed format to 3 separate printf() calls
		 * #1 "format="%c (%d) %s" and prints just the level, tag & timestamp hence just discard
		 * #2 prints the actual message hence display as is
		 * #3 format="%s" and prints CR/LF pair hence discard */
		int32_t Len = xstrlen(format) ;
		// try to identify the format string structure and skip as required
#if		(debugTRACK)
		ets_printf("f='%s' => ", format) ;
#endif
		int32_t	Idx ;
		if (Len > 17 && format[17] == ':') {				// "0   123456789012345678"
			Idx = 18 ;										// "\033[0;3?m%c (%d) %s: "
		} else if (Len > 10 && format[10] == ':') {			// "01234567890"
			Idx = 11 ;										// "%c (%d) %s: "
		} else {
			Idx = 0 ;
		}
		// now remove some variables from stack if required...
		if (Idx) {
			format += Idx ;
			Len -= Idx ;
			void * pVoid ;
			pVoid = va_arg(args, void *) ;					// spill the tag
			pVoid = va_arg(args, void *) ;					// spill the timestamp
			(void) pVoid ;
		}

		if (Len == 0)										// nothing left to print
			return ;

		// Handle unexpected duplicate OR extra CRLF lines for each "wifi" event
		if (Len == 2 && strcmp(format, "%s") == 0 && strcmp(tag, "wifi") == 0) {
#if		(debugSKIP)
			ets_printf("Skipped duplicate (CRLF?) line\n") ;
#endif
			return ;
		}
		xvSyslog(esp_log_xlate[level], tag, format, args) ;
	} else {
#if		(debugSKIP)
		ets_printf("Skipped l=%d  t='%s'  f='%s'  p=0x%08X\n", level, tag, format, va_arg(args, void *)) ;
#endif
	}
}

void IRAM_ATTR esp_log_write(esp_log_level_t level, const char* tag, const char* format, ...) {
	va_list vArgs ;
	va_start(vArgs, format) ;
	esp_log_writev(level, tag, format, vArgs) ;
	va_end(vArgs) ;
}
