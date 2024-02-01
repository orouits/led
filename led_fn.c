#include "led.h"

#include <b64/cencode.h>
#include <b64/cdecode.h>
#include <libgen.h>

//-----------------------------------------------
// LED functions utilities
//-----------------------------------------------

#define countof(a) (sizeof(a)/sizeof(a[0]))

int led_zone_pre_process() {
    int rc;
    led_line_init(&led.line_write);

    if (led.fn_regex != NULL) {
        led.line_prep.zone_start = led.line_prep.len;
        led.line_prep.zone_stop = led.line_prep.len;
        rc = led_regex_match_offset(led.fn_regex, led.line_prep.str, led.line_prep.len, &led.line_prep.zone_start, &led.line_prep.zone_stop);
    }
    else {
        led.line_prep.zone_start = 0;
        led.line_prep.zone_stop = led.line_prep.len;
        rc = LED_RGX_STR_MATCH;
    }

    if (!led.opt.output_match) led_line_append_before_zone(&led.line_write, &led.line_prep);
    return rc;
}

void led_zone_post_process() {
    if (!led.opt.output_match) led_line_append_after_zone(&led.line_write, &led.line_prep);
}

//-----------------------------------------------
// LED functions
//-----------------------------------------------

void led_fn_impl_none() {
    led_line_copy(&led.line_write, &led.line_prep);
}

void led_fn_impl_substitute() {
    PCRE2_SIZE len = LED_BUF_MAX;
    int rc = pcre2_substitute(
                led.fn_regex,
                (PCRE2_UCHAR8*)led.line_prep.str,
                led.line_prep.len,
                0,
                PCRE2_SUBSTITUTE_EXTENDED|PCRE2_SUBSTITUTE_GLOBAL,
                NULL,
                NULL,
                (PCRE2_UCHAR8*)led.fn_arg[0].str,
                led.fn_arg[0].len,
                (PCRE2_UCHAR8*)led.line_write.buf,
                &len);
    led_assert_pcre(rc);
    led.line_write.str = led.line_write.buf;
    led.line_write.len = len;
}

void led_fn_impl_remove() {
    led_zone_pre_process();

    if (led.line_prep.zone_start == 0 && led.line_prep.zone_stop == led.line_prep.len)
        // delete all the line if it all match
        led_line_reset(&led.line_write);
    else
        // only remove matching zone
        led_zone_post_process();
}

void led_fn_impl_remove_blank() {
    if (led.line_prep.str[0] == '\0' || led_regex_match(LED_REGEX_BLANK_LINE, led.line_prep.str, led.line_prep.len))
        led_line_reset(&led.line_write);
    else
        led_line_copy(&led.line_write, &led.line_prep);
}

void led_fn_impl_range_sel() {
    led_line_init(&led.line_write);

    if (led.fn_arg[0].len) {
        long val = led.fn_arg[0].val;
        size_t uval = led.fn_arg[0].uval;
        if (val > 0)
            led.line_prep.zone_start = uval > led.line_prep.len ? led.line_prep.len : uval;
        else
            led.line_prep.zone_start = uval > led.line_prep.len ? 0 : led.line_prep.len - uval;
    }
    if (led.fn_arg[1].len) {
        size_t uval = led.fn_arg[1].uval;
        led.line_prep.zone_stop = led.line_prep.zone_start + uval > led.line_prep.len ? led.line_prep.len : led.line_prep.zone_start + uval;
    }
    else
        led.line_prep.zone_stop = led.line_prep.len;

    led_line_append_zone(&led.line_write, &led.line_prep);
}

void led_fn_impl_range_unsel() {
    if (led.fn_arg[0].len) {
        long val = led.fn_arg[0].val;
        size_t uval = led.fn_arg[0].uval;
        if (val > 0) {
            led.line_prep.zone_start = uval > led.line_prep.len ? led.line_prep.len : uval;
        }
        else {
            led.line_prep.zone_start = uval > led.line_prep.len ? 0 : led.line_prep.len - uval;
        }
    }
    if (led.fn_arg[1].len) {
        size_t uval = (size_t)led.fn_arg[1].val;
        led.line_prep.zone_stop = led.line_prep.zone_start + uval > led.line_prep.len ? led.line_prep.len : led.line_prep.zone_start + uval;
    }
    else
        led.line_prep.zone_stop = led.line_prep.len;

    led_line_append_before_zone(&led.line_write, &led.line_prep);
    led_line_append_after_zone(&led.line_write, &led.line_prep);
}

void led_fn_impl_translate() {
    led_zone_pre_process();

    for (size_t i=led.line_prep.zone_start; i<led.line_prep.zone_stop; i++) {
        char c = led.line_prep.str[i];
        for (size_t j=0; j<led.fn_arg[0].len; j++) {
            if (led.fn_arg[0].str[j] == c) {
                if (j < led.fn_arg[1].len)
                    led_line_append_char(&led.line_write, led.fn_arg[0].str[j]);
                break;
            }
        }
    }
    led_zone_post_process();
}

void led_fn_impl_case_lower() {
    led_zone_pre_process();

    for (size_t i=led.line_prep.zone_start; i<led.line_prep.zone_stop; i++)
        led_line_append_char(&led.line_write, tolower(led.line_prep.str[i]));

    led_zone_post_process();
}

void led_fn_impl_case_upper() {
    led_zone_pre_process();

    for (size_t i=led.line_prep.zone_start; i<led.line_prep.zone_stop; i++)
        led_line_append_char(&led.line_write, toupper(led.line_prep.str[i]));

    led_zone_post_process();
}

void led_fn_impl_case_first() {
    led_zone_pre_process();

    led_line_append_char(&led.line_write, toupper(led.line_prep.str[led.line_prep.zone_start]));
    for (size_t i=led.line_prep.zone_start+1; i<led.line_prep.zone_stop; i++)
        led_line_append_char(&led.line_write, tolower(led.line_prep.str[i]));

    led_zone_post_process();
}

void led_fn_impl_case_camel() {
    led_zone_pre_process();

    int wasword = FALSE;
    for (size_t i=led.line_prep.zone_start; i<led.line_prep.zone_stop; i++) {
        int c = led.line_prep.str[i];
        int isword = isalnum(c) || c == '_';
        if (isword) {
            if (wasword) led_line_append_char(&led.line_write, tolower(c));
            else led_line_append_char(&led.line_write, toupper(c));
        }
        wasword = isword;
    }

    led_zone_post_process();
}

void led_fn_impl_insert() {
    led_line_append_str_len(&led.line_write, led.fn_arg[0].str, led.fn_arg[0].len);
    led_line_append_char(&led.line_write, '\n');
    led_line_append(&led.line_write, &led.line_prep);
}

void led_fn_impl_append() {
    led_line_append(&led.line_write, &led.line_prep);
    led_line_append_char(&led.line_write, '\n');
    led_line_append_str_len(&led.line_write, led.fn_arg[0].str, led.fn_arg[0].len);
}

void led_fn_impl_quote_base(char q) {
    led_zone_pre_process();

    if (! (led.line_prep.str[led.line_prep.zone_start] == q && led.line_prep.str[led.line_prep.zone_stop - 1] == q) ) {
        led_debug("quote active");
        led_line_append_char(&led.line_write, q);
        led_line_append_zone(&led.line_write, &led.line_prep);
        led_line_append_char(&led.line_write, q);
    }
    else
        led_line_append_zone(&led.line_write, &led.line_prep);

    led_zone_post_process();
}

void led_fn_impl_quote_simple() { led_fn_impl_quote_base('\''); }
void led_fn_impl_quote_double() { led_fn_impl_quote_base('"'); }
void led_fn_impl_quote_back() { led_fn_impl_quote_base('`'); }

void led_fn_impl_quote_remove() {
    const char* QUOTES="'\"`";
    led_zone_pre_process();

    char q = QUOTES[0];
    for(size_t i = 0; q != '\0'; i++, q = QUOTES[i]) {
        if (led.line_prep.str[led.line_prep.zone_start] == q && led.line_prep.str[led.line_prep.zone_stop - 1] == q) break;
    }

    if (q) {
        led_debug("quotes found: %c", q);
        led_line_append_str_start_stop(&led.line_write, led.line_prep.str, led.line_prep.zone_start+1, led.line_prep.zone_stop-1);
    }
    else
        led_line_append_zone(&led.line_write, &led.line_prep);

    led_zone_post_process();
}

void led_fn_impl_trim() {
    led_zone_pre_process();

    size_t str_start = led.line_prep.zone_start;
    size_t str_stop = led.line_prep.zone_stop;
    for (; str_start < led.line_prep.zone_stop; str_start++) {
        if (!isspace(led.line_prep.str[str_start])) break;
    }
    for (; str_stop > str_start; str_stop--) {
        if (!isspace(led.line_prep.str[str_stop - 1])) break;
    }
    led_line_append_str_start_stop(&led.line_write, led.line_prep.str, str_start, str_stop);

    led_zone_post_process();
}

void led_fn_impl_trim_left() {
    led_zone_pre_process();

    size_t str_start = led.line_prep.zone_start;
    for (; str_start < led.line_prep.zone_stop; str_start++) {
        if (!isspace(led.line_prep.str[str_start])) break;
    }
    led_line_append_str_start_stop(&led.line_write, led.line_prep.str, str_start, led.line_prep.zone_stop);

    led_zone_post_process();
}

void led_fn_impl_trim_right() {
    led_zone_pre_process();

    size_t str_stop = led.line_prep.zone_stop;
    for (; str_stop > led.line_prep.zone_start; str_stop--)
        if (!isspace(led.line_prep.str[str_stop - 1])) break;
    led_line_append_str_start_stop(&led.line_write, led.line_prep.str, led.line_prep.zone_start, str_stop);

    led_zone_post_process();
}

void led_fn_impl_base64_encode() {
    led_zone_pre_process();

    base64_encodestate base64_state;
	size_t count = 0;

	base64_init_encodestate(&base64_state);
	count = base64_encode_block(led.line_prep.str + led.line_prep.zone_start, led.line_prep.zone_stop - led.line_prep.zone_start, led.line_write.buf + led.line_write.len, &base64_state);
	count += base64_encode_blockend(led.line_write.buf + led.line_write.len + count, &base64_state);
    led.line_write.len += count - 1; // remove newline
    led.line_write.str[led.line_write.len] = '\0';

    led_zone_post_process();
}

void led_fn_impl_base64_decode() {
    led_zone_pre_process();

	base64_decodestate base64_state;
	size_t count = 0;

	base64_init_decodestate(&base64_state);
	count = base64_decode_block(led.line_prep.str + led.line_prep.zone_start, led.line_prep.zone_stop - led.line_prep.zone_start, led.line_write.buf + led.line_write.len, &base64_state);
    led.line_write.len += count;
    led.line_write.str[led.line_write.len] = '\0';

    led_zone_post_process();
}

void led_fn_impl_url_encode() {
    led_zone_pre_process();

    const char *HEX = "0123456789ABCDEF";
    char pcbuf[4] = "%00";

    for (size_t i = led.line_prep.zone_start; i < led.line_prep.zone_stop; i++) {
        char c = led.line_prep.str[i];
        if (isalnum(c))
            led_line_append_char(&led.line_write, c);
        else {
            pcbuf[1] = HEX[(c >> 4) & 0x0F];
            pcbuf[2] = HEX[c & 0x0F];
            led_line_append_str(&led.line_write, pcbuf);
        }
    }

    led_zone_post_process();
}

void led_fn_impl_path_canonical() {
    led_zone_pre_process();

    char c = led.line_prep.buf[led.line_prep.zone_stop]; // temporary save this char for realpath function
    led.line_prep.buf[led.line_prep.zone_stop] = '\0';
    if (realpath(led.line_prep.str + led.line_prep.zone_start, led.line_write.buf + led.line_write.len) != NULL ) {
        led.line_prep.buf[led.line_prep.zone_stop] = c;
        led.line_write.len = strlen(led.line_write.str);
    }
    else {
        led.line_prep.buf[led.line_prep.zone_stop] = c;
        led_line_append_zone(&led.line_write, &led.line_prep);
    }

    led_zone_post_process();
}

void led_fn_impl_path_dir() {
    led_zone_pre_process();

    const char* dir = dirname(led.line_prep.str + led.line_prep.zone_start);
    if (dir != NULL) led_line_append_str(&led.line_write, dir);
    else led_line_append_zone(&led.line_write, &led.line_prep);

    led_zone_post_process();
}

void led_fn_impl_path_file() {
    led_zone_pre_process();

    const char* fname = basename(led.line_prep.str + led.line_prep.zone_start);
    if (fname != NULL) led_line_append_str(&led.line_write, fname);
    else led_line_append_zone(&led.line_write, &led.line_prep);

    led_zone_post_process();
}

void led_fn_impl_revert() {
    led_zone_pre_process();

    for (size_t i = led.line_prep.zone_stop; i > led.line_prep.zone_start; i--)
        led_line_append_char(&led.line_write, led.line_prep.buf[i - 1]);

    led_zone_post_process();
}

void led_fn_impl_field_base(const char* field_sep) {
    led_zone_pre_process();
    size_t field_n = led.fn_arg[0].uval;
    size_t n = 0;
    int was_sep = TRUE;
    size_t str_start = led.line_prep.zone_start;
    size_t str_stop = led.line_prep.zone_stop;
    for (; str_start < led.line_prep.zone_stop; str_start++ ) {
        int is_sep = led_char_in_str(led.line_prep.str[str_start], field_sep);
        if (was_sep && !is_sep) {
            n++;
            if (n == field_n) break;
        }
        was_sep = is_sep;
    }
    if (n == field_n) {
        was_sep = FALSE;
        for (str_stop = str_start; str_stop < led.line_prep.zone_stop; str_stop++ ) {
            int is_sep = led_char_in_str(led.line_prep.str[str_stop], field_sep);
            if (!was_sep && is_sep) break;
        }
        led_line_append_str_start_stop(&led.line_write, led.line_prep.str, str_start, str_stop);
    }

    led_zone_post_process();
}

void led_fn_impl_field() { led_fn_impl_field_base( led.fn_arg[1].str); }
void led_fn_impl_field_csv() { led_fn_impl_field_base(",;"); }
void led_fn_impl_field_space() { led_fn_impl_field_base(" \t\n"); }
void led_fn_impl_field_mixed() { led_fn_impl_field_base(",; \t\n"); }

void led_fn_impl_join() {
   for (size_t i = 0; i < led.line_prep.len; i++) {
        char c = led.line_prep.str[i];
        if ( c != '\n') led_line_append_char(&led.line_write, c);
   }
}

void led_fn_impl_split_base(const char* field_sep) {
    led_zone_pre_process();

    for (size_t i = led.line_prep.zone_start; i < led.line_prep.zone_stop; i++) {
        char c = led.line_prep.str[i];
        if ( led_char_in_str(c, field_sep) ) c = '\n';
        led_line_append_char(&led.line_write, c);
    }

    led_zone_post_process();
}

void led_fn_impl_split() { led_fn_impl_split_base(led.fn_arg[0].str); }
void led_fn_impl_split_space() { led_fn_impl_split_base(" \t\n"); }
void led_fn_impl_split_csv() { led_fn_impl_split_base(",;"); }
void led_fn_impl_split_mixed() { led_fn_impl_split_base(",; \t\n"); }

void led_fn_impl_randomize_base(const char* charset, size_t len) {
    led_zone_pre_process();

    for (size_t i = led.line_prep.zone_start; i < led.line_prep.zone_stop; i++) {
        char c = charset[rand() % len];
        led_line_append_char(&led.line_write, c);
    }

    led_zone_post_process();
}

const char randomize_table_num[] = "0123456789";
const char randomize_table_alpha[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
const char randomize_table_alnum[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
const char randomize_table_hexa[] = "0123456789ABCDEF";
const char randomize_table_mixed[] = "0123456789-_/=!:;,~#$*?%abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

void led_fn_impl_randomize_num() { led_fn_impl_randomize_base(randomize_table_num, sizeof randomize_table_num - 1); }
void led_fn_impl_randomize_alpha() { led_fn_impl_randomize_base(randomize_table_alpha, sizeof randomize_table_alpha -1); }
void led_fn_impl_randomize_alnum() { led_fn_impl_randomize_base(randomize_table_alnum, sizeof randomize_table_alnum - 1); }
void led_fn_impl_randomize_hexa() { led_fn_impl_randomize_base(randomize_table_hexa, sizeof randomize_table_hexa - 1); }
void led_fn_impl_randomize_mixed() { led_fn_impl_randomize_base(randomize_table_mixed, sizeof randomize_table_mixed - 1); }

void led_fn_impl_execute() {
    led_zone_pre_process();

    if (led.line_prep.zone_start < led.line_prep.zone_stop) {
        led_line_struct cmd;
        led_line_init(&cmd);
        led_line_append_str_len(&cmd, led.fn_arg[0].str, led.fn_arg[0].len);
        if (cmd.len > 0) led_line_append_char(&cmd, ' ');
        led_line_append_str_len(&cmd, led.line_prep.str + led.line_prep.zone_start, led.line_prep.zone_stop - led.line_prep.zone_start);
        FILE *fp = popen(cmd.str, "r");
        led_assert(fp != NULL, LED_ERR_ARG, "Command error");

        char output[4096];
        while (fgets(output, sizeof output, fp) != NULL) {
            led_line_append_str(&led.line_write, output);
        }
        led_line_unappend_char(&led.line_write, '\n');
        pclose(fp);
    }

    led_zone_post_process();
}

void led_fn_impl_fname_lower() {
    led_zone_pre_process();

    if (led.line_prep.zone_start < led.line_prep.zone_stop) {
        size_t iname = led.line_prep.zone_start;
        for (size_t i = iname; i < led.line_prep.zone_stop; i++)
            if (led.line_prep.str[i] == '/') iname = i+1;
        size_t iextdot = led.line_prep.zone_stop;
        for (size_t i = iname; i < led.line_prep.zone_stop; i++)
            if (led.line_prep.str[i] == '.') iextdot = i;

        led_line_append_str_start_stop(&led.line_write, led.line_prep.str, led.line_prep.zone_start, iname);
        for (; iname < led.line_prep.zone_stop; iname++) {
            char c = led.line_prep.str[iname];
            if (isalnum(c))
                led_line_append_char(&led.line_write, tolower(c));
            else if (iname == iextdot)
                led_line_append_char(&led.line_write, '.');
            else if (isalnum(led_line_last_char(&led.line_write)))
                led_line_append_char(&led.line_write, '_');
        }
    }

    led_zone_post_process();
}

void led_fn_impl_fname_upper() {
    led_zone_pre_process();

    if (led.line_prep.zone_start < led.line_prep.zone_stop) {
        size_t iname = led.line_prep.zone_start;
        for (size_t i = iname; i < led.line_prep.zone_stop; i++)
            if (led.line_prep.str[i] == '/') iname = i+1;
        size_t iextdot = led.line_prep.zone_stop;
        for (size_t i = iname; i < led.line_prep.zone_stop; i++)
            if (led.line_prep.str[i] == '.') iextdot = i;

        led_line_append_str_start_stop(&led.line_write, led.line_prep.str, led.line_prep.zone_start, iname);
        for (; iname < led.line_prep.zone_stop; iname++) {
            char c = led.line_prep.str[iname];
            if (isalnum(c))
                led_line_append_char(&led.line_write, toupper(c));
            else if (iname == iextdot)
                led_line_append_char(&led.line_write, '.');
            else if (isalnum(led_line_last_char(&led.line_write)))
                led_line_append_char(&led.line_write, '_');
        }
    }
    led_zone_post_process();
}

void led_fn_impl_fname_camel() {
    led_zone_pre_process();

    if (led.line_prep.zone_start < led.line_prep.zone_stop) {
        size_t iname = led.line_prep.zone_start;
        for (size_t i = iname; i < led.line_prep.zone_stop; i++)
            if (led.line_prep.str[i] == '/') iname = i+1;

        led_line_append_str_start_stop(&led.line_write, led.line_prep.str, led.line_prep.zone_start, iname);
        int wasword = TRUE;
        int isfirst = TRUE;
        for (; iname < led.line_prep.zone_stop; iname++) {
            char c = led.line_prep.str[iname];
            if (c == '.') {
                led_line_append_char(&led.line_write, c);
                isfirst = TRUE;
            }
            else {
                int isword = isalnum(c);
                if (isword) {
                    if (wasword || isfirst) led_line_append_char(&led.line_write, tolower(c));
                    else led_line_append_char(&led.line_write, toupper(c));
                    isfirst = FALSE;
                }
                wasword = isword;
            }
        }
    }
    led_zone_post_process();
}

void led_fn_impl_generate() {
    led_zone_pre_process();
    size_t n = led.fn_arg[1].uval > 0 ? led.line_prep.zone_start + led.fn_arg[1].uval : led.line_prep.zone_stop;
    char c = led.fn_arg[0].str[0];

    for (size_t i = led.line_prep.zone_start; i < n; i++) {
        led_line_append_char(&led.line_write, c);
    }

    led_zone_post_process();
}

led_fn_struct LED_FN_TABLE[] = {
    { "nn", "none", &led_fn_impl_none, "", "No processing", "none:" },
    { "sub", "substitute", &led_fn_impl_substitute, "S", "Substitute", "substitute:<regex> <replace>" },
    { "exe", "execute", &led_fn_impl_execute, "S", "Execute", "execute:<regex> <command>" },
    { "rm", "remove", &led_fn_impl_remove, "", "Remove line", "remove:" },
    { "rmb", "remove_blank", &led_fn_impl_remove_blank, "", "Remove blank/empty lines", "remove_blank:" },
    { "ins", "insert", &led_fn_impl_insert, "Sp", "Insert line", "insert: <string> [N]" },
    { "app", "append", &led_fn_impl_append, "Sp", "Append line", "append: <string> [N]" },
    { "rns", "range_sel", &led_fn_impl_range_sel, "Np", "Range select", "range_sel: <start> [count]" },
    { "rnu", "range_unsel", &led_fn_impl_range_unsel, "Np", "Range unselect", "range_unsel: <start> [count]" },
    { "tr", "translate", &led_fn_impl_translate, "SS", "Translate", "translate: <chars> <chars>" },
    { "csl", "case_lower", &led_fn_impl_case_lower, "", "Case to lower", "case_lower:" },
    { "csu", "case_upper", &led_fn_impl_case_upper, "", "Case to upper", "case_upper:" },
    { "csf", "case_first", &led_fn_impl_case_first, "", "Case first upper", "case_first:" },
    { "csc", "case_camel", &led_fn_impl_case_camel, "", "Case to camel style", "case_camel:" },
    { "qts", "quote_simple", &led_fn_impl_quote_simple, "", "Quote simple", "quote_simple:" },
    { "qtd", "quote_double", &led_fn_impl_quote_double, "", "Quote double", "quote_double:" },
    { "qtb", "quote_back", &led_fn_impl_quote_back, "", "Quote back", "quote_back:" },
    { "qtr", "quote_remove", &led_fn_impl_quote_remove, "", "Quote remove", "quote_remove:" },
    { "sp", "split", &led_fn_impl_split, "S", "Split using chars", "split: <chars> [regex]" },
    { "spc", "split_csv", &led_fn_impl_split_csv, "", "Split using comma", "split: [regex]" },
    { "sps", "split_space", &led_fn_impl_split_space, "", "Split using space", "split: [regex]" },
    { "spm", "split_mixed", &led_fn_impl_split_mixed, "", "Split using comma and space", "split: [regex]" },
    { "jn", "join", &led_fn_impl_join, "", "Join lines (only with pack mode)", "join:" },
    { "tm", "trim", &led_fn_impl_trim, "", "Trim", "trim:" },
    { "tml", "trim_left", &led_fn_impl_trim_left, "", "Trim left", "trim_left:" },
    { "tmr", "trim_right", &led_fn_impl_trim_right, "", "Trim right", "trim_right:" },
    { "rv", "revert", &led_fn_impl_revert, "", "Revert", "revert:" },
    { "fld", "field", &led_fn_impl_field, "PSp", "Extract field with separator chars", "field: <N> <sep> <count>" },
    { "fls", "field_space", &led_fn_impl_field_space, "Pp", "Extract field separated by space", "field_space: <N> <count>" },
    { "flc", "field_csv", &led_fn_impl_field_csv, "Pp", "Extract field separated by comma", "field_csv: <N> <count>" },
    { "flm", "field_mixed", &led_fn_impl_field_mixed, "Pp", "Extract field separated by space or comma", "field_mixed: <N> <count>" },
    { "b64e", "base64_encode", &led_fn_impl_base64_encode, "", "Encode base64", "base64_encode:" },
    { "b64d", "base64_decode", &led_fn_impl_base64_decode, "", "Decode base64", "base64_decode:" },
    { "urle", "url_encode", &led_fn_impl_url_encode, "", "Encode URL", "url_encode:" },
    { "phc", "path_canonical", &led_fn_impl_path_canonical, "", "Convert to canonical path", "path_canonical:" },
    { "phd", "path_dir", &led_fn_impl_path_dir, "", "Extract last dir of the path", "path_dir:" },
    { "phf", "path_file", &led_fn_impl_path_file, "", "Extract file of the path", "path_file:" },
    { "fnl", "fname_lower", &led_fn_impl_fname_lower, "", "simplify file name using lower case", "fname_lower:" },
    { "fnu", "fname_upper", &led_fn_impl_fname_upper, "", "simplify file name using upper case", "fname_upper:" },
    { "fnc", "fname_camel", &led_fn_impl_fname_camel, "", "simplify file name using camel case", "fname_camel:" },
    { "rzn", "randomize_num", &led_fn_impl_randomize_num, "", "Randomize numeric values", "randomize_num:" },
    { "rza", "randomize_alpha", &led_fn_impl_randomize_alpha, "", "Randomize alpha values", "randomize_alpha:" },
    { "rzan", "randomize_alnum", &led_fn_impl_randomize_alnum, "", "Randomize alpha numeric values", "randomize_alnum:" },
    { "rzh", "randomize_hexa", &led_fn_impl_randomize_hexa, "", "Randomize alpha numeric values", "randomize_hexa:" },
    { "rzm", "randomize_mixed", &led_fn_impl_randomize_mixed, "", "Randomize alpha numeric and custom chars", "randomize_mixed:" },
    { "gen", "generate", &led_fn_impl_generate, "Sp", "Generate chars", "generate: <char> [N]" },
};

#define LED_FN_TABLE_MAX sizeof(LED_FN_TABLE)/sizeof(led_fn_struct)

led_fn_struct* led_fn_table_descriptor(size_t fn_id) {
    led_assert(fn_id < LED_FN_TABLE_MAX, LED_ERR_INTERNAL, "Function index out of table");
    return LED_FN_TABLE + fn_id;
}

size_t led_fn_table_size() {
    return LED_FN_TABLE_MAX;
}
