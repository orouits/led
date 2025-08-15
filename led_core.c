/***************************************************************************
 Copyright (C) 2024 - Olivier ROUITS <olivier.rouits@free.fr>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 USA
 ***************************************************************************/

#include "led.h"

//-----------------------------------------------
// LED object
//-----------------------------------------------

led_t led;

//-----------------------------------------------
// LED tech trace and error functions
//-----------------------------------------------

void led_free() {
    if (led.opt.file_in && led.file_in.file) {
        fclose(led.file_in.file);
        led.file_in.file = NULL;
        led_str_empty(&led.file_in.name);
    }
    if (led.opt.file_out && led.file_out.file) {
        fclose(led.file_out.file);
        led.file_out.file = NULL;
        led_str_empty(&led.file_out.name);
    }
    if (led.sel.regex_start != NULL) {
        pcre2_code_free(led.sel.regex_start);
        led.sel.regex_start = NULL;
    }
    if (led.sel.regex_stop != NULL) {
        pcre2_code_free(led.sel.regex_stop);
        led.sel.regex_stop = NULL;
    }
    led_foreach_pval(led.func_list) {
        // do not free STD regex here.
        if (foreach.pval->regex == LED_REGEX_ALL_LINE || foreach.pval->regex == LED_REGEX_ALL_PACKEDLINES) continue;
        if (foreach.pval->regex != NULL) {
            pcre2_code_free(foreach.pval->regex);
            foreach.pval->regex = NULL;
        }
    }
    led_regex_free();
}

void led_assert(bool cond, int code, const char* message, ...) {
    if (!cond) {
        if (message) {
            va_list args;
            va_start(args, message);
            vsnprintf((char*)led.buf_message, sizeof(led.buf_message), message, args);
            va_end(args);
            fprintf(stderr, "\e[31m[ERROR] %s\e[0m\n", led.buf_message);
        }
        led_free();
        exit(code);
    }
}

void led_assert_pcre(int rc) {
    if (rc < 0) {
        pcre2_get_error_message(rc, led.buf_message, LED_MSG_MAX);
        fprintf(stderr, "\e[31m[ERROR] (PCRE) %s\e[0m\n", led.buf_message);
        led_free();
        exit(LED_ERR_PCRE);
    }
}

void led_debug(const char* message, ...) {
    if (led.opt.verbose) {
        va_list args;
        va_start(args, message);
        vsnprintf((char*)led.buf_message, LED_MSG_MAX, message, args);
        va_end(args);
        fprintf(stderr, "\e[34m[DEBUG] %s\e[0m\n", led.buf_message);
    }
}

//-----------------------------------------------
// LED init functions
//-----------------------------------------------

pcre2_code* LED_REGEX_ALL_LINE;
pcre2_code* LED_REGEX_ALL_PACKEDLINES;
pcre2_code* LED_REGEX_BLANK_LINE;
pcre2_code* LED_REGEX_INTEGER;
pcre2_code* LED_REGEX_REGISTER;
pcre2_code* LED_REGEX_FUNC;
pcre2_code* LED_REGEX_FUNC2;

void led_regex_init() {
    LED_REGEX_ALL_LINE = led_regex_compile("^.*$",0);
    LED_REGEX_ALL_PACKEDLINES = led_regex_compile(".*", PCRE2_DOTALL);
    LED_REGEX_BLANK_LINE = led_regex_compile("^[ \t]*$",0);
    LED_REGEX_INTEGER = led_regex_compile("^[0-9]+$",0);
    LED_REGEX_REGISTER = led_regex_compile("\\$R[0-9]?",0);
    LED_REGEX_FUNC = led_regex_compile("^[a-z0-9_]+/",0);
    LED_REGEX_FUNC2 = led_regex_compile("^[a-z0-9_]+:",0);
}

void led_regex_free() {
    if (LED_REGEX_ALL_LINE != NULL) { pcre2_code_free(LED_REGEX_ALL_LINE); LED_REGEX_ALL_LINE = NULL; }
    if (LED_REGEX_ALL_PACKEDLINES != NULL) { pcre2_code_free(LED_REGEX_ALL_LINE); LED_REGEX_ALL_LINE = NULL; }
    if (LED_REGEX_BLANK_LINE != NULL) { pcre2_code_free(LED_REGEX_BLANK_LINE); LED_REGEX_BLANK_LINE = NULL; }
    if (LED_REGEX_INTEGER != NULL) { pcre2_code_free(LED_REGEX_INTEGER); LED_REGEX_INTEGER = NULL; }
    if (LED_REGEX_REGISTER != NULL) { pcre2_code_free(LED_REGEX_REGISTER); LED_REGEX_REGISTER = NULL; }
    if (LED_REGEX_FUNC != NULL) { pcre2_code_free(LED_REGEX_FUNC); LED_REGEX_FUNC = NULL; }
    if (LED_REGEX_FUNC2 != NULL) { pcre2_code_free(LED_REGEX_FUNC2); LED_REGEX_FUNC2 = NULL; }
}
