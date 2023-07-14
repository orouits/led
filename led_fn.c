#include "led.h"

//-----------------------------------------------
// LED functions
//-----------------------------------------------

void led_fn_none() {
}

void led_fn_substitute() {
    if (led.func_regex == NULL) {
        led.func_regex = led_regex_compile(led.func_arg[0].str);
        led_assert(led.func_arg[1].str != NULL, LED_ERR_ARG, "substitute: missing replace argument");
    }

    PCRE2_SIZE len = LED_LINE_MAX;
    int rc = pcre2_substitute(
                led.func_regex,
                led.curline.str,
                led.curline.len,
                0,
                PCRE2_SUBSTITUTE_EXTENDED|PCRE2_SUBSTITUTE_GLOBAL,
                NULL,
                NULL,
                led.func_arg[1].str,
                led.func_arg[1].len,
                led.buf_line_trans,
                &len);
    led_assert_pcre(rc);
    led.curline.str = led.buf_line_trans;
    led.curline.len = len;
}

void led_fn_remove() {
    led.curline.str = NULL;
    led.curline.len = 0;
}

void led_fn_range() {
    int start = 0;
    int count = led.curline.len;

    if (led.func_arg[0].len) {
        start = led.func_arg[0].val;
        if (start > led.curline.len) start = led.curline.len;
        else if (start < -led.curline.len) start = -led.curline.len;
        if (start < 0) start += led.curline.len;
    }
    if (led.func_arg[1].len) {
        count = led.func_arg[1].val;
        if (count < 0) count = 0;
    }
    if (count + start > led.curline.len) count = led.curline.len - start;

    if (led_str_equal(led.func_arg[2].str, "n")) {
        memcpy(led.buf_line_trans,led.curline.str, start);
        memcpy(led.buf_line_trans + start,led.curline.str + start + count, led.curline.len - start - count);
        led.buf_line_trans[led.curline.len - count] = '\0';
        led.curline.len = led.curline.len - count;
    }
    else {
        memcpy(led.buf_line_trans,led.curline.str + start, count);
        led.buf_line_trans[count] = '\0';
        led.curline.len = count;
    }
    led.curline.str = led.buf_line_trans;
}

void led_fn_translate() {
    // to be optimized, basic search currently, UTF8 not supported
    // with UTF8 it should be better to build an associative array or have a quicksearch.
    for (int i=0; i<led.curline.len; i++) {
        char c = led.curline.str[i];
        led.buf_line_trans[i] = c;
        for (int j=0; j<led.func_arg[0].len; j++) {
            if (led.func_arg[0].str[j] == c) {
                if (j < led.func_arg[1].len) led.buf_line_trans[i] = led.func_arg[1].str[j];
                break;
            }
        }
    }
    led.buf_line_trans[led.curline.len] = '\0';
    led.curline.str = led.buf_line_trans;
}

void led_fn_case() {
    if ( led.func_arg[0].len == 0 || led.func_arg[0].str[0] == 'u' ) {
        memcpy(led.buf_line_trans, led.curline.str, led.curline.len);
        for (int i=0; i<led.curline.len; i++) {
            led.buf_line_trans[i] = toupper(led.buf_line_trans[i]);
        }
        led.buf_line_trans[led.curline.len] = '\0';
    }
    else if ( led.func_arg[0].str[0] == 'l' ) {
        memcpy(led.buf_line_trans, led.curline.str, led.curline.len);
        for (int i=0; i<led.curline.len; i++) {
            led.buf_line_trans[i] = tolower(led.buf_line_trans[i]);
        }
        led.buf_line_trans[led.curline.len] = '\0';
    }
    else if ( led.func_arg[0].str[0] == 'f' ) {
        memcpy(led.buf_line_trans, led.curline.str, led.curline.len);
        led.buf_line_trans[0] = toupper(led.buf_line_trans[0]);
        for (int i=1; i<led.curline.len; i++) {
            led.buf_line_trans[i] = tolower(led.buf_line_trans[i]);
        }
        led.buf_line_trans[led.curline.len] = '\0';
    }
    else if ( led.func_arg[0].str[0] == 'c' ) {
        int wasword = FALSE;
        int j=0;
        for (int i=0; i<led.curline.len; i++) {
            int c = led.curline.str[i];
            int isword = isalnum(c) || c == '_';
            if (isword) {
                if (wasword) led.buf_line_trans[j++] = tolower(led.curline.str[i]);
                else led.buf_line_trans[j++] = toupper(led.curline.str[i]);
            }
            wasword = isword;
        }
        led.buf_line_trans[j] = '\0';
        led.curline.len = j;
    }
    led.curline.str = led.buf_line_trans;
}

void led_fn_insert() {
    memcpy(led.buf_line_trans, led.func_arg[0].str, led.func_arg[0].len);
    led.buf_line_trans[led.func_arg[0].len] = '\n';
    memcpy(led.buf_line_trans + led.func_arg[0].len + 1, led.curline.str, led.curline.len);
    led.buf_line_trans[led.func_arg[0].len + 1 + led.curline.len] = '\0';
    led.curline.str = led.buf_line_trans;
    led.curline.len += led.func_arg[0].len + 1;
}

void led_fn_append() {
    memcpy(led.buf_line_trans, led.curline.str, led.curline.len);
    led.buf_line_trans[led.curline.len] = '\n';
    memcpy(led.buf_line_trans + led.curline.len + 1, led.func_arg[0].str, led.func_arg[0].len);
    led.buf_line_trans[led.curline.len + 1 + led.func_arg[0].len] = '\0';
    led.curline.str = led.buf_line_trans;
    led.curline.len += led.func_arg[0].len + 1;
}

void led_fn_quote() {
    char q = '\'';
    if (led.func_arg[0].len > 0 && led.func_arg[0].str[0] == 'd') q = '"';    
    if (! (led.curline.str[0] == q && led.curline.str[led.curline.len - 1] == q) ) {
        led_debug("quote active");
        led_assert(led.curline.len <= LED_LINE_MAX - 2, LED_ERR_MAXLINE, "Line too long to be quoted");
        led.buf_line_trans[0] = q;
        memcpy(led.buf_line_trans + 1, led.curline.str, led.curline.len++);
        led.buf_line_trans[led.curline.len++] = q;
        led.buf_line_trans[led.curline.len++] = 0;
        led.curline.str = led.buf_line_trans;
    }
}

void led_fn_unquote() {
    char q = '\'';
    if (led.func_arg[0].len > 0 && led.func_arg[0].str[0] == 'd') q = '"';    
    if (! (led.curline.str[0] == q && led.curline.str[led.curline.len - 1] == q) ) {
        led_debug("quote active");
        led_assert(led.curline.len <= LED_LINE_MAX - 2, LED_ERR_MAXLINE, "Line too long to be quoted");
        led.buf_line_trans[0] = q;
        memcpy(led.buf_line_trans + 1, led.curline.str, led.curline.len++);
        led.buf_line_trans[led.curline.len++] = q;
        led.buf_line_trans[led.curline.len++] = 0;
        led.curline.str = led.buf_line_trans;
    }
}