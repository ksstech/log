/*
 * log.c - replacement for esp-idf module, redirects log output to syslog functionality
 * Copyright (c) 2017-25 Andre M. Maree / KSS Technologies (Pty) Ltd.
 */

#include "hal_platform.h"
#include "string_general.h"
#include "syslog.h"

#include <string.h>

#include "esp_attr.h"
#include <xtensa/hal.h>
#include "soc/soc.h"

#include "esp_log.h"
#include "esp_private/log_util.h"

#include "sys/queue.h"
#include "soc/soc_memory_layout.h"

// ########################################### Macros ##############################################

#define	debugFLAG					(0xF000)
#define	debugTIMING					(debugFLAG_GLOBAL & debugFLAG & 0x1000)
#define	debugTRACK					(debugFLAG_GLOBAL & debugFLAG & 0x2000)
#define	debugPARAM					(debugFLAG_GLOBAL & debugFLAG & 0x4000)
#define	debugRESULT					(debugFLAG_GLOBAL & debugFLAG & 0x8000)

// #################################### local/static variables #####################################

esp_log_cache_enabled_t esp_log_cache_enabled = NULL;

// ################################# forward function declarations #################################

void xvSyslog(int Priority, const char * MsgID, const char * format, va_list args);

// ################################### public/global functions #####################################


void esp_log_util_set_cache_enabled_cb(esp_log_cache_enabled_t func) { esp_log_cache_enabled = func; }

void esp_log_level_set(const char* tag, esp_log_level_t level) {
	/* IDF semantics: "*" = set the GLOBAL default, a named tag = per-tag override. This bridge has
	 * no per-tag machinery (deliberate - syslog's dual runtime thresholds plus compile-time
	 * LOG_LOCAL_LEVEL per component cover that need), so honour ONLY the wildcard and IGNORE named
	 * tags. The previous version applied EVERY call globally: esp_wifi's wifi_init.c calls this 4x
	 * with CONFIG_LOG_DEFAULT_LEVEL (2/WARN) at every boot, silently forcing ioSLOGhi to 4 -
	 * stomping the configured level and, because the console threshold is the MASTER gate in
	 * xvSyslog(), hiding every NOTICE/INFO line from console AND host, fleet-wide, since boot. */
	if (tag && tag[0] == '*' && tag[1] == 0)
		vSyslogSetConsoleLevel((level > 0) ? level + 2 : level);
}

esp_log_level_t esp_log_level_get(const char* tag) {
	/* No per-tag levels exist: every tag runs at the effective global (console) level, so
	 * returning it for any tag is the truthful answer under this design. */
	esp_log_level_t level = xSyslogGetConsoleLevel();
	return level ? level - 2 : level;		// convert back to esp_log_level_t
}

/* As of 20200323) wifi library changed format to 3 separate printf() calls
 * #1 "format="%c (%d) %s:" and prints just the level, tag & timestamp hence just discard
 * #2 prints the actual message hence display as is
 * #3 format="%s" and prints CR/LF pair hence discard
 * As of 2024xxyy (v5.4) a major rewrite of the log component happened.
 * AMM still to review and update
 */
/* NOT IRAM_ATTR - deliberately, and the reasoning matters because it looks like it should be.
 *
 * These override IDF's esp_log_write[v]. IDF does NOT mark its own versions IRAM_ATTR
 * (components/log/src/os/log_write.c), so IDF's contract is that they are task-context only.
 * Constrained-context logging in IDF goes via ESP_EARLY_LOGx / ESP_DRAM_LOGx, both of which expand
 * straight to esp_rom_printf (esp_log.h ESP_LOG_EARLY_IMPL) and never enter this function.
 *
 * That contract is what makes the strstr()/strcmp() below safe: format and tag are caller-supplied
 * .flash.rodata literals, so reading them requires the cache to be enabled. Our own ESP_LOGx uses
 * are all in cmd-proc/cmd-nvs.c, ie task context, so both halves of the contract hold.
 *
 * Marking these IRAM_ATTR advertised a cache-disabled capability they cannot deliver anyway - the
 * strstr() would fault before xvSyslog() ever got the chance to. If a future IDF starts calling
 * esp_log_write from a cache-disabled path, the fix is to bail out early here, NOT to re-add
 * IRAM_ATTR, because the whole chain below (xReport, xPrintFX, xStdioWrite, xUBufWrite) is in
 * flash. See analysis/uart-console-io-flow.md S50.2/S71. */
void esp_log_writev(esp_log_level_t level, const char* tag, const char * format, va_list args) {
	if (format) {
		void * pV = strstr(format, "%c (%d) %s:");
		if (pV) {
			format = pV + (sizeof("%c (%d)") - 1);
			pV = va_arg(args, void *);					// spill tag (2nd copy)
			int xTS = va_arg(args, int);				// spill the timestamp
			(void) xTS;
		}
		// resolve WIFI lib anomalies, extra " wifi:", extra CRLF
		if ((strcmp(tag, "wifi") == 0) && (strcmp(format, " %s:") == 0 || strcmp(format, "%s") == 0))
			return;
	}
	level += (level > 0) ? 2 : 0;
	xvSyslog(level, tag, format, args);
}

void esp_log_write(esp_log_level_t level, const char* tag, const char* format, ...) {
	va_list args;
	va_start(args, format);
	esp_log_writev(level, tag, format, args);
	va_end(args);
}
