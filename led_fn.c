#include "led.h"
#include <b64/cencode.h>
#include <b64/cdecode.h>

//-----------------------------------------------
// LED functions utilities
//-----------------------------------------------

#define countof(a) (sizeof(a)/sizeof(a[0]))

//-----------------------------------------------
// LED functions
//-----------------------------------------------

void led_fn_impl_none() {
    led_line_append_str_len(led.line_src.str, led.line_src.len);
}

void led_fn_impl_substitute() {
    PCRE2_SIZE len = LED_LINE_MAX;
    int rc = pcre2_substitute(
                led.fn_arg[0].regex,
                (PCRE2_UCHAR8*)led.line_src.str,
                led.line_src.len,
                0,
                PCRE2_SUBSTITUTE_EXTENDED|PCRE2_SUBSTITUTE_GLOBAL,
                NULL,
                NULL,
                (PCRE2_UCHAR8*)led.fn_arg[1].str,
                led.fn_arg[1].len,
                (PCRE2_UCHAR8*)led.line_dst.buf,
                &len);
    led_assert_pcre(rc);
    led.line_dst.str = led.line_dst.buf;
    led.line_dst.len = len;
}

void led_fn_impl_remove() {
    if (!led.fn_arg[0].regex || led_regex_match(led.fn_arg[0].regex, led.line_src.str, led.line_src.len))
        led_line_reset();
    else
        led_line_copy();
}

void led_fn_impl_range_sel() {
    size_t zone_start = 0;
    size_t zone_stop = led.line_src.len;

    if (led.fn_arg[0].len) {
        long val = led.fn_arg[0].val;
        size_t uval = led.fn_arg[0].uval;
        if (val > 0)
            zone_start = uval > led.line_src.len ? led.line_src.len : uval;
        else
            zone_start = uval > led.line_src.len ? 0 : led.line_src.len - uval;
    }
    if (led.fn_arg[1].len) {
        size_t uval = led.fn_arg[1].uval;
        zone_stop = zone_start + uval > led.line_src.len ? led.line_src.len : zone_start + uval;
    }
    else
        zone_stop = led.line_src.len;

    led_line_append_str_start_stop(led.line_src.str, zone_start, zone_stop);
}

void led_fn_impl_range_unsel() {
    size_t zone_start = 0;
    size_t zone_stop = led.line_src.len;

    if (led.fn_arg[0].len) {
        long val = led.fn_arg[0].val;
        size_t uval = led.fn_arg[0].uval;
        if (val > 0) {
            zone_start = uval > led.line_src.len ? led.line_src.len : uval;
        }
        else {
            zone_start = uval > led.line_src.len ? 0 : led.line_src.len - uval;
        }
    }
    if (led.fn_arg[1].len) {
        size_t uval = (size_t)led.fn_arg[1].val;
        zone_stop = zone_start + uval > led.line_src.len ? led.line_src.len : zone_start + uval;
    }
    else
        zone_stop = led.line_src.len;

    led_line_append_str_start_stop(led.line_src.str, 0, zone_start);
    led_line_append_str_start_stop(led.line_src.str, zone_stop, led.line_src.len);
}

void led_fn_impl_translate() {
    size_t zone_start = 0;
    size_t zone_stop = led.line_src.len;
    if (led.fn_arg[2].regex) led_regex_match_offset(led.fn_arg[2].regex,led.line_src.str, led.line_src.len, &zone_start, &zone_stop);

    led_line_copy();
    for (size_t i=zone_start; i<zone_stop; i++) {
        char c = led.line_src.str[i];
        for (size_t j=0; j<led.fn_arg[0].len; j++) {
            if (led.fn_arg[0].str[j] == c) {
                if (j < led.fn_arg[1].len)
                    led.line_dst.str[i] = led.fn_arg[1].str[j];
                break;
            }
        }
    }
}

void led_fn_impl_case_lower() {
    size_t zone_start = 0;
    size_t zone_stop = led.line_src.len;
    if (led.fn_arg[0].regex) led_regex_match_offset(led.fn_arg[0].regex,led.line_src.str, led.line_src.len, &zone_start, &zone_stop);

    led_line_append_str_start_stop(led.line_src.str, 0, zone_start);
    for (size_t i=zone_start; i<zone_stop; i++)
        led_line_append_char(tolower(led.line_dst.str[i]));
    led_line_append_str_start_stop(led.line_src.str, zone_stop, led.line_src.len);

}

void led_fn_impl_case_upper() {
    size_t zone_start = 0;
    size_t zone_stop = led.line_src.len;
    if (led.fn_arg[0].regex) led_regex_match_offset(led.fn_arg[0].regex,led.line_src.str, led.line_src.len, &zone_start, &zone_stop);

    led_line_append_str_start_stop(led.line_src.str, 0, zone_start);
    for (size_t i=zone_start; i<zone_stop; i++)
        led_line_append_char(toupper(led.line_dst.str[i]));
    led_line_append_str_start_stop(led.line_src.str, zone_stop, led.line_src.len);
}

void led_fn_impl_case_first() {
    size_t zone_start = 0;
    size_t zone_stop = led.line_src.len;
    if (led.fn_arg[0].regex) led_regex_match_offset(led.fn_arg[0].regex,led.line_src.str, led.line_src.len, &zone_start, &zone_stop);

    led_line_append_str_start_stop(led.line_src.str, 0, zone_start);
    if (zone_start < led.line_dst.len)
        led_line_append_char(toupper(led.line_dst.str[zone_start]));
    for (size_t i=zone_start+1; i<zone_stop; i++)
        led_line_append_char(tolower(led.line_dst.str[i]));
    led_line_append_str_start_stop(led.line_src.str, zone_stop, led.line_src.len);
}

void led_fn_impl_case_camel() {
    // buggy
    size_t zone_start = 0;
    size_t zone_stop = led.line_src.len;
    if (led.fn_arg[0].regex) led_regex_match_offset(led.fn_arg[0].regex,led.line_src.str, led.line_src.len, &zone_start, &zone_stop);

    led_line_append_str_start_stop(led.line_src.str, 0, zone_start);
    int wasword = FALSE;
    for (size_t i=zone_start; i<zone_stop; i++) {
        int c = led.line_src.str[i];
        int isword = isalnum(c) || c == '_';
        if (isword) {
            if (wasword) led_line_append_char(tolower(led.line_src.str[i]));
            else led_line_append_char(toupper(led.line_src.str[i]));
        }
        wasword = isword;
    }
    led_line_append_str_start_stop(led.line_src.str, zone_stop, led.line_src.len);
}


void led_fn_impl_insert() {
    led_line_append_str_len(led.fn_arg[0].str, led.fn_arg[0].len);
    led_line_append_char('\n');
    led_line_append_str_len(led.line_src.str, led.line_src.len);
}

void led_fn_impl_append() {
    led_line_append_str_len(led.line_src.str, led.line_src.len);
    led_line_append_char('\n');
    led_line_append_str_len(led.fn_arg[0].str, led.fn_arg[0].len);
}

void led_fn_impl_quote(char q) {
    size_t zone_start = 0;
    size_t zone_stop = led.line_src.len;
    if (led.fn_arg[0].regex) led_regex_match_offset(led.fn_arg[0].regex,led.line_src.str, led.line_src.len, &zone_start, &zone_stop);

    if (! (led.line_src.str[zone_start] == q && led.line_src.str[zone_stop - 1] == q) ) {
        led_debug("quote active");
        led_assert(led.line_src.len <= LED_LINE_MAX - 2, LED_ERR_MAXLINE, "Line too long to be quoted");
        led_line_append_str_start_stop(led.line_src.str, 0, zone_start);
        led_line_append_char(q);
        led_line_append_str_start_stop(led.line_src.str, zone_start, zone_stop);
        led_line_append_char(q);
        led_line_append_str_start_stop(led.line_src.str, zone_stop, led.line_src.len);
    }
    else
        led_line_copy();
}

void led_fn_impl_quote_simple() { led_fn_impl_quote('\''); }
void led_fn_impl_quote_double() { led_fn_impl_quote('"'); }
void led_fn_impl_quote_back() { led_fn_impl_quote('`'); }

const char* QUOTES="'\"`";

void led_fn_impl_quote_remove() {
    size_t zone_start = 0;
    size_t zone_stop = led.line_src.len;
    if (led.fn_arg[0].regex) led_regex_match_offset(led.fn_arg[0].regex,led.line_src.str, led.line_src.len, &zone_start, &zone_stop);

    char q = QUOTES[0];
    for(size_t i = 0; q != '\0'; i++, q = QUOTES[i]) {
        if (led.line_src.str[zone_start] == q && led.line_src.str[zone_stop - 1] == q) break;
    }

    if (q) {
        led_debug("quotes found: %c", q);
        led_line_append_str_start_stop(led.line_src.str, 0, zone_start);
        led_line_append_str_start_stop(led.line_src.str, zone_start+1, zone_stop-1);
        led_line_append_str_start_stop(led.line_src.str, zone_stop, led.line_src.len);
    }
    else
        led_line_copy();

}

void led_fn_impl_trim() {
    size_t zone_start = 0;
    size_t zone_stop = led.line_src.len;
    if (led.fn_arg[0].regex) led_regex_match_offset(led.fn_arg[0].regex,led.line_src.str, led.line_src.len, &zone_start, &zone_stop);

    size_t str_start = zone_start;
    size_t str_stop = zone_stop;
    for (; str_start < zone_stop; str_start++) {
        if (!isspace(led.line_src.str[str_start])) break;
    }
    for (; str_stop > str_start; str_stop--) {
        if (!isspace(led.line_src.str[str_stop - 1])) break;
    }
    led_line_append_str_start_stop(led.line_src.str, 0, zone_start);
    led_line_append_str_start_stop(led.line_src.str, str_start, str_stop);
    led_line_append_str_start_stop(led.line_src.str, zone_stop, led.line_src.len);
}

void led_fn_impl_trim_left() {
    size_t zone_start = 0;
    size_t zone_stop = led.line_src.len;
    if (led.fn_arg[0].regex) led_regex_match_offset(led.fn_arg[0].regex,led.line_src.str, led.line_src.len, &zone_start, &zone_stop);

    size_t str_start = zone_start;
    for (; str_start < zone_stop; str_start++) {
        if (!isspace(led.line_src.str[str_start])) break;
    }
    led_line_append_str_start_stop(led.line_src.str, 0, zone_start);
    led_line_append_str_start_stop(led.line_src.str, str_start, led.line_src.len);
}

void led_fn_impl_trim_right() {
    size_t zone_start = 0;
    size_t zone_stop = led.line_src.len;
    if (led.fn_arg[0].regex) led_regex_match_offset(led.fn_arg[0].regex,led.line_src.str, led.line_src.len, &zone_start, &zone_stop);

    size_t str_stop = zone_stop;
    for (; str_stop > zone_start; str_stop--)
        if (!isspace(led.line_src.str[str_stop - 1])) break;
    led_line_append_str_start_stop(led.line_src.str, 0, str_stop);
    led_line_append_str_start_stop(led.line_src.str, zone_stop, led.line_src.len);
}


void led_fn_impl_encrypt_base64() {
    base64_encodestate base64_state;
	size_t count = 0;

	base64_init_encodestate(&base64_state);
	count = base64_encode_block(led.line_src.str, led.line_src.len, led.line_dst.buf, &base64_state);
	count += base64_encode_blockend(led.line_dst.buf + count, &base64_state);
	led.line_dst.str = led.line_dst.buf;
    led.line_dst.len = count;
    led.line_dst.str[led.line_dst.len] = '\0';
}

void led_fn_impl_decrypt_base64() {
	base64_decodestate base64_state;
	size_t count = 0;

	base64_init_decodestate(&base64_state);
	count = base64_decode_block(led.line_src.str, led.line_src.len, led.line_dst.buf, &base64_state);
	led.line_dst.str = led.line_dst.buf;
    led.line_dst.len = count;
    led.line_dst.str[led.line_dst.len] = '\0';
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
    { "tm:", "trim:", &led_fn_impl_trim, "r", "Trim", "trim: [<regex>]" },
    { "tml:", "trim_left:", &led_fn_impl_trim_left, "r", "Trim left", "trim_left: [<regex>]" },
    { "tmr:", "trim_right:", &led_fn_impl_trim_right, "r", "Trim right", "trim_right: [<regex>]" },
    { "sp:", "split:", NULL, "s", "Split", "split: [chars]" },
    { "rv:", "revert:", NULL, "r", "Revert", "revert: [<regex>]" },
    { "fl:", "field:", NULL, "sp", "Extract fields", "field: [<sep>] [<N>]" },
    { "jn:", "join:", NULL, "", "Join lines", "join:" },
    { "ecrb64:", "encrypt_base64:", &led_fn_impl_encrypt_base64, "r", "Encrypt base64", "encrypt_base64: [<regex>]" },
    { "dcrb64:", "decrypt_base64:", &led_fn_impl_decrypt_base64, "r", "Decrypt base64", "decrypt_base64: [<regex>]" },
    { "urc:", "url_encode:", NULL, "r", "Encode URL", "url_encode: [<regex>]" },
    { "urd:", "url_decode:", NULL, "r", "Decode URL", "url_decode: [<regex>]" },
    { "phc:", "path_canonical:", NULL, "r", "Conert to canonical path", "path_canonical: [<regex>]" },
    { "phd:", "path_dir:", NULL, "r", "Extract last dir of the path", "path_dir: [<regex>]" },
    { "phf:", "path_file:", NULL, "r", "Extract file of the path", "path_file: [<regex>]" },
    { "phr:", "path_rename:", NULL, "r", "Rename file of the path without specific chars", "path_rename: [<regex>]" },
    { "rnn:", "randomize_num:", NULL, "r", "Randomize numeric values", "randomize_num: [<regex>]" },
    { "rna:", "randomize_alpha:", NULL, "r", "Randomize alpha values", "randomize_alpha: [<regex>]" },
    { "rnan:", "randomize_alphaum:", NULL, "r", "Randomize alpha numeric values", "randomize_alphaum: [<regex>]" },
};

#define LED_FN_TABLE_MAX sizeof(LED_FN_TABLE)/sizeof(led_fn_struct)

led_fn_struct* led_fn_table_descriptor(size_t fn_id) {
    led_assert(fn_id < LED_FN_TABLE_MAX, LED_ERR_INTERNAL, "Function index out of table");
    return LED_FN_TABLE + fn_id;
}

size_t led_fn_table_size() {
    return LED_FN_TABLE_MAX;
}