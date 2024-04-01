#include "led.h"

#include <b64/cencode.h>
#include <b64/cdecode.h>
#include <libgen.h>

//-----------------------------------------------
// LED functions utilities
//-----------------------------------------------

#define countof(a) (sizeof(a)/sizeof(a[0]))

int led_zone_pre_process(led_fn_struct* pfunc) {
    int rc;
    led_line_init(&led.line_write);

    if (pfunc->regex != NULL)
        rc = led_line_match(&led.line_prep, pfunc->regex);
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

void led_fn_impl_register(led_fn_struct* pfunc) {
    // register is a passtrough function, line stays unchanged
    led_line_copy(&led.line_write, &led.line_prep);

    if (pfunc->regex) {
        pcre2_match_data* match_data = pcre2_match_data_create_from_pattern(pfunc->regex, NULL);
        int rc = pcre2_match(pfunc->regex, (PCRE2_SPTR)led.line_prep.str, led.line_prep.len, 0, 0, match_data, NULL);
        led_debug("match_count %d ", rc);
        for (int ir = 0; ir < rc && ir < LED_REG_MAX; ir++) {
            PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(match_data);
            int iv = ir * 2;
            led_debug("match_offset values %d %d", ovector[iv], ovector[iv+1]);
            led_line_init(&led.line_reg[ir]);
            led_line_append_str_start_stop(&led.line_reg[ir], led.line_prep.str, ovector[iv], ovector[iv+1]);
            led_debug("register value %d (%s)", ir, led.line_reg[ir].str);
        }
        pcre2_match_data_free(match_data);
    }
    else {
        led_line_copy(&led.line_reg[0], &led.line_prep);
    }
}

void led_fn_impl_substitute(led_fn_struct* pfunc) {
    if ( ! pfunc->arg[2].str ) {
        led_debug("Replace registers in substitute string (len=%d) %s", pfunc->arg[0].len, pfunc->arg[0].str);
        // store the substitute line into unused 3rd arg.
        pfunc->arg[2].str = pfunc->tmp_buf;
        pfunc->arg[2].len = 0;
        for (size_t i = 0; i < pfunc->arg[0].len; i++) {
            if (pfunc->arg[2].len == LED_BUF_MAX)
                break;
            if (led_str_equal_len(pfunc->arg[0].str + i, "$R", 2)) {
                size_t in = i+2;
                size_t ir = 0;
                if (in < pfunc->arg[0].len && pfunc->arg[0].str[in] >= '0' && pfunc->arg[0].str[in] <= '9')
                    ir = pfunc->arg[0].str[in++] - '0';
                led_debug("Replace register %d found at %d", ir, i);
                for (size_t j=0; j < led.line_reg[ir].len && pfunc->arg[2].len < LED_BUF_MAX; j++) {
                    pfunc->arg[2].str[pfunc->arg[2].len++] = led.line_reg[ir].str[j];
                }
                i = in-1;
            }
            else {
                pfunc->arg[2].str[pfunc->arg[2].len++] = pfunc->arg[0].str[i];
            }
        }
        pfunc->arg[2].str[pfunc->arg[2].len] = '\0';
        led_debug("Substitute line (len=%d) %s", pfunc->arg[2].len, pfunc->arg[2].str);
    }

    led_debug("Substitute prep line (len=%d)", led.line_prep.len);
    PCRE2_SIZE len = LED_BUF_MAX;
    int rc = pcre2_substitute(
                pfunc->regex,
                (PCRE2_UCHAR8*)led.line_prep.str,
                led.line_prep.len,
                0,
                PCRE2_SUBSTITUTE_EXTENDED|PCRE2_SUBSTITUTE_GLOBAL,
                NULL,
                NULL,
                (PCRE2_UCHAR8*)pfunc->arg[2].str,
                pfunc->arg[2].len,
                (PCRE2_UCHAR8*)led.line_write.buf,
                &len);
    led_assert_pcre(rc);
    led.line_write.str = led.line_write.buf;
    led.line_write.len = len;
}

void led_fn_impl_delete(led_fn_struct* pfunc) {
    led_zone_pre_process(pfunc);

    if (led.line_prep.zone_start == 0 && led.line_prep.zone_stop == led.line_prep.len)
        // delete all the line if it all match
        led_line_reset(&led.line_write);
    else
        // only remove matching zone
        led_zone_post_process();
}

void led_fn_impl_delete_blank(led_fn_struct* pfunc) {
    if (led.line_prep.str[0] == '\0' || led_regex_match(LED_REGEX_BLANK_LINE, led.line_prep.str, led.line_prep.len))
        led_line_reset(&led.line_write);
    else
        led_line_copy(&led.line_write, &led.line_prep);
}

void led_fn_impl_range_sel(led_fn_struct* pfunc) {
    led_line_init(&led.line_write);

    if (pfunc->arg[0].len) {
        long val = pfunc->arg[0].val;
        size_t uval = pfunc->arg[0].uval;
        if (val > 0)
            led.line_prep.zone_start = uval > led.line_prep.len ? led.line_prep.len : uval;
        else
            led.line_prep.zone_start = uval > led.line_prep.len ? 0 : led.line_prep.len - uval;
    }
    if (pfunc->arg[1].len) {
        size_t uval = pfunc->arg[1].uval;
        led.line_prep.zone_stop = led.line_prep.zone_start + uval > led.line_prep.len ? led.line_prep.len : led.line_prep.zone_start + uval;
    }
    else
        led.line_prep.zone_stop = led.line_prep.len;

    led_line_append_zone(&led.line_write, &led.line_prep);
}

void led_fn_impl_range_unsel(led_fn_struct* pfunc) {
    if (pfunc->arg[0].len) {
        long val = pfunc->arg[0].val;
        size_t uval = pfunc->arg[0].uval;
        if (val > 0) {
            led.line_prep.zone_start = uval > led.line_prep.len ? led.line_prep.len : uval;
        }
        else {
            led.line_prep.zone_start = uval > led.line_prep.len ? 0 : led.line_prep.len - uval;
        }
    }
    if (pfunc->arg[1].len) {
        size_t uval = (size_t)pfunc->arg[1].val;
        led.line_prep.zone_stop = led.line_prep.zone_start + uval > led.line_prep.len ? led.line_prep.len : led.line_prep.zone_start + uval;
    }
    else
        led.line_prep.zone_stop = led.line_prep.len;

    led_line_append_before_zone(&led.line_write, &led.line_prep);
    led_line_append_after_zone(&led.line_write, &led.line_prep);
}

void led_fn_impl_translate(led_fn_struct* pfunc) {
    led_zone_pre_process(pfunc);

    for (size_t i=led.line_prep.zone_start; i<led.line_prep.zone_stop; i++) {
        char c = led.line_prep.str[i];
        size_t j;
        for (j=0; j<pfunc->arg[0].len; j++) {
            if (pfunc->arg[0].str[j] == c) {
                if (j < pfunc->arg[1].len)
                    led_line_append_char(&led.line_write, pfunc->arg[1].str[j]);
                break;
            }
        }

        /* output only if no substitution has been done */
        if (j == pfunc->arg[0].len)
            led_line_append_char(&led.line_write, led.line_prep.str[i]);
    }
    led_zone_post_process();
}

void led_fn_impl_case_lower(led_fn_struct* pfunc) {
    led_zone_pre_process(pfunc);

    for (size_t i=led.line_prep.zone_start; i<led.line_prep.zone_stop; i++)
        led_line_append_char(&led.line_write, tolower(led.line_prep.str[i]));

    led_zone_post_process();
}

void led_fn_impl_case_upper(led_fn_struct* pfunc) {
    led_zone_pre_process(pfunc);

    for (size_t i=led.line_prep.zone_start; i<led.line_prep.zone_stop; i++)
        led_line_append_char(&led.line_write, toupper(led.line_prep.str[i]));

    led_zone_post_process();
}

void led_fn_impl_case_first(led_fn_struct* pfunc) {
    led_zone_pre_process(pfunc);

    led_line_append_char(&led.line_write, toupper(led.line_prep.str[led.line_prep.zone_start]));
    for (size_t i=led.line_prep.zone_start+1; i<led.line_prep.zone_stop; i++)
        led_line_append_char(&led.line_write, tolower(led.line_prep.str[i]));

    led_zone_post_process();
}

void led_fn_impl_case_camel(led_fn_struct* pfunc) {
    led_zone_pre_process(pfunc);

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

void led_fn_impl_insert(led_fn_struct* pfunc) {
    led_line_append_str_len(&led.line_write, pfunc->arg[0].str, pfunc->arg[0].len);
    led_line_append_char(&led.line_write, '\n');
    led_line_append(&led.line_write, &led.line_prep);
}

void led_fn_impl_append(led_fn_struct* pfunc) {
    led_line_append(&led.line_write, &led.line_prep);
    led_line_append_char(&led.line_write, '\n');
    led_line_append_str_len(&led.line_write, pfunc->arg[0].str, pfunc->arg[0].len);
}

void led_fn_impl_quote_base(led_fn_struct* pfunc, char q) {
    led_zone_pre_process(pfunc);

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

void led_fn_impl_quote_simple(led_fn_struct* pfunc) { led_fn_impl_quote_base(pfunc, '\''); }
void led_fn_impl_quote_double(led_fn_struct* pfunc) { led_fn_impl_quote_base(pfunc, '"'); }
void led_fn_impl_quote_back(led_fn_struct* pfunc) { led_fn_impl_quote_base(pfunc, '`'); }

void led_fn_impl_quote_remove(led_fn_struct* pfunc) {
    const char* QUOTES="'\"`";
    led_zone_pre_process(pfunc);

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

void led_fn_impl_trim(led_fn_struct* pfunc) {
    led_zone_pre_process(pfunc);

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

void led_fn_impl_trim_left(led_fn_struct* pfunc) {
    led_zone_pre_process(pfunc);

    size_t str_start = led.line_prep.zone_start;
    for (; str_start < led.line_prep.zone_stop; str_start++) {
        if (!isspace(led.line_prep.str[str_start])) break;
    }
    led_line_append_str_start_stop(&led.line_write, led.line_prep.str, str_start, led.line_prep.zone_stop);

    led_zone_post_process();
}

void led_fn_impl_trim_right(led_fn_struct* pfunc) {
    led_zone_pre_process(pfunc);

    size_t str_stop = led.line_prep.zone_stop;
    for (; str_stop > led.line_prep.zone_start; str_stop--)
        if (!isspace(led.line_prep.str[str_stop - 1])) break;
    led_line_append_str_start_stop(&led.line_write, led.line_prep.str, led.line_prep.zone_start, str_stop);

    led_zone_post_process();
}

void led_fn_impl_base64_encode(led_fn_struct* pfunc) {
    led_zone_pre_process(pfunc);

    base64_encodestate base64_state;
	size_t count = 0;

	base64_init_encodestate(&base64_state);
	count = base64_encode_block(led.line_prep.str + led.line_prep.zone_start, led.line_prep.zone_stop - led.line_prep.zone_start, led.line_write.buf + led.line_write.len, &base64_state);
	count += base64_encode_blockend(led.line_write.buf + led.line_write.len + count, &base64_state);
    led.line_write.len += count - 1; // remove newline
    led.line_write.str[led.line_write.len] = '\0';

    led_zone_post_process();
}

void led_fn_impl_base64_decode(led_fn_struct* pfunc) {
    led_zone_pre_process(pfunc);

	base64_decodestate base64_state;
	size_t count = 0;

	base64_init_decodestate(&base64_state);
	count = base64_decode_block(led.line_prep.str + led.line_prep.zone_start, led.line_prep.zone_stop - led.line_prep.zone_start, led.line_write.buf + led.line_write.len, &base64_state);
    led.line_write.len += count;
    led.line_write.str[led.line_write.len] = '\0';

    led_zone_post_process();
}

void led_fn_impl_url_encode(led_fn_struct* pfunc) {
    led_zone_pre_process(pfunc);

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

void led_fn_impl_realpath(led_fn_struct* pfunc) {
    led_zone_pre_process(pfunc);

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

void led_fn_impl_dirname(led_fn_struct* pfunc) {
    led_zone_pre_process(pfunc);

    const char* dir = dirname(led.line_prep.str + led.line_prep.zone_start);
    if (dir != NULL) led_line_append_str(&led.line_write, dir);
    else led_line_append_zone(&led.line_write, &led.line_prep);

    led_zone_post_process();
}

void led_fn_impl_basename(led_fn_struct* pfunc) {
    led_zone_pre_process(pfunc);

    const char* fname = basename(led.line_prep.str + led.line_prep.zone_start);
    if (fname != NULL) led_line_append_str(&led.line_write, fname);
    else led_line_append_zone(&led.line_write, &led.line_prep);

    led_zone_post_process();
}

void led_fn_impl_revert(led_fn_struct* pfunc) {
    led_zone_pre_process(pfunc);

    for (size_t i = led.line_prep.zone_stop; i > led.line_prep.zone_start; i--)
        led_line_append_char(&led.line_write, led.line_prep.buf[i - 1]);

    led_zone_post_process();
}

void led_fn_impl_field_base(led_fn_struct* pfunc, const char* field_sep) {
    led_zone_pre_process(pfunc);
    size_t field_n = pfunc->arg[0].uval;
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

void led_fn_impl_field(led_fn_struct* pfunc) { led_fn_impl_field_base(pfunc, pfunc->arg[1].str); }
void led_fn_impl_field_csv(led_fn_struct* pfunc) { led_fn_impl_field_base(pfunc, ",;"); }
void led_fn_impl_field_space(led_fn_struct* pfunc) { led_fn_impl_field_base(pfunc, " \t\n"); }
void led_fn_impl_field_mixed(led_fn_struct* pfunc) { led_fn_impl_field_base(pfunc, ",; \t\n"); }

void led_fn_impl_join(led_fn_struct* pfunc) {
   for (size_t i = 0; i < led.line_prep.len; i++) {
        char c = led.line_prep.str[i];
        if ( c != '\n') led_line_append_char(&led.line_write, c);
   }
}

void led_fn_impl_split_base(led_fn_struct* pfunc, const char* field_sep) {
    led_zone_pre_process(pfunc);

    for (size_t i = led.line_prep.zone_start; i < led.line_prep.zone_stop; i++) {
        char c = led.line_prep.str[i];
        if ( led_char_in_str(c, field_sep) ) c = '\n';
        led_line_append_char(&led.line_write, c);
    }
    led_zone_post_process();
}

void led_fn_impl_split(led_fn_struct* pfunc) { led_fn_impl_split_base(pfunc, pfunc->arg[0].str); }
void led_fn_impl_split_space(led_fn_struct* pfunc) { led_fn_impl_split_base(pfunc, " \t\n"); }
void led_fn_impl_split_csv(led_fn_struct* pfunc) { led_fn_impl_split_base(pfunc, ",;"); }
void led_fn_impl_split_mixed(led_fn_struct* pfunc) { led_fn_impl_split_base(pfunc, ",; \t\n"); }

void led_fn_impl_randomize_base(led_fn_struct* pfunc, const char* charset, size_t len) {
    led_zone_pre_process(pfunc);

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

void led_fn_impl_randomize_num(led_fn_struct* pfunc) { led_fn_impl_randomize_base(pfunc, randomize_table_num, sizeof randomize_table_num - 1); }
void led_fn_impl_randomize_alpha(led_fn_struct* pfunc) { led_fn_impl_randomize_base(pfunc, randomize_table_alpha, sizeof randomize_table_alpha -1); }
void led_fn_impl_randomize_alnum(led_fn_struct* pfunc) { led_fn_impl_randomize_base(pfunc, randomize_table_alnum, sizeof randomize_table_alnum - 1); }
void led_fn_impl_randomize_hexa(led_fn_struct* pfunc) { led_fn_impl_randomize_base(pfunc, randomize_table_hexa, sizeof randomize_table_hexa - 1); }
void led_fn_impl_randomize_mixed(led_fn_struct* pfunc) { led_fn_impl_randomize_base(pfunc, randomize_table_mixed, sizeof randomize_table_mixed - 1); }

void led_fn_impl_fname_lower(led_fn_struct* pfunc) {
    led_zone_pre_process(pfunc);

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

void led_fn_impl_fname_upper(led_fn_struct* pfunc) {
    led_zone_pre_process(pfunc);

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

void led_fn_impl_fname_camel(led_fn_struct* pfunc) {
    led_zone_pre_process(pfunc);

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

void led_fn_impl_generate(led_fn_struct* pfunc) {
    led_zone_pre_process(pfunc);
    size_t n = pfunc->arg[1].uval > 0 ? led.line_prep.zone_start + pfunc->arg[1].uval : led.line_prep.zone_stop;
    char c = pfunc->arg[0].str[0];

    for (size_t i = led.line_prep.zone_start; i < n; i++) {
        led_line_append_char(&led.line_write, c);
    }

    led_zone_post_process();
}

led_fn_desc_struct LED_FN_TABLE[] = {
    { "r", "register", &led_fn_impl_register, "", "Register", "r/[regex]" },
    { "s", "substitute", &led_fn_impl_substitute, "Ss", "Substitute", "s/[regex]/replace[/opts]" },
    { "d", "delete", &led_fn_impl_delete, "", "Delete line", "d/" },
    { "i", "insert", &led_fn_impl_insert, "Sp", "Insert line", "i//<string>[/N]" },
    { "a", "append", &led_fn_impl_append, "Sp", "Append line", "a//<string>[/N]" },
    { "db", "delete_blank", &led_fn_impl_delete_blank, "", "Delete blank/empty lines", "db/" },
    { "rns", "range_sel", &led_fn_impl_range_sel, "Np", "Range select", "rns/start[/count]" },
    { "rnu", "range_unsel", &led_fn_impl_range_unsel, "Np", "Range unselect", "rnu/[regex]/start[/count]" },
    { "tr", "translate", &led_fn_impl_translate, "SS", "Translate", "tr/[regex]/chars/chars" },
    { "cl", "case_lower", &led_fn_impl_case_lower, "", "Case to lower", "cl/[regex]" },
    { "cu", "case_upper", &led_fn_impl_case_upper, "", "Case to upper", "cu/[regex]" },
    { "cf", "case_first", &led_fn_impl_case_first, "", "Case first upper", "cf/[regex]" },
    { "cc", "case_camel", &led_fn_impl_case_camel, "", "Case to camel style", "cc/[regex]" },
    { "qt", "quote_simple", &led_fn_impl_quote_simple, "", "Quote simple", "q/[regex]" },
    { "qd", "quote_double", &led_fn_impl_quote_double, "", "Quote double", "qd/[regex]" },
    { "qb", "quote_back", &led_fn_impl_quote_back, "", "Quote back", "qb/[regex]" },
    { "qr", "quote_remove", &led_fn_impl_quote_remove, "", "Quote remove", "qr/[regex]/chars" },
    { "sp", "split", &led_fn_impl_split, "", "Split using characters", "split/[regex]/chars" },
    { "spc", "split_csv", &led_fn_impl_split_csv, "", "Split using comma", "split/[regex]" },
    { "sps", "split_space", &led_fn_impl_split_space, "", "Split using space", "split/[regex]" },
    { "spm", "split_mixed", &led_fn_impl_split_mixed, "", "Split using comma and space", "split/[regex]" },
    { "jn", "join", &led_fn_impl_join, "", "Join lines (only with pack mode)", "join/" },
    { "tm", "trim", &led_fn_impl_trim, "", "Trim", "trim/[regex]" },
    { "tml", "trim_left", &led_fn_impl_trim_left, "", "Trim left", "trim_left/[regex]" },
    { "tmr", "trim_right", &led_fn_impl_trim_right, "", "Trim right", "trim_right/[regex]" },
    { "rv", "revert", &led_fn_impl_revert, "", "Revert", "revert/[regex]" },
    { "fld", "field", &led_fn_impl_field, "PSp", "Extract field with separator chars", "field/[regex]/N/sep[/count]" },
    { "fls", "field_space", &led_fn_impl_field_space, "Pp", "Extract field separated by space", "field_space/[regex]/N[/count]" },
    { "flc", "field_csv", &led_fn_impl_field_csv, "Pp", "Extract field separated by comma", "field_csv/N[/count]" },
    { "flm", "field_mixed", &led_fn_impl_field_mixed, "Pp", "Extract field separated by space or comma", "field_mixed/[regex]N[/count]" },
    { "b64e", "base64_encode", &led_fn_impl_base64_encode, "", "Encode base64", "base64_encode/[regex]" },
    { "b64d", "base64_decode", &led_fn_impl_base64_decode, "", "Decode base64", "base64_decode/[regex]" },
    { "urle", "url_encode", &led_fn_impl_url_encode, "", "Encode URL", "url_encode/[regex]" },
    { "rp", "realpath", &led_fn_impl_realpath, "", "Convert to real path (canonical)", "realpath/[regex]" },
    { "dn", "dirname", &led_fn_impl_dirname, "", "Extract last dir of the path", "dirname/[regex]" },
    { "bn", "basename", &led_fn_impl_basename, "", "Extract file of the path", "basename/[regex]" },
    { "fnl", "fname_lower", &led_fn_impl_fname_lower, "", "simplify file name using lower case", "fname_lower/" },
    { "fnu", "fname_upper", &led_fn_impl_fname_upper, "", "simplify file name using upper case", "fname_upper/" },
    { "fnc", "fname_camel", &led_fn_impl_fname_camel, "", "simplify file name using camel case", "fname_camel/" },
    { "rzn", "randomize_num", &led_fn_impl_randomize_num, "", "Randomize numeric values", "randomize_num/" },
    { "rza", "randomize_alpha", &led_fn_impl_randomize_alpha, "", "Randomize alpha values", "randomize_alpha/" },
    { "rzan", "randomize_alnum", &led_fn_impl_randomize_alnum, "", "Randomize alpha numeric values", "randomize_alnum/" },
    { "rzh", "randomize_hexa", &led_fn_impl_randomize_hexa, "", "Randomize alpha numeric values", "randomize_hexa/" },
    { "rzm", "randomize_mixed", &led_fn_impl_randomize_mixed, "", "Randomize alpha numeric and custom chars", "randomize_mixed/" },
    { "gen", "generate", &led_fn_impl_generate, "Sp", "Generate chars", "generate/<char>[/N]" },
};

#define LED_FN_TABLE_MAX sizeof(LED_FN_TABLE)/sizeof(led_fn_desc_struct)

led_fn_desc_struct* led_fn_table_descriptor(size_t id) {
    led_assert(id < LED_FN_TABLE_MAX, LED_ERR_INTERNAL, "Function index out of table");
    return LED_FN_TABLE + id;
}

size_t led_fn_table_size() {
    return LED_FN_TABLE_MAX;
}
