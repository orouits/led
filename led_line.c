#include "led.h"

//-----------------------------------------------
// LED line functions
//-----------------------------------------------

int led_line_defined(led_line_struct* pline) {
    return pline->str != NULL;
}

int led_line_len(led_line_struct* pline) {
    return pline->len;
}

size_t led_line_reset(led_line_struct* pline) {
    memset(pline, 0, sizeof *pline);
    return pline->len;
}
size_t led_line_init(led_line_struct* pline) {
    led_line_reset(pline);
    pline->str = pline->buf;
    return pline->len;
}
int led_line_select(led_line_struct* pline, int selected) {
    pline->selected = selected;
    return selected;
}
int led_line_selected(led_line_struct* pline) {
    return pline->selected;
}
int led_line_isblank(led_line_struct* pline) {
    return led_regex_match(LED_REGEX_BLANK_LINE, pline->str, pline->len) > 0;
}
int led_line_isempty(led_line_struct* pline) {
    return pline->len == 0;
}
int led_line_match(led_line_struct* pline, pcre2_code_8* regex) {
    pline->zone_start = pline->len;
    pline->zone_stop = pline->len;
    return led_regex_match_offset(regex, pline->str, pline->len, &pline->zone_start, &pline->zone_stop);
}
size_t led_line_copy(led_line_struct* pline, led_line_struct* pline_src) {
    memcpy(pline, pline_src, sizeof *pline_src);
    if (led_line_defined(pline_src)) pline->str = pline->buf;
    return pline->len;
}
size_t led_line_append(led_line_struct* pline, led_line_struct* pline_src) {
    led_line_append_str_len(pline, pline_src->str, pline_src->len);
    return pline->len;
}
size_t led_line_append_zone(led_line_struct* pline, led_line_struct* pline_src) {
    return led_line_append_str_start_stop(pline, pline_src->str, pline_src->zone_start, pline_src->zone_stop);
}
size_t led_line_append_before_zone(led_line_struct* pline, led_line_struct* pline_src) {
    return led_line_append_str_start_stop(pline, pline_src->str, 0, pline_src->zone_start);
}
size_t led_line_append_after_zone(led_line_struct* pline, led_line_struct* pline_src) {
    return led_line_append_str_start_stop(pline, pline_src->str, pline_src->zone_stop, pline_src->len);
}
size_t led_line_append_char(led_line_struct* pline, const char c) {
    pline->str = pline->buf;
    if (pline->len < LED_BUF_MAX-1) {
        pline->str[pline->len++] = c;
        pline->str[pline->len] = '\0';
    }
    return pline->len;
}
size_t led_line_append_str(led_line_struct* pline, const char* str) {
    pline->str = pline->buf;
    for (size_t i = 0; pline->len < LED_BUF_MAX-1 && str[i]; pline->len++, i++) pline->str[pline->len] = str[i];
    pline->str[pline->len] = '\0';
    return pline->len;
}
size_t led_line_append_str_len(led_line_struct* pline, const char* str, size_t len) {
    return led_line_append_str_start_len(pline, str, 0, len);
}
size_t led_line_append_str_start_len(led_line_struct* pline, const char* str, size_t start, size_t len) {
    pline->str = pline->buf;
    str += start;
    for (size_t i = 0; pline->len < LED_BUF_MAX-1 && i < len && str[i] ; pline->len++, i++) pline->str[pline->len] = str[i];
    pline->str[pline->len] = '\0';
    return pline->len;
}
size_t led_line_append_str_start_stop(led_line_struct* pline, const char* str, size_t start, size_t stop) {
    pline->str = pline->buf;
    for (size_t i = start; pline->len < LED_BUF_MAX-1 && str[i] && i < stop; pline->len++, i++) pline->str[pline->len] = str[i];
    pline->str[pline->len] = '\0';
    return pline->len;
}

size_t led_line_unappend_char(led_line_struct* pline, char c) {
    if (led_line_defined(pline) && pline->len > 0 && pline->buf[pline->len - 1] == c)
        pline->buf[--pline->len] = '\0';
    return pline->len;
}

size_t led_line_search_first(led_line_struct* pline, char c, size_t start, size_t stop) {
    size_t ichar = stop;
    for (size_t i = start; i < stop && ichar < stop; i++)
        if (pline->str[ichar] == c) ichar = i;
    return ichar;
}

size_t led_line_search_last(led_line_struct* pline, char c, size_t start, size_t stop) {
    size_t ichar = start;
    for (size_t i = start; i < stop; i++)
        if (pline->str[ichar] == c) ichar = i + 1;
    return ichar;
}

char led_line_last_char(led_line_struct* pline) {
    if (pline->len == 0) return '\0';
    return pline->str[pline->len - 1];
}
