#include "led.h"

//-----------------------------------------------
// LED str functions
//-----------------------------------------------

int led_str_trim(char* line) {
    int len = strlen(line);
    int last = len - 1;
    while (last >= 0 && isspace(led.curfile.name[last])) {
        last--;
    }
    len = last +1;
    line[len] = '\0';
    return len;
}

int led_str_equal(const char* str1, const char* str2) {
    return str1 && str2 && strcmp(str1, str2) == 0;
}

int led_str_equal_len(const char* str1, const char* str2, int len) {
    return str1 && str2 && strncmp(str1, str2, len) == 0;
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

int led_regex_match(pcre2_code* regex, const char* line, int len) {
    pcre2_match_data* match_data = pcre2_match_data_create_from_pattern(regex, NULL);
    int rc = pcre2_match(regex, (PCRE2_SPTR)line, len, 0, 0, match_data, NULL);
    pcre2_match_data_free(match_data);
    return rc > 0;
}

int led_regex_match_offset(pcre2_code* regex, const char* line, int len, size_t* pzone_start, size_t* pzone_stop) {
    pcre2_match_data* match_data = pcre2_match_data_create_from_pattern(regex, NULL);
    int rc = pcre2_match(regex, (PCRE2_SPTR)line, len, 0, 0, match_data, NULL);
    led_debug("match_offset %d ", rc);
    if( rc > 0) {
        PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(match_data);
        int iv = (rc - 1) * 2;
        *pzone_start = ovector[iv];
        *pzone_stop = ovector[iv + 1];
        led_debug("match_offset values %d %d", *pzone_start, *pzone_stop);
    }
    else {
        *pzone_start = *pzone_stop;
    }
    pcre2_match_data_free(match_data);
    return rc > 0;
}

int led_str_match(const char* str, const char* regex) {
    return led_regex_match(led_regex_compile(regex), str, strlen(str));
}
