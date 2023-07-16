#include "led.h"

//-----------------------------------------------
// LED functions
//-----------------------------------------------

void led_fn_impl_none() {
}

void led_fn_impl_substitute() {
    PCRE2_SIZE len = LED_LINE_MAX;
    int rc = pcre2_substitute(
                led.fn_arg[0].regex,
                led.curline.str,
                led.curline.len,
                0,
                PCRE2_SUBSTITUTE_EXTENDED|PCRE2_SUBSTITUTE_GLOBAL,
                NULL,
                NULL,
                led.fn_arg[1].str,
                led.fn_arg[1].len,
                led.buf_line_trans,
                &len);
    led_assert_pcre(rc);
    led.curline.str = led.buf_line_trans;
    led.curline.len = len;
}

void led_fn_impl_remove() {
    if (!led.fn_arg[0].regex || led_regex_match(led.fn_arg[0].regex, led.curline.str, led.curline.len)) {
        led.curline.str = NULL;
        led.curline.len = 0;
    }
}

void led_fn_impl_rangesel() {
    int start = 0;
    int count = led.curline.len;

    if (led.fn_arg[0].len) {
        start = led.fn_arg[0].val;
        if (start > led.curline.len) start = led.curline.len;
        else if (start < -led.curline.len) start = -led.curline.len;
        if (start < 0) start += led.curline.len;
    }
    if (led.fn_arg[1].len) {
        count = led.fn_arg[1].val;
        if (count < 0) count = 0;
    }
    if (count + start > led.curline.len) count = led.curline.len - start;

    if (led_str_equal(led.fn_arg[2].str, "n")) {
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

void led_fn_impl_translate() {
    // to be optimized, basic search currently, UTF8 not supported
    // with UTF8 it should be better to build an associative array or have a quicksearch.
    for (int i=0; i<led.curline.len; i++) {
        char c = led.curline.str[i];
        led.buf_line_trans[i] = c;
        for (int j=0; j<led.fn_arg[0].len; j++) {
            if (led.fn_arg[0].str[j] == c) {
                if (j < led.fn_arg[1].len) led.buf_line_trans[i] = led.fn_arg[1].str[j];
                break;
            }
        }
    }
    led.buf_line_trans[led.curline.len] = '\0';
    led.curline.str = led.buf_line_trans;
}

void led_fn_impl_caselower() {
    memcpy(led.buf_line_trans, led.curline.str, led.curline.len);
    for (int i=0; i<led.curline.len; i++) {
        led.buf_line_trans[i] = tolower(led.buf_line_trans[i]);
    }
    led.buf_line_trans[led.curline.len] = '\0';
    led.curline.str = led.buf_line_trans;
}

void led_fn_impl_caseupper() {
    memcpy(led.buf_line_trans, led.curline.str, led.curline.len);
    for (int i=0; i<led.curline.len; i++) {
        led.buf_line_trans[i] = toupper(led.buf_line_trans[i]);
    }
    led.buf_line_trans[led.curline.len] = '\0';
    led.curline.str = led.buf_line_trans;
}

void led_fn_impl_casefirst() {
    memcpy(led.buf_line_trans, led.curline.str, led.curline.len);
    led.buf_line_trans[0] = toupper(led.buf_line_trans[0]);
    for (int i=1; i<led.curline.len; i++) {
        led.buf_line_trans[i] = tolower(led.buf_line_trans[i]);
    }
    led.buf_line_trans[led.curline.len] = '\0';
    led.curline.str = led.buf_line_trans;
}

void led_fn_impl_casecamel() {
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
    led.curline.str = led.buf_line_trans;
}


void led_fn_impl_insert() {
    memcpy(led.buf_line_trans, led.fn_arg[0].str, led.fn_arg[0].len);
    led.buf_line_trans[led.fn_arg[0].len] = '\n';
    memcpy(led.buf_line_trans + led.fn_arg[0].len + 1, led.curline.str, led.curline.len);
    led.buf_line_trans[led.fn_arg[0].len + 1 + led.curline.len] = '\0';
    led.curline.str = led.buf_line_trans;
    led.curline.len += led.fn_arg[0].len + 1;
}

void led_fn_impl_append() {
    memcpy(led.buf_line_trans, led.curline.str, led.curline.len);
    led.buf_line_trans[led.curline.len] = '\n';
    memcpy(led.buf_line_trans + led.curline.len + 1, led.fn_arg[0].str, led.fn_arg[0].len);
    led.buf_line_trans[led.curline.len + 1 + led.fn_arg[0].len] = '\0';
    led.curline.str = led.buf_line_trans;
    led.curline.len += led.fn_arg[0].len + 1;
}

void led_fn_impl_quote(char q) {
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

void led_fn_impl_quotesimple() { led_fn_impl_quote('\''); }
void led_fn_impl_quotedouble() { led_fn_impl_quote('"'); }
void led_fn_impl_quoteback() { led_fn_impl_quote('`'); }