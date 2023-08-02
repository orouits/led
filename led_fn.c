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
                (PCRE2_UCHAR8*)led.curline.str,
                led.curline.len,
                0,
                PCRE2_SUBSTITUTE_EXTENDED|PCRE2_SUBSTITUTE_GLOBAL,
                NULL,
                NULL,
                (PCRE2_UCHAR8*)led.fn_arg[1].str,
                led.fn_arg[1].len,
                (PCRE2_UCHAR8*)led.buf_line_trans,
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

void led_fn_impl_range_sel() {
    size_t start = 0;
    size_t count = led.curline.len;

    if (led.fn_arg[0].len) {
        long val = led.fn_arg[0].val;
        size_t uval = led.fn_arg[0].uval;
        if (val > 0)
            start = uval > led.curline.len ? led.curline.len : uval;
        else
            start = uval > led.curline.len ? 0 : led.curline.len - uval;
    }
    if (led.fn_arg[1].len) {
        size_t uval = led.fn_arg[1].uval;
        count = uval + start > led.curline.len ? led.curline.len - start : uval;
    }
    else
        count = led.curline.len - start;

    memcpy(led.buf_line_trans, led.curline.str + start, count);
    led.buf_line_trans[count] = '\0';
    led.curline.len = count;
    led.curline.str = led.buf_line_trans;
}

void led_fn_impl_range_unsel() {
    size_t start = 0;
    size_t count = led.curline.len;

    if (led.fn_arg[0].len) {
        long val = led.fn_arg[0].val;
        size_t uval = led.fn_arg[0].uval;
        if (val > 0) {
            start = uval > led.curline.len ? led.curline.len : uval;
        }
        else {
            start = uval > led.curline.len ? 0 : led.curline.len - uval;
        }
    }
    if (led.fn_arg[1].len) {
        size_t uval = (size_t)led.fn_arg[1].val;
        count = uval + start > led.curline.len ? led.curline.len - start : uval;
    }
    else
        count = led.curline.len - start;

    memcpy(led.buf_line_trans, led.curline.str, start);
    memcpy(led.buf_line_trans + start, led.curline.str + start + count, led.curline.len - start - count);
    led.buf_line_trans[led.curline.len - count] = '\0';
    led.curline.len = led.curline.len - count;
    led.curline.str = led.buf_line_trans;
}

void led_fn_impl_translate() {
    size_t zone_start = 0;
    size_t zone_stop = led.curline.len;
    if (led.fn_arg[2].regex) led_regex_match_offset(led.fn_arg[2].regex,led.curline.str, led.curline.len, &zone_start, &zone_stop);

    for (size_t i=zone_start; i<zone_stop; i++) {
        char c = led.curline.str[i];
        led.buf_line_trans[i] = c;
        for (size_t j=0; j<led.fn_arg[0].len; j++) {
            if (led.fn_arg[0].str[j] == c) {
                if (j < led.fn_arg[1].len) led.buf_line_trans[i] = led.fn_arg[1].str[j];
                break;
            }
        }
    }
    led.buf_line_trans[led.curline.len] = '\0';
    led.curline.str = led.buf_line_trans;
}

void led_fn_impl_case_lower() {
    size_t zone_start = 0;
    size_t zone_stop = led.curline.len;
    if (led.fn_arg[0].regex) led_regex_match_offset(led.fn_arg[0].regex,led.curline.str, led.curline.len, &zone_start, &zone_stop);

    memcpy(led.buf_line_trans, led.curline.str, led.curline.len);
    for (size_t i=zone_start; i<zone_stop; i++) {
        led.buf_line_trans[i] = tolower(led.buf_line_trans[i]);
    }
    led.buf_line_trans[led.curline.len] = '\0';
    led.curline.str = led.buf_line_trans;
}

void led_fn_impl_case_upper() {
    size_t zone_start = 0;
    size_t zone_stop = led.curline.len;
    if (led.fn_arg[0].regex) led_regex_match_offset(led.fn_arg[0].regex,led.curline.str, led.curline.len, &zone_start, &zone_stop);

    memcpy(led.buf_line_trans, led.curline.str, led.curline.len);
    for (size_t i=zone_start; i<zone_stop; i++) {
        led.buf_line_trans[i] = toupper(led.buf_line_trans[i]);
    }
    led.buf_line_trans[led.curline.len] = '\0';
    led.curline.str = led.buf_line_trans;
}

void led_fn_impl_case_first() {
    size_t zone_start = 0;
    size_t zone_stop = led.curline.len;
    if (led.fn_arg[0].regex) led_regex_match_offset(led.fn_arg[0].regex,led.curline.str, led.curline.len, &zone_start, &zone_stop);

    memcpy(led.buf_line_trans, led.curline.str, led.curline.len);
    if (zone_start < led.curline.len)
        led.buf_line_trans[zone_start] = toupper(led.buf_line_trans[zone_start]);
    for (size_t i=zone_start+1; i<zone_stop; i++)
        led.buf_line_trans[i] = tolower(led.buf_line_trans[i]);
    led.buf_line_trans[led.curline.len] = '\0';
    led.curline.str = led.buf_line_trans;
}

void led_fn_impl_case_camel() {
    // buggy
    size_t zone_start = 0;
    size_t zone_stop = led.curline.len;
    if (led.fn_arg[0].regex) led_regex_match_offset(led.fn_arg[0].regex,led.curline.str, led.curline.len, &zone_start, &zone_stop);

    int wasword = FALSE;
    int j=0;
    for (size_t i=zone_start; i<zone_stop; i++) {
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
    size_t zone_start = 0;
    size_t zone_stop = led.curline.len;
    if (led.fn_arg[0].regex) led_regex_match_offset(led.fn_arg[0].regex,led.curline.str, led.curline.len, &zone_start, &zone_stop);

    if (! (led.curline.str[zone_start] == q && led.curline.str[zone_stop - 1] == q) ) {
        led_debug("quote active");
        led_assert(led.curline.len <= LED_LINE_MAX - 2, LED_ERR_MAXLINE, "Line too long to be quoted");
        memcpy(led.buf_line_trans, led.curline.str, zone_start);
        led.buf_line_trans[zone_start] = q;
        memcpy(led.buf_line_trans + zone_start + 1, led.curline.str + zone_start, zone_stop - zone_start);
        led.buf_line_trans[zone_stop + 1] = q;
        memcpy(led.buf_line_trans + zone_stop + 2, led.curline.str + zone_stop, led.curline.len - zone_stop);
        led.curline.len += 2;
        led.buf_line_trans[led.curline.len] = 0;
        led.curline.str = led.buf_line_trans;
    }
}

void led_fn_impl_quote_simple() { led_fn_impl_quote('\''); }
void led_fn_impl_quote_double() { led_fn_impl_quote('"'); }
void led_fn_impl_quote_back() { led_fn_impl_quote('`'); }

const char* QUOTES="'\"`";

void led_fn_impl_quote_remove() {
    size_t zone_start = 0;
    size_t zone_stop = led.curline.len;
    if (led.fn_arg[0].regex) led_regex_match_offset(led.fn_arg[0].regex,led.curline.str, led.curline.len, &zone_start, &zone_stop);

    char q = QUOTES[0];
    for(size_t i = 0; q != '\0'; i++, q = QUOTES[i]) {
        if (led.curline.str[zone_start] == q && led.curline.str[zone_stop - 1] == q) break;
    }

    if (q) {
        led_debug("quotes found: %c", q);
        memcpy(led.buf_line_trans, led.curline.str, zone_start);
        memcpy(led.buf_line_trans + zone_start, led.curline.str + zone_start + 1, zone_stop - zone_start - 2);
        memcpy(led.buf_line_trans + zone_stop - 2, led.curline.str + zone_stop, led.curline.len - zone_stop);
        led.curline.len -= 2;
        led.curline.str = led.buf_line_trans;
    }
}

led_fn_struct LED_FN_TABLE[] = {
    { "nn:", "none:", &led_fn_impl_none, "", "No processing", "none:" },
    { "sub:", "substitute:", &led_fn_impl_substitute, "RS", "Substitute", "substitute: <regex> <replace>" },
    { "exe:", "execute:", NULL, "RS", "Execute", "execute: <regex> <replace=command>" },
    { "rm:", "remove:", &led_fn_impl_remove, "r", "Remove line", "remove: [<regex>]" },
    { "rmb:", "remove_blank:", NULL, "", "Remove blank/empty lines", "remove_blank:" },
    { "ins:", "insert:", &led_fn_impl_insert, "Sp", "Insert line", "insert: <string> [N]" },
    { "app:", "append:", &led_fn_impl_append, "Sp", "Append line", "append: <string> [N]" },
    { "rns:", "range_sel:", &led_fn_impl_range_sel, "Np", "Range select", "range_sel: <start> [count]" },
    { "rnu:", "range_unsel:", &led_fn_impl_range_unsel, "Np", "Range unselect", "range_unsel: <start> [count]" },
    { "tr:", "translate:", &led_fn_impl_translate, "SS", "Translate", "translate: <chars> <chars>" },
    { "csl:", "case_lower:", &led_fn_impl_case_lower, "r", "Case to lower", "case_lower: [<regex>]" },
    { "csu:", "case_upper:", &led_fn_impl_case_upper, "r", "Case to upper", "case_upper: [<regex>]" },
    { "csf:", "case_first:", &led_fn_impl_case_first, "r", "Case first upper", "case_first: [<regex>]" },
    { "csc:", "case_camel:", &led_fn_impl_case_camel, "r", "Case to camel style", "case_camel: [<regex>]" },
    { "qts:", "quote_simple:", &led_fn_impl_quote_simple, "r", "Quote simple", "quote_simple: [<regex>]" },
    { "qtd:", "quote_double:", &led_fn_impl_quote_double, "r", "Quote double", "quote_double: [<regex>]" },
    { "qtb:", "quote_back:", &led_fn_impl_quote_back, "r", "Quote back", "quote_back: [<regex>]" },
    { "qtr:", "quote_remove:", &led_fn_impl_quote_remove, "r", "Quote remove", "quote_remove: [<regex>]" },
    { "tm:", "trim:", NULL, "r", "Trim", "trim: [<regex>]" },
    { "tml:", "trim_left:", NULL, "r", "Trim left", "trim_left: [<regex>]" },
    { "tmr:", "trim_right:", NULL, "r", "Trim right", "trim_right: [<regex>]" },
    { "sp:", "split:", NULL, "S", "Split", "split: <string>" },
    { "rv:", "revert:", NULL, "r", "Revert", "revert: [<regex>]" },
    { "fl:", "field:", NULL, "SP", "Extract fields", "field: <sep> <N>" },
    { "jn:", "join:", NULL, "", "Join lines", "join:" },
    { "ecrb64:", "encrypt_base64:", NULL, "s", "Encrypt base64", "encrypt_base64: [<regex>]" },
    { "dcrb64:", "decrypt_base64:", NULL, "s", "Decrypt base64", "decrypt_base64: [<regex>]" },
    { "urc:", "url_encode:", NULL, "s", "Encode URL", "url_encode: [<regex>]" },
    { "urd:", "url_decode:", NULL, "s", "Decode URL", "url_decode: [<regex>]" },
    { "phc:", "path_canonical:", NULL, "s", "Conert to canonical path", "path_canonical: [<regex>]" },
    { "phd:", "path_dir:", NULL, "s", "Extract last dir of the path", "path_dir: [<regex>]" },
    { "phf:", "path_file:", NULL, "s", "Extract file of the path", "path_file: [<regex>]" },
    { "phr:", "path_rename:", NULL, "s", "Rename file of the path without specific chars", "path_rename: [<regex>]" },
    { "rnn:", "randomize_num:", NULL, "s", "Randomize numeric values", "randomize_num: [<regex>]" },
    { "rna:", "randomize_alpha:", NULL, "s", "Randomize alpha values", "randomize_alpha: [<regex>]" },
    { "rnan:", "randomize_alphaum:", NULL, "s", "Randomize alpha numeric values", "randomize_alphaum: [<regex>]" },
};

#define LED_FN_TABLE_MAX sizeof(LED_FN_TABLE)/sizeof(led_fn_struct)

led_fn_struct* led_fn_table_descriptor(size_t fn_id) {
    led_assert(fn_id < LED_FN_TABLE_MAX, LED_ERR_INTERNAL, "Function index out of table");
    return LED_FN_TABLE + fn_id;
}

size_t led_fn_table_size() {
    return LED_FN_TABLE_MAX;
}