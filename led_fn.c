#include "led.h"

#include <b64/cencode.h>
#include <b64/cdecode.h>
#include <libgen.h>

//-----------------------------------------------
// LED functions utilities
//-----------------------------------------------

#define countof(a) (sizeof(a)/sizeof(a[0]))

int led_zone_pre_process(pcre2_code* regex) {
    int rc;
    led_line_init(&led.line_dst);

    if (regex != NULL) {
        led.line_ready.zone_start = led.line_ready.len;
        led.line_ready.zone_stop = led.line_ready.len;
        rc = led_regex_match_offset(regex, led.line_ready.str, led.line_ready.len, &led.line_ready.zone_start, &led.line_ready.zone_stop);
    }
    else {
        led.line_ready.zone_start = 0;
        led.line_ready.zone_stop = led.line_ready.len;
        rc = LED_RGX_STR_MATCH;
    }

    if (!led.opt.output_match) led_line_append_before_zone(&led.line_dst, &led.line_ready);
    return rc;
}

void led_zone_post_process() {
    if (!led.opt.output_match) led_line_append_after_zone(&led.line_dst, &led.line_ready);
}

//-----------------------------------------------
// LED functions
//-----------------------------------------------

void led_fn_impl_none() {
    led_line_copy(&led.line_dst, &led.line_ready);
}

void led_fn_impl_substitute() {
    PCRE2_SIZE len = LED_BUF_MAX;
    int rc = pcre2_substitute(
                led.fn_arg[0].regex,
                (PCRE2_UCHAR8*)led.line_ready.str,
                led.line_ready.len,
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
    led_zone_pre_process(led.fn_arg[0].regex);

    if (led.line_ready.zone_start == 0 && led.line_ready.zone_stop == led.line_ready.len)
        // delete all the line if it all match
        led_line_reset(&led.line_dst);
    else
        // only remove matching zone
        led_zone_post_process();
}

void led_fn_impl_remove_blank() {
    if (led.line_ready.str[0] == '\0' || led_regex_match(LED_REGEX_BLANK_LINE, led.line_ready.str, led.line_ready.len))
        led_line_reset(&led.line_dst);
    else
        led_line_copy(&led.line_dst, &led.line_ready);
}

void led_fn_impl_range_sel() {
    led_line_init(&led.line_dst);

    if (led.fn_arg[0].len) {
        long val = led.fn_arg[0].val;
        size_t uval = led.fn_arg[0].uval;
        if (val > 0)
            led.line_ready.zone_start = uval > led.line_ready.len ? led.line_ready.len : uval;
        else
            led.line_ready.zone_start = uval > led.line_ready.len ? 0 : led.line_ready.len - uval;
    }
    if (led.fn_arg[1].len) {
        size_t uval = led.fn_arg[1].uval;
        led.line_ready.zone_stop = led.line_ready.zone_start + uval > led.line_ready.len ? led.line_ready.len : led.line_ready.zone_start + uval;
    }
    else
        led.line_ready.zone_stop = led.line_ready.len;

    led_line_append_zone(&led.line_dst, &led.line_ready);
}

void led_fn_impl_range_unsel() {
    if (led.fn_arg[0].len) {
        long val = led.fn_arg[0].val;
        size_t uval = led.fn_arg[0].uval;
        if (val > 0) {
            led.line_ready.zone_start = uval > led.line_ready.len ? led.line_ready.len : uval;
        }
        else {
            led.line_ready.zone_start = uval > led.line_ready.len ? 0 : led.line_ready.len - uval;
        }
    }
    if (led.fn_arg[1].len) {
        size_t uval = (size_t)led.fn_arg[1].val;
        led.line_ready.zone_stop = led.line_ready.zone_start + uval > led.line_ready.len ? led.line_ready.len : led.line_ready.zone_start + uval;
    }
    else
        led.line_ready.zone_stop = led.line_ready.len;

    led_line_append_before_zone(&led.line_dst, &led.line_ready);
    led_line_append_after_zone(&led.line_dst, &led.line_ready);
}

void led_fn_impl_translate() {
    led_zone_pre_process(led.fn_arg[1].regex);

    for (size_t i=led.line_ready.zone_start; i<led.line_ready.zone_stop; i++) {
        char c = led.line_ready.str[i];
        for (size_t j=0; j<led.fn_arg[0].len; j++) {
            if (led.fn_arg[0].str[j] == c) {
                if (j < led.fn_arg[1].len)
                    led_line_append_char(&led.line_dst, led.fn_arg[0].str[j]);
                break;
            }
        }
    }
    led_zone_post_process();
}

void led_fn_impl_case_lower() {
    led_zone_pre_process(led.fn_arg[1].regex);

    for (size_t i=led.line_ready.zone_start; i<led.line_ready.zone_stop; i++)
        led_line_append_char(&led.line_dst, tolower(led.line_ready.str[i]));

    led_zone_post_process();
}

void led_fn_impl_case_upper() {
    led_zone_pre_process(led.fn_arg[0].regex);

    for (size_t i=led.line_ready.zone_start; i<led.line_ready.zone_stop; i++)
        led_line_append_char(&led.line_dst, toupper(led.line_ready.str[i]));

    led_zone_post_process();
}

void led_fn_impl_case_first() {
    led_zone_pre_process(led.fn_arg[0].regex);

    led_line_append_char(&led.line_dst, toupper(led.line_ready.str[led.line_ready.zone_start]));
    for (size_t i=led.line_ready.zone_start+1; i<led.line_ready.zone_stop; i++)
        led_line_append_char(&led.line_dst, tolower(led.line_ready.str[i]));

    led_zone_post_process();
}

void led_fn_impl_case_camel() {
    led_zone_pre_process(led.fn_arg[0].regex);

    int wasword = FALSE;
    for (size_t i=led.line_ready.zone_start; i<led.line_ready.zone_stop; i++) {
        int c = led.line_ready.str[i];
        int isword = isalnum(c) || c == '_';
        if (isword) {
            if (wasword) led_line_append_char(&led.line_dst, tolower(led.line_ready.str[i]));
            else led_line_append_char(&led.line_dst, toupper(led.line_ready.str[i]));
        }
        wasword = isword;
    }

    led_zone_post_process();
}

void led_fn_impl_insert() {
    led_line_append_str_len(&led.line_dst, led.fn_arg[0].str, led.fn_arg[0].len);
    led_line_append_char(&led.line_dst, '\n');
    led_line_append(&led.line_dst, &led.line_ready);
}

void led_fn_impl_append() {
    led_line_append(&led.line_dst, &led.line_ready);
    led_line_append_char(&led.line_dst, '\n');
    led_line_append_str_len(&led.line_dst, led.fn_arg[0].str, led.fn_arg[0].len);
}

void led_fn_impl_quote(char q) {
    led_zone_pre_process(led.fn_arg[0].regex);

    if (! (led.line_ready.str[led.line_ready.zone_start] == q && led.line_ready.str[led.line_ready.zone_stop - 1] == q) ) {
        led_debug("quote active");
        led_line_append_char(&led.line_dst, q);
        led_line_append_zone(&led.line_dst, &led.line_ready);
        led_line_append_char(&led.line_dst, q);
    }
    else
        led_line_append_zone(&led.line_dst, &led.line_ready);

    led_zone_post_process();
}

void led_fn_impl_quote_simple() { led_fn_impl_quote('\''); }
void led_fn_impl_quote_double() { led_fn_impl_quote('"'); }
void led_fn_impl_quote_back() { led_fn_impl_quote('`'); }

void led_fn_impl_quote_remove() {
    const char* QUOTES="'\"`";
    led_zone_pre_process(led.fn_arg[0].regex);

    char q = QUOTES[0];
    for(size_t i = 0; q != '\0'; i++, q = QUOTES[i]) {
        if (led.line_ready.str[led.line_ready.zone_start] == q && led.line_ready.str[led.line_ready.zone_stop - 1] == q) break;
    }

    if (q) {
        led_debug("quotes found: %c", q);
        led_line_append_str_start_stop(&led.line_dst, led.line_ready.str, led.line_ready.zone_start+1, led.line_ready.zone_stop-1);
    }
    else
        led_line_append_zone(&led.line_dst, &led.line_ready);

    led_zone_post_process();
}

void led_fn_impl_trim() {
    led_zone_pre_process(led.fn_arg[0].regex);

    size_t str_start = led.line_ready.zone_start;
    size_t str_stop = led.line_ready.zone_stop;
    for (; str_start < led.line_ready.zone_stop; str_start++) {
        if (!isspace(led.line_ready.str[str_start])) break;
    }
    for (; str_stop > str_start; str_stop--) {
        if (!isspace(led.line_ready.str[str_stop - 1])) break;
    }
    led_line_append_str_start_stop(&led.line_dst, led.line_ready.str, str_start, str_stop);

    led_zone_post_process();
}

void led_fn_impl_trim_left() {
    led_zone_pre_process(led.fn_arg[0].regex);

    size_t str_start = led.line_ready.zone_start;
    for (; str_start < led.line_ready.zone_stop; str_start++) {
        if (!isspace(led.line_ready.str[str_start])) break;
    }
    led_line_append_str_start_stop(&led.line_dst, led.line_ready.str, str_start, led.line_ready.zone_stop);

    led_zone_post_process();
}

void led_fn_impl_trim_right() {
    led_zone_pre_process(led.fn_arg[0].regex);

    size_t str_stop = led.line_ready.zone_stop;
    for (; str_stop > led.line_ready.zone_start; str_stop--)
        if (!isspace(led.line_ready.str[str_stop - 1])) break;
    led_line_append_str_start_stop(&led.line_dst, led.line_ready.str, led.line_ready.zone_start, str_stop);

    led_zone_post_process();
}

void led_fn_impl_base64_encode() {
    led_zone_pre_process(led.fn_arg[0].regex);

    base64_encodestate base64_state;
	size_t count = 0;

	base64_init_encodestate(&base64_state);
	count = base64_encode_block(led.line_ready.str + led.line_ready.zone_start, led.line_ready.zone_stop - led.line_ready.zone_start, led.line_dst.buf + led.line_dst.len, &base64_state);
	count += base64_encode_blockend(led.line_dst.buf + led.line_dst.len + count, &base64_state);
    led.line_dst.len += count;
    led.line_dst.str[led.line_dst.len] = '\0';

    led_zone_post_process();
}

void led_fn_impl_base64_decode() {
    led_zone_pre_process(led.fn_arg[0].regex);

	base64_decodestate base64_state;
	size_t count = 0;

	base64_init_decodestate(&base64_state);
	count = base64_decode_block(led.line_ready.str + led.line_ready.zone_start, led.line_ready.zone_stop - led.line_ready.zone_start, led.line_dst.buf + led.line_dst.len, &base64_state);
    led.line_dst.len += count;
    led.line_dst.str[led.line_dst.len] = '\0';

    led_zone_post_process();
}

void led_fn_impl_url_encode() {
    led_zone_pre_process(led.fn_arg[0].regex);

    const char *HEX = "0123456789ABCDEF";
    char pcbuf[4] = "%00";

    for (size_t i = led.line_ready.zone_start; i < led.line_ready.zone_stop; i++) {
        char c = led.line_ready.str[i];
        if (isalnum(c))
            led_line_append_char(&led.line_dst, c);
        else {
            pcbuf[1] = HEX[(c >> 4) & 0x0F];
            pcbuf[2] = HEX[c & 0x0F];
            led_line_append_str(&led.line_dst, pcbuf);
        }
    }

    led_zone_post_process();
}

void led_fn_impl_path_canonical() {
    led_zone_pre_process(led.fn_arg[0].regex);

    char c = led.line_ready.buf[led.line_ready.zone_stop]; // temporary save this char for realpath function
    led.line_ready.buf[led.line_ready.zone_stop] = '\0';
    if (realpath(led.line_ready.str + led.line_ready.zone_start, led.line_dst.buf + led.line_dst.len) != NULL ) {
        led.line_ready.buf[led.line_ready.zone_stop] = c;
        led.line_dst.len = strlen(led.line_dst.str);
    }
    else {
        led.line_ready.buf[led.line_ready.zone_stop] = c;
        led_line_append_zone(&led.line_dst, &led.line_ready);
    }

    led_zone_post_process();
}

void led_fn_impl_path_dir() {
    led_zone_pre_process(led.fn_arg[0].regex);

    const char* dir = dirname(led.line_ready.str + led.line_ready.zone_start);
    if (dir != NULL) led_line_append_str(&led.line_dst, dir);
    else led_line_append_zone(&led.line_dst, &led.line_ready);

    led_zone_post_process();
}

void led_fn_impl_path_file() {
    led_zone_pre_process(led.fn_arg[0].regex);

    const char* fname = basename(led.line_ready.str + led.line_ready.zone_start);
    if (fname != NULL) led_line_append_str(&led.line_dst, fname);
    else led_line_append_zone(&led.line_dst, &led.line_ready);

    led_zone_post_process();
}

void led_fn_impl_revert() {
    led_zone_pre_process(led.fn_arg[0].regex);

    for (size_t i = led.line_ready.zone_stop; i > led.line_ready.zone_start; i--)
        led_line_append_char(&led.line_dst, led.line_ready.buf[i - 1]);

    led_zone_post_process();
}

led_fn_struct LED_FN_TABLE[] = {
    { "nn:", "none:", &led_fn_impl_none, "", "No processing", "none:" },
    { "sub:", "substitute:", &led_fn_impl_substitute, "RS", "Substitute", "substitute: <regex> <replace>" },
    { "exe:", "execute:", NULL, "RS", "Execute", "execute: <regex> <replace=command>" },
    { "rm:", "remove:", &led_fn_impl_remove, "r", "Remove line", "remove: [<regex>]" },
    { "rmb:", "remove_blank:", &led_fn_impl_remove_blank, "", "Remove blank/empty lines", "remove_blank:" },
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
    { "rv:", "revert:", &led_fn_impl_revert, "r", "Revert", "revert: [<regex>]" },
    { "fl:", "field:", NULL, "sp", "Extract fields", "field: [<sep>] [<N>]" },
    { "jn:", "join:", NULL, "", "Join lines", "join:" },
    { "b64e:", "base64_encode:", &led_fn_impl_base64_encode, "r", "Encrypt base64", "encrypt_base64: [<regex>]" },
    { "b64d:", "base64_decode:", &led_fn_impl_base64_decode, "r", "Decrypt base64", "decrypt_base64: [<regex>]" },
    { "urle:", "url_encode:", &led_fn_impl_url_encode, "r", "Encode URL", "url_encode: [<regex>]" },
    { "phc:", "path_canonical:", &led_fn_impl_path_canonical, "r", "Conert to canonical path", "path_canonical: [<regex>]" },
    { "phd:", "path_dir:", &led_fn_impl_path_dir, "r", "Extract last dir of the path", "path_dir: [<regex>]" },
    { "phf:", "path_file:", &led_fn_impl_path_file, "r", "Extract file of the path", "path_file: [<regex>]" },
    { "phr:", "path_rename:", NULL, "r", "Rename file of the path without specific chars", "path_rename: [<regex>]" },
    { "rnn:", "randomize_num:", NULL, "r", "Randomize numeric values", "randomize_num: [<regex>]" },
    { "rna:", "randomize_alpha:", NULL, "r", "Randomize alpha values", "randomize_alpha: [<regex>]" },
    { "rnan:", "randomize_alnum:", NULL, "r", "Randomize alpha numeric values", "randomize_alnum: [<regex>]" },
    { "rnh:", "randomize_hexa:", NULL, "r", "Randomize alpha numeric values", "randomize_hexa: [<regex>]" },
};

#define LED_FN_TABLE_MAX sizeof(LED_FN_TABLE)/sizeof(led_fn_struct)

led_fn_struct* led_fn_table_descriptor(size_t fn_id) {
    led_assert(fn_id < LED_FN_TABLE_MAX, LED_ERR_INTERNAL, "Function index out of table");
    return LED_FN_TABLE + fn_id;
}

size_t led_fn_table_size() {
    return LED_FN_TABLE_MAX;
}
