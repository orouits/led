#include "led.h"

//-----------------------------------------------
// LED str functions
//-----------------------------------------------

pcre2_code* LED_REGEX_ALL_LINE = NULL;
pcre2_code* LED_REGEX_BLANK_LINE = NULL;
pcre2_code* LED_REGEX_INTEGER = NULL;
pcre2_code* LED_REGEX_REGISTER = NULL;
pcre2_code* LED_REGEX_FUNC = NULL;
pcre2_code* LED_REGEX_FUNC2 = NULL;

void led_regex_init() {
    if (LED_REGEX_ALL_LINE == NULL) LED_REGEX_ALL_LINE = led_regex_compile("^.*$");
    if (LED_REGEX_BLANK_LINE == NULL) LED_REGEX_BLANK_LINE = led_regex_compile("^[ \t]*$");
    if (LED_REGEX_INTEGER == NULL) LED_REGEX_INTEGER = led_regex_compile("^[0-9]+$");
    if (LED_REGEX_REGISTER == NULL) LED_REGEX_REGISTER = led_regex_compile("\\$R[0-9]?");
    if (LED_REGEX_FUNC == NULL) LED_REGEX_FUNC = led_regex_compile("^[a-z0-9_]+/");
    if (LED_REGEX_FUNC2 == NULL) LED_REGEX_FUNC2 = led_regex_compile("^[a-z0-9_]+:");
}

void led_regex_free() {
    if (LED_REGEX_ALL_LINE != NULL) { pcre2_code_free(LED_REGEX_ALL_LINE); LED_REGEX_ALL_LINE = NULL; }
    if (LED_REGEX_BLANK_LINE != NULL) { pcre2_code_free(LED_REGEX_BLANK_LINE); LED_REGEX_BLANK_LINE = NULL; }
    if (LED_REGEX_INTEGER != NULL) { pcre2_code_free(LED_REGEX_INTEGER); LED_REGEX_INTEGER = NULL; }
    if (LED_REGEX_REGISTER != NULL) { pcre2_code_free(LED_REGEX_REGISTER); LED_REGEX_REGISTER = NULL; }
    if (LED_REGEX_FUNC != NULL) { pcre2_code_free(LED_REGEX_FUNC); LED_REGEX_FUNC = NULL; }
    if (LED_REGEX_FUNC2 != NULL) { pcre2_code_free(LED_REGEX_FUNC2); LED_REGEX_FUNC2 = NULL; }
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

int lstr_match(lstr* sval, pcre2_code* regex) {
    pcre2_match_data* match_data = pcre2_match_data_create_from_pattern(regex, NULL);
    int rc = pcre2_match(regex, (PCRE2_SPTR)sval->str, sval->len, 0, 0, match_data, NULL);
    pcre2_match_data_free(match_data);
    return rc > 0;
}

int lstr_match_offset(lstr* sval, pcre2_code* regex, size_t* pzone_start, size_t* pzone_stop) {
    pcre2_match_data* match_data = pcre2_match_data_create_from_pattern(regex, NULL);
    int rc = pcre2_match(regex, (PCRE2_SPTR)sval->str, sval->len, 0, 0, match_data, NULL);
    led_debug("match_offset %d ", rc);
    if( rc > 0) {
        PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(match_data);
        int iv = (rc - 1) * 2;
        *pzone_start = ovector[iv];
        *pzone_stop = ovector[iv + 1];
        led_debug("match_offset values %d (%c) - %d (%c)", *pzone_start, sval->str[*pzone_start], *pzone_stop, sval->str[*pzone_stop]);
    }
    pcre2_match_data_free(match_data);
    return rc > 0;
}
