#include "led.h"

//-----------------------------------------------
// LED str functions
//-----------------------------------------------

char* led_str_empty(char* str) {
    str[0] = '\0';
    return str;
}

char* led_str_app(char* dest, const char* src, int maxlen) {
    int len = strlen(dest);
    return strncat(dest, src, maxlen - len);
}

char* led_str_cpy(char* dest, const char* src, int maxlen) {
    int len = strlen(dest);
    return strncpy(dest, src, maxlen - len);
}

char* led_str_trunc(char* dest, int size) {
    int len = strlen(dest);
    if (size >= 0) {
        if (size > len) size = len;
    }
    else {
        if (len + size > 0) size = len + size;
        else size = 0;
    }
    dest[size] = '\0';
    return dest;
}

char* led_str_trim(char* str) {
    size_t len = strlen(str);
    while (len > 0 && isspace(str[len-1])) len--;
    str[len] = '\0';
    return str;
}

int led_str_isempty(char* str) {
    return str[0] == '\0';
}

int led_str_equal(const char* str1, const char* str2) {
    return str1 && str2 && strcmp(str1, str2) == 0;
}

int led_str_equal_len(const char* str1, const char* str2, int len) {
    return str1 && str2 && strncmp(str1, str2, len) == 0;
}

int led_char_in_str(char c, const char* str) {
    return led_char_pos_str(c, str) >= 0;
}

int led_char_pos_str(char c, const char* str) {
    int pos = -1;
    for (size_t i = 0; str[i] != '\0' && pos < 0; i++)
        if (str[i] == c ) pos = i;
    return pos;
}

char* led_str_cut(char* str, char c) {
    char* next = str;
    while (*next != '\0') {
        if (*next == c ) { 
            *next = '\0'; 
            return next + 1; 
        }
        next++;
    }
    return NULL;
}

pcre2_code* LED_REGEX_BLANK_LINE = NULL;
pcre2_code* LED_REGEX_REGISTER = NULL;

void led_regex_init() {
    if (LED_REGEX_BLANK_LINE == NULL) LED_REGEX_BLANK_LINE = led_regex_compile("^\\s*$");
    if (LED_REGEX_REGISTER == NULL) LED_REGEX_REGISTER = led_regex_compile("\\$R");
}

void led_regex_free() {
    if (LED_REGEX_BLANK_LINE != NULL) { pcre2_code_free(LED_REGEX_BLANK_LINE); LED_REGEX_BLANK_LINE = NULL; }
    if (LED_REGEX_REGISTER != NULL) { pcre2_code_free(LED_REGEX_REGISTER); LED_REGEX_REGISTER = NULL; }
}

pcre2_code* led_regex_compile(const char* pattern) {
    int pcre_err;
    PCRE2_SIZE pcre_erroff;
    PCRE2_UCHAR pcre_errbuf[256];
    led_assert(pattern != NULL, LED_ERR_ARG, "Missing regex");
    pcre2_code* regex = pcre2_compile((PCRE2_SPTR)pattern, PCRE2_ZERO_TERMINATED, 0, &pcre_err, &pcre_erroff, NULL);
    pcre2_get_error_message(pcre_err, pcre_errbuf, sizeof(pcre_errbuf));
    led_assert(regex != NULL, LED_ERR_PCRE, "Regex error \"%s\" offset %d: %s", pattern, pcre_erroff, pcre_errbuf);
    return regex;
}

int led_regex_match(pcre2_code* regex, const char* str, int len) {
    pcre2_match_data* match_data = pcre2_match_data_create_from_pattern(regex, NULL);
    int rc = pcre2_match(regex, (PCRE2_SPTR)str, len, 0, 0, match_data, NULL);
    pcre2_match_data_free(match_data);
    return rc > 0;
}

int led_regex_match_offset(pcre2_code* regex, const char* str, int len, size_t* pzone_start, size_t* pzone_stop) {
    pcre2_match_data* match_data = pcre2_match_data_create_from_pattern(regex, NULL);
    int rc = pcre2_match(regex, (PCRE2_SPTR)str, len, 0, 0, match_data, NULL);
    led_debug("match_offset %d ", rc);
    if( rc > 0) {
        PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(match_data);
        int iv = (rc - 1) * 2;
        *pzone_start = ovector[iv];
        *pzone_stop = ovector[iv + 1];
        led_debug("match_offset values %d (%c) - %d (%c)", *pzone_start, str[*pzone_start], *pzone_stop, str[*pzone_stop]);
    }
    pcre2_match_data_free(match_data);
    return rc > 0;
}

int led_str_match(const char* str_regex, const char* str) {
    return led_regex_match(led_regex_compile(str_regex), str, strlen(str));
}
