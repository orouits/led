#include "led.h"

#include <b64/cencode.h>
#include <b64/cdecode.h>

//-----------------------------------------------
// LED functions utilities
//-----------------------------------------------

#define countof(a) (sizeof(a)/sizeof(a[0]))

int led_zone_pre_process(led_fn_t* pfunc) {
    led_line_init(&led.line_write);

    led.line_prep.zone_start = led.line_prep.zone_stop = lstr_len(&led.line_prep.sval);
    int rc = lstr_match_offset(&led.line_prep.sval, pfunc->regex, &led.line_prep.zone_start, &led.line_prep.zone_stop);

    if (!led.opt.output_match)
        lstr_app_start_stop(&led.line_write.sval, &led.line_prep.sval, 0, led.line_prep.zone_start);

    return rc;
}

void led_zone_post_process() {
    if (!led.opt.output_match)
        lstr_app_start_stop(&led.line_write.sval, &led.line_prep.sval, led.line_prep.zone_stop, led.line_prep.sval.len);
}

//-----------------------------------------------
// LED functions
//-----------------------------------------------

void led_fn_impl_register(led_fn_t* pfunc) {
    // register is a passtrough function, line stays unchanged
    led_line_cpy(&led.line_write, &led.line_prep);

    pcre2_match_data* match_data = pcre2_match_data_create_from_pattern(pfunc->regex, NULL);
    int rc = pcre2_match(pfunc->regex, (PCRE2_SPTR)lstr_str(&led.line_prep.sval), lstr_len(&led.line_prep.sval), 0, 0, match_data, NULL);
    PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(match_data);
    led_debug("match_count %d ", rc);

    if (pfunc->arg_count > 0) {
        // usecase with fixed register ID argument
        size_t ir = pfunc->arg[0].uval;
        led_assert(ir < LED_REG_MAX, LED_ERR_ARG, "Register ID %lu exeed maximum register ID %d", ir, LED_REG_MAX-1);
        if( rc > 0) {
            int iv = (rc - 1) * 2;
            led_debug("match_offset values %d %d", ovector[iv], ovector[iv+1]);
            led_line_init(&led.line_reg[ir]);
            lstr_app_start_stop(&led.line_reg[ir].sval, &led.line_prep.sval, ovector[iv], ovector[iv+1]);
            led_debug("register value %d (%s)", ir, lstr_str(&led.line_reg[ir].sval));
        }
    }
    else {
        // usecase with unfixed register ID, catch all groups and distribute into registers, R0 is the global matching zone
        for (int ir = 0; ir < rc && ir < LED_REG_MAX; ir++) {
            int iv = ir * 2;
            led_debug("match_offset values %d %d", ovector[iv], ovector[iv+1]);
            led_line_init(&led.line_reg[ir]);
            lstr_app_start_stop(&led.line_reg[ir].sval, &led.line_prep.sval, ovector[iv], ovector[iv+1]);
            led_debug("register value %d (%s)", ir, lstr_str(&led.line_reg[ir].sval));
        }
    }
    pcre2_match_data_free(match_data);
}

void led_fn_impl_register_recall(led_fn_t* pfunc) {
    size_t ir = pfunc->arg_count > 0 ? pfunc->arg[0].uval : 0;
    led_assert(ir < LED_REG_MAX, LED_ERR_ARG, "Register ID %lu exeed maximum register ID %d", ir, LED_REG_MAX-1);

    if (led_line_isinit(&led.line_reg[ir])) {
        led.line_reg[ir].zone_start = led.line_reg[ir].zone_stop = lstr_len(&led.line_reg[ir].sval);
        lstr_match_offset(&led.line_reg[ir].sval, pfunc->regex, &led.line_reg[ir].zone_start, &led.line_reg[ir].zone_stop);
        led_line_init(&led.line_write);
        lstr_app_start_stop(&led.line_write.sval, &led.line_reg[ir].sval, led.line_reg[ir].zone_start, led.line_reg[ir].zone_stop);
    }
    else {
        // no change to current line if register is not init
        led_line_cpy(&led.line_write, &led.line_prep);
    }
}

void led_fn_helper_substitute(led_fn_t* pfunc, lstr* sinput, lstr* soutput) {
    lstr_decl(rsval, LED_BUF_MAX);
    led_debug("Replace registers in substitute string (len=%d) %s", lstr_len(&pfunc->arg[0].sval), lstr_str(&pfunc->arg[0].sval));

    for (size_t i = 0; i < lstr_len(&pfunc->arg[0].sval); i++) {
        if (lstr_isfull(&rsval)) break;
        if (lstr_startswith_str_at(&pfunc->arg[0].sval, "$R", i)) {
            size_t ir = 0;
            size_t in = i+2; // position of of register ID if given.
            if ( in < lstr_len(&pfunc->arg[0].sval) && lstr_char_at(&pfunc->arg[0].sval, in) >= '0' && lstr_char_at(&pfunc->arg[0].sval, in) <= '9' )
                ir = lstr_char_at(&pfunc->arg[0].sval, in++) - '0';
            else
                in--; // only $R is given, no ID, adjust "in".
            led_debug("Replace register %d found at %d", ir, i);
            for (size_t i = 0; i < lstr_len(&led.line_reg[ir].sval); i++) {
                char c = lstr_char_at(&led.line_reg[ir].sval, i);
                if (c == '\\') // double anti slash to make it a true character
                    lstr_app_char(&rsval, c);
                lstr_app_char(&rsval, c);
            }
            i = in; // position "i" at end of register mark
        }
        else {
            lstr_app_char(&rsval, lstr_char_at(&pfunc->arg[0].sval, i));
        }
    }

    led_debug("Substitute input line (len=%d) to rsval (len=%d)", lstr_len(sinput), lstr_len(&rsval));
    PCRE2_SIZE len = lstr_size(soutput);
    int rc = pcre2_substitute(
                pfunc->regex,
                (PCRE2_UCHAR8*)lstr_str(sinput),
                lstr_len(sinput),
                0,
                PCRE2_SUBSTITUTE_EXTENDED|PCRE2_SUBSTITUTE_GLOBAL,
                NULL,
                NULL,
                (PCRE2_UCHAR8*)lstr_str(&rsval),
                lstr_len(&rsval),
                (PCRE2_UCHAR8*)lstr_str(soutput),
                &len);
    led_assert_pcre(rc);
    soutput->len = len;
}

void led_fn_impl_substitute(led_fn_t* pfunc) {
    led_fn_helper_substitute(pfunc, &led.line_prep.sval, lstr_init_buf(&led.line_write.sval,led.line_write.buf));
}

void led_fn_impl_delete(led_fn_t* pfunc) {
    led_zone_pre_process(pfunc);

    if (led.line_prep.zone_start == 0 && led.line_prep.zone_stop == lstr_len(&led.line_prep.sval))
        // delete all the line if it all match
        led_line_reset(&led.line_write);
    else
        // only remove matching zone
        led_zone_post_process();
}

void led_fn_impl_delete_blank(led_fn_t*) {
    if (lstr_isempty(&led.line_prep.sval) || lstr_isblank(&led.line_prep.sval))
        led_line_reset(&led.line_write);
    else
        lstr_cpy(&led.line_write.sval, &led.line_prep.sval);
}

void led_fn_impl_insert(led_fn_t* pfunc) {
    lstr_decl(newline, LED_BUF_MAX);
    led_fn_helper_substitute(pfunc, &led.line_prep.sval, &newline);

    lstr_init_buf(&led.line_write.sval,led.line_write.buf);
    size_t n = pfunc->arg_count > 1 ? pfunc->arg[1].uval : 1;
    for (size_t i = 0; i < n; i++) {
        lstr_app(&led.line_write.sval, &newline);
        lstr_app_char(&led.line_write.sval, '\n');
    }
    lstr_app(&led.line_write.sval, &led.line_prep.sval);
}

void led_fn_impl_append(led_fn_t* pfunc) {
    lstr_decl(newline, LED_BUF_MAX);
    led_fn_helper_substitute(pfunc, &led.line_prep.sval, &newline);

    lstr_init_buf(&led.line_write.sval,led.line_write.buf);
    lstr_cpy(&led.line_write.sval, &led.line_prep.sval);
    size_t n = pfunc->arg_count > 1 ? pfunc->arg[1].uval : 1;
    for (size_t i = 0; i < n; i++) {
        lstr_app_char(&led.line_write.sval, '\n');
        lstr_app(&led.line_write.sval, &newline);;
    }
}

void led_fn_impl_range_sel(led_fn_t* pfunc) {
    led_line_init(&led.line_write);

    if (lstr_iscontent(&pfunc->arg[0].sval)) {
        long val = pfunc->arg[0].val;
        size_t uval = pfunc->arg[0].uval;
        if (val > 0)
            led.line_prep.zone_start = uval > lstr_len(&led.line_prep.sval) ? lstr_len(&led.line_prep.sval) : uval;
        else
            led.line_prep.zone_start = uval > lstr_len(&led.line_prep.sval) ? 0 : lstr_len(&led.line_prep.sval) - uval;
    }
    if (lstr_iscontent(&pfunc->arg[1].sval)) {
        size_t uval = pfunc->arg[1].uval;
        led.line_prep.zone_stop = led.line_prep.zone_start + uval > lstr_len(&led.line_prep.sval) ? lstr_len(&led.line_prep.sval) : led.line_prep.zone_start + uval;
    }
    else
        led.line_prep.zone_stop = lstr_len(&led.line_prep.sval);

    led_line_append_zone(&led.line_write, &led.line_prep);
}

void led_fn_impl_range_unsel(led_fn_t* pfunc) {
    if (lstr_iscontent(&pfunc->arg[0].sval)) {
        long val = pfunc->arg[0].val;
        size_t uval = pfunc->arg[0].uval;
        if (val > 0) {
            led.line_prep.zone_start = uval > lstr_len(&led.line_prep.sval) ? lstr_len(&led.line_prep.sval) : uval;
        }
        else {
            led.line_prep.zone_start = uval > lstr_len(&led.line_prep.sval) ? 0 : lstr_len(&led.line_prep.sval) - uval;
        }
    }
    if (lstr_iscontent(&pfunc->arg[1].sval)) {
        size_t uval = (size_t)pfunc->arg[1].val;
        led.line_prep.zone_stop = led.line_prep.zone_start + uval > lstr_len(&led.line_prep.sval) ? lstr_len(&led.line_prep.sval) : led.line_prep.zone_start + uval;
    }
    else
        led.line_prep.zone_stop = lstr_len(&led.line_prep.sval);

    led_line_append_before_zone(&led.line_write, &led.line_prep);
    led_line_append_after_zone(&led.line_write, &led.line_prep);
}

void led_fn_impl_translate(led_fn_t* pfunc) {
    led_zone_pre_process(pfunc);

    for (size_t i = led.line_prep.zone_start; i < led.line_prep.zone_stop; i++) {
        char c = lstr_char_at(&led.line_prep.sval, i);
        size_t j;
        for (j = 0; j < lstr_len(&pfunc->arg[0].sval); j++) {
            if (lstr_char_at(&pfunc->arg[0].sval, j) == c) {
                if (j < lstr_len(&pfunc->arg[1].sval))
                    lstr_app_char(&led.line_write.sval, lstr_char_at(&pfunc->arg[1].sval, j));
                break;
            }
        }

        /* output only if no substitution has been done */
        if (j == lstr_len(&pfunc->arg[0].sval))
            lstr_app_char(&led.line_write.sval, c);
    }
    led_zone_post_process();
}

void led_fn_impl_case_lower(led_fn_t* pfunc) {
    led_zone_pre_process(pfunc);

    for (size_t i=led.line_prep.zone_start; i<led.line_prep.zone_stop; i++)
        lstr_app_char(&led.line_write.sval, tolower(lstr_char_at(&led.line_prep.sval, i)));

    led_zone_post_process();
}

void led_fn_impl_case_upper(led_fn_t* pfunc) {
    led_zone_pre_process(pfunc);

    for (size_t i=led.line_prep.zone_start; i<led.line_prep.zone_stop; i++)
        lstr_app_char(&led.line_write.sval, toupper(lstr_char_at(&led.line_prep.sval, i)));

    led_zone_post_process();
}

void led_fn_impl_case_first(led_fn_t* pfunc) {
    led_zone_pre_process(pfunc);

    lstr_app_char(&led.line_write.sval, toupper(lstr_char_at(&led.line_prep.sval, led.line_prep.zone_start)));
    for (size_t i = led.line_prep.zone_start + 1; i<led.line_prep.zone_stop; i++)
        lstr_app_char(&led.line_write.sval, tolower(lstr_char_at(&led.line_prep.sval, i)));

    led_zone_post_process();
}

void led_fn_impl_case_camel(led_fn_t* pfunc) {
    led_zone_pre_process(pfunc);

    int wasword = FALSE;
    for (size_t i=led.line_prep.zone_start; i<led.line_prep.zone_stop; i++) {
        int c = lstr_char_at(&led.line_prep.sval, i);
        int isword = isalnum(c) || c == '_';
        if (isword) {
            if (wasword) lstr_app_char(&led.line_write.sval, tolower(c));
            else lstr_app_char(&led.line_write.sval, toupper(c));
        }
        wasword = isword;
    }

    led_zone_post_process();
}

void led_fn_impl_case_snake(led_fn_t* pfunc) {
    led_zone_pre_process(pfunc);

    for (size_t i=led.line_prep.zone_start; i<led.line_prep.zone_stop; i++) {
        char c = lstr_char_at(&led.line_prep.sval, i);
        char lc = lstr_last_char(&led.line_write.sval);
        if (isalnum(c))
            lstr_app_char(&led.line_write.sval, tolower(c));
        else if (lc != '_')
            lstr_app_char(&led.line_write.sval, '_');
    }

    led_zone_post_process();
}

void led_fn_impl_quote_base(led_fn_t* pfunc, char q) {
    led_zone_pre_process(pfunc);

    if (! (lstr_char_at(&led.line_prep.sval, led.line_prep.zone_start) == q && lstr_char_at(&led.line_prep.sval, led.line_prep.zone_stop - 1) == q) ) {
        led_debug("quote active");
        lstr_app_char(&led.line_write.sval, q);
        led_line_append_zone(&led.line_write, &led.line_prep);
        lstr_app_char(&led.line_write.sval, q);
    }
    else
        led_line_append_zone(&led.line_write, &led.line_prep);

    led_zone_post_process();
}

void led_fn_impl_quote_simple(led_fn_t* pfunc) { led_fn_impl_quote_base(pfunc, '\''); }
void led_fn_impl_quote_double(led_fn_t* pfunc) { led_fn_impl_quote_base(pfunc, '"'); }
void led_fn_impl_quote_back(led_fn_t* pfunc) { led_fn_impl_quote_base(pfunc, '`'); }

void led_fn_impl_quote_remove(led_fn_t* pfunc) {
    const char* QUOTES="'\"`";
    led_zone_pre_process(pfunc);

    char q = QUOTES[0];
    for(size_t i = 0; q != '\0'; i++, q = QUOTES[i]) {
        if (lstr_char_at(&led.line_prep.sval, led.line_prep.zone_start) == q && lstr_char_at(&led.line_prep.sval, led.line_prep.zone_stop - 1) == q) break;
    }

    if (q) {
        led_debug("quotes found: %c", q);
        lstr_app_start_stop(&led.line_write.sval, &led.line_prep.sval, led.line_prep.zone_start + 1, led.line_prep.zone_stop - 1);
    }
    else
        led_line_append_zone(&led.line_write, &led.line_prep);

    led_zone_post_process();
}

void led_fn_impl_trim(led_fn_t* pfunc) {
    led_zone_pre_process(pfunc);

    size_t str_start = led.line_prep.zone_start;
    size_t str_stop = led.line_prep.zone_stop;
    for (; str_start < led.line_prep.zone_stop; str_start++) {
        if (!isspace(lstr_char_at(&led.line_prep.sval, str_start))) break;
    }
    for (; str_stop > str_start; str_stop--) {
        if (!isspace(lstr_char_at(&led.line_prep.sval, str_stop - 1))) break;
    }
    lstr_app_start_stop(&led.line_write.sval, &led.line_prep.sval, str_start, str_stop);

    led_zone_post_process();
}

void led_fn_impl_trim_left(led_fn_t* pfunc) {
    led_zone_pre_process(pfunc);

    size_t str_start = led.line_prep.zone_start;
    for (; str_start < led.line_prep.zone_stop; str_start++) {
        if (!isspace(lstr_char_at(&led.line_prep.sval, str_start))) break;
    }
    lstr_app_start_stop(&led.line_write.sval, &led.line_prep.sval, str_start, led.line_prep.zone_stop);

    led_zone_post_process();
}

void led_fn_impl_trim_right(led_fn_t* pfunc) {
    led_zone_pre_process(pfunc);

    size_t str_stop = led.line_prep.zone_stop;
    for (; str_stop > led.line_prep.zone_start; str_stop--)
        if (!isspace(lstr_char_at(&led.line_prep.sval, str_stop - 1))) break;
    lstr_app_start_stop(&led.line_write.sval, &led.line_prep.sval, led.line_prep.zone_start, str_stop);

    led_zone_post_process();
}

void led_fn_impl_base64_encode(led_fn_t* pfunc) {
    led_zone_pre_process(pfunc);

    char b64buf[LED_BUF_MAX];
    base64_encodestate base64_state;
	size_t count = 0;

	base64_init_encodestate(&base64_state);
	count = base64_encode_block(
        lstr_str_at(&led.line_prep.sval, led.line_prep.zone_start),
        led.line_prep.zone_stop - led.line_prep.zone_start,
        b64buf,
        &base64_state);
	count += base64_encode_blockend(
        b64buf + count,
        &base64_state);
    // remove newline and final 0
    b64buf[count - 1] = '\0';

    lstr_app_str(&led.line_write.sval, b64buf);
    led_zone_post_process();
}

void led_fn_impl_base64_decode(led_fn_t* pfunc) {
    led_zone_pre_process(pfunc);

    char b64buf[LED_BUF_MAX];
	base64_decodestate base64_state;
	size_t count = 0;

	base64_init_decodestate(&base64_state);
	count = base64_decode_block(
        lstr_str_at(&led.line_prep.sval, led.line_prep.zone_start),
        led.line_prep.zone_stop - led.line_prep.zone_start,
        b64buf,
        &base64_state);
    b64buf[count] = '\0';

    lstr_app_str(&led.line_write.sval, b64buf);
    led_zone_post_process();
}

void led_fn_impl_url_encode(led_fn_t* pfunc) {
    led_zone_pre_process(pfunc);

    const char *HEX = "0123456789ABCDEF";
    char pcbuf[4] = "%00";

    for (size_t i = led.line_prep.zone_start; i < led.line_prep.zone_stop; i++) {
        char c = lstr_char_at(&led.line_prep.sval, i);
        if (isalnum(c))
            lstr_app_char(&led.line_write.sval, c);
        else {
            pcbuf[1] = HEX[(c >> 4) & 0x0F];
            pcbuf[2] = HEX[c & 0x0F];
            lstr_app_str(&led.line_write.sval, pcbuf);
        }
    }

    led_zone_post_process();
}

const char fname_stdchar_table[] = "/._-~:=%";

void led_fn_impl_shell_escape(led_fn_t* pfunc) {
    lstr_decl_str(table, fname_stdchar_table);
    led_zone_pre_process(pfunc);

    for (size_t i = led.line_prep.zone_start; i < led.line_prep.zone_stop; i++) {
        char c = lstr_char_at(&led.line_prep.sval, i);
        if (isalnum(c) || lstr_ischar(&table, c))
            lstr_app_char(&led.line_write.sval, c);
        else {
            lstr_app_char(&led.line_write.sval, '\\');
            lstr_app_char(&led.line_write.sval, c);
        }
    }

    led_zone_post_process();
}

void led_fn_impl_shell_unescape(led_fn_t* pfunc) {
    led_zone_pre_process(pfunc);

    for (size_t i = led.line_prep.zone_start; i < led.line_prep.zone_stop; i++) {
        int wasesc = FALSE;
        char c = lstr_char_at(&led.line_prep.sval, i);
        if (!wasesc && c == '\\')
            wasesc = TRUE;
        else
            lstr_app_char(&led.line_write.sval, c);
    }

    led_zone_post_process();
}

void led_fn_impl_realpath(led_fn_t* pfunc) {
    led_zone_pre_process(pfunc);

    char c = led.line_prep.buf[led.line_prep.zone_stop]; // temporary save this char for realpath function
    led.line_prep.buf[led.line_prep.zone_stop] = '\0';
    if (realpath(lstr_str_at(&led.line_prep.sval, led.line_prep.zone_start), led.line_write.buf + lstr_len(&led.line_write.sval)) != NULL ) {
        led.line_prep.buf[led.line_prep.zone_stop] = c;
        lstr_init_buf(&led.line_write.sval, led.line_write.buf);
    }
    else {
        led.line_prep.buf[led.line_prep.zone_stop] = c;
        led_line_append_zone(&led.line_write, &led.line_prep);
    }

    led_zone_post_process();
}

void led_fn_impl_dirname(led_fn_t* pfunc) {
    led_zone_pre_process(pfunc);

    const char* dir = dirname(lstr_str_at(&led.line_prep.sval, led.line_prep.zone_start));
    if (dir != NULL) lstr_app_str(&led.line_write.sval, dir);
    else led_line_append_zone(&led.line_write, &led.line_prep);

    led_zone_post_process();
}

void led_fn_impl_basename(led_fn_t* pfunc) {
    led_zone_pre_process(pfunc);

    const char* fname = basename(lstr_str_at(&led.line_prep.sval, led.line_prep.zone_start));
    if (fname != NULL) lstr_app_str(&led.line_write.sval, fname);
    else led_line_append_zone(&led.line_write, &led.line_prep);

    led_zone_post_process();
}

void led_fn_impl_revert(led_fn_t* pfunc) {
    led_zone_pre_process(pfunc);

    for (size_t i = led.line_prep.zone_stop; i > led.line_prep.zone_start; i--)
        lstr_app_char(&led.line_write.sval, lstr_char_at(&led.line_prep.sval, i - 1));

    led_zone_post_process();
}

void led_fn_impl_field_base(led_fn_t* pfunc, const char* field_sep) {
    lstr_decl_str(sepsval, field_sep);
    led_zone_pre_process(pfunc);
    size_t field_n = pfunc->arg[0].uval;
    size_t n = 0;
    int was_sep = TRUE;
    size_t str_start = led.line_prep.zone_start;
    size_t str_stop = led.line_prep.zone_stop;
    for (; str_start < led.line_prep.zone_stop; str_start++ ) {
        int is_sep = lstr_ischar(&sepsval, lstr_char_at(&led.line_prep.sval, str_start));
        if (was_sep && !is_sep) {
            n++;
            if (n == field_n) break;
        }
        was_sep = is_sep;
    }
    if (n == field_n) {
        was_sep = FALSE;
        for (str_stop = str_start; str_stop < led.line_prep.zone_stop; str_stop++ ) {
            int is_sep = lstr_ischar(&sepsval, lstr_char_at(&led.line_prep.sval, str_stop));
            if (!was_sep && is_sep) break;
        }
        lstr_app_start_stop(&led.line_write.sval, &led.line_prep.sval, str_start, str_stop);
    }

    led_zone_post_process();
}

void led_fn_impl_field(led_fn_t* pfunc) { led_fn_impl_field_base(pfunc, lstr_str(&pfunc->arg[1].sval)); }
void led_fn_impl_field_csv(led_fn_t* pfunc) { led_fn_impl_field_base(pfunc, ",;"); }
void led_fn_impl_field_space(led_fn_t* pfunc) { led_fn_impl_field_base(pfunc, " \t\n"); }
void led_fn_impl_field_mixed(led_fn_t* pfunc) { led_fn_impl_field_base(pfunc, ",; \t\n"); }

void led_fn_impl_join(led_fn_t*) {
   for (size_t i = 0; i < lstr_len(&led.line_prep.sval); i++) {
        char c = lstr_char_at(&led.line_prep.sval, i);
        if ( c != '\n') lstr_app_char(&led.line_write.sval, c);
   }
}

void led_fn_impl_split_base(led_fn_t* pfunc, const char* field_sep) {
    lstr_decl_str(sepsval, field_sep);
    led_zone_pre_process(pfunc);

    for (size_t i = led.line_prep.zone_start; i < led.line_prep.zone_stop; i++) {
        char c = lstr_char_at(&led.line_prep.sval, i);
        if ( lstr_ischar(&sepsval, c) ) c = '\n';
        lstr_app_char(&led.line_write.sval, c);
    }
    led_zone_post_process();
}

void led_fn_impl_split(led_fn_t* pfunc) { led_fn_impl_split_base(pfunc, lstr_str(&pfunc->arg[0].sval)); }
void led_fn_impl_split_space(led_fn_t* pfunc) { led_fn_impl_split_base(pfunc, " \t\n"); }
void led_fn_impl_split_csv(led_fn_t* pfunc) { led_fn_impl_split_base(pfunc, ",;"); }
void led_fn_impl_split_mixed(led_fn_t* pfunc) { led_fn_impl_split_base(pfunc, ",; \t\n"); }

void led_fn_impl_randomize_base(led_fn_t* pfunc, const char* charset, size_t len) {
    led_zone_pre_process(pfunc);

    for (size_t i = led.line_prep.zone_start; i < led.line_prep.zone_stop; i++) {
        char c = charset[rand() % len];
        lstr_app_char(&led.line_write.sval, c);
    }

    led_zone_post_process();
}

const char randomize_table_num[] = "0123456789";
const char randomize_table_alpha[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
const char randomize_table_alnum[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
const char randomize_table_hexa[] = "0123456789ABCDEF";
const char randomize_table_mixed[] = "0123456789-_/=!:;,~#$*?%abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

void led_fn_impl_randomize_num(led_fn_t* pfunc) { led_fn_impl_randomize_base(pfunc, randomize_table_num, sizeof randomize_table_num - 1); }
void led_fn_impl_randomize_alpha(led_fn_t* pfunc) { led_fn_impl_randomize_base(pfunc, randomize_table_alpha, sizeof randomize_table_alpha -1); }
void led_fn_impl_randomize_alnum(led_fn_t* pfunc) { led_fn_impl_randomize_base(pfunc, randomize_table_alnum, sizeof randomize_table_alnum - 1); }
void led_fn_impl_randomize_hexa(led_fn_t* pfunc) { led_fn_impl_randomize_base(pfunc, randomize_table_hexa, sizeof randomize_table_hexa - 1); }
void led_fn_impl_randomize_mixed(led_fn_t* pfunc) { led_fn_impl_randomize_base(pfunc, randomize_table_mixed, sizeof randomize_table_mixed - 1); }

size_t led_fn_helper_fname_pos() {
    size_t iname = lstr_rfind_char_start_stop(&led.line_prep.sval, '/', led.line_prep.zone_start, led.line_prep.zone_stop);
    if (iname == lstr_len(&led.line_prep.sval)) iname = led.line_prep.zone_start;
    else iname++;
    led_debug("led_fn_helper_fname_pos iname: %u %s", iname, lstr_str_at(&led.line_prep.sval, iname));
    return iname;
}

void led_fn_impl_fname_lower(led_fn_t* pfunc) {
    led_zone_pre_process(pfunc);

    if (led.line_prep.zone_start < led.line_prep.zone_stop) {
        size_t iname = led_fn_helper_fname_pos();
        lstr_app_start_stop(&led.line_write.sval, &led.line_prep.sval, led.line_prep.zone_start, iname);

        for (; iname < led.line_prep.zone_stop; iname++) {
            char c = lstr_char_at(&led.line_prep.sval, iname);
            if (isalnum(c))
                lstr_app_char(&led.line_write.sval, tolower(c));
            else if (c == '.') {
                if (isalnum(lstr_last_char(&led.line_write.sval))) lstr_app_char(&led.line_write.sval, c);
                else lstr_set_last_char(&led.line_write.sval, c);
            }
            else {
                if (isalnum(lstr_last_char(&led.line_write.sval))) lstr_app_char(&led.line_write.sval, '_');
            }
        }
    }
    led_zone_post_process();
}

void led_fn_impl_fname_upper(led_fn_t* pfunc) {
    led_debug("led_fn_impl_fname_upper");
    led_zone_pre_process(pfunc);

    if (led.line_prep.zone_start < led.line_prep.zone_stop) {
        size_t iname = led_fn_helper_fname_pos();
        lstr_app_start_stop(&led.line_write.sval, &led.line_prep.sval, led.line_prep.zone_start, iname);

        for (; iname < led.line_prep.zone_stop; iname++) {
            char c = lstr_char_at(&led.line_prep.sval, iname);
            if (isalnum(c))
                lstr_app_char(&led.line_write.sval, toupper(c));
            else if (c == '.') {
                if (isalnum(lstr_last_char(&led.line_write.sval))) lstr_app_char(&led.line_write.sval, c);
                else lstr_set_last_char(&led.line_write.sval, c);
            }
            else {
                if (isalnum(lstr_last_char(&led.line_write.sval))) lstr_app_char(&led.line_write.sval, '_');
            }
        }
    }
    led_zone_post_process();
}

void led_fn_impl_fname_camel(led_fn_t* pfunc) {
    led_zone_pre_process(pfunc);

    if (led.line_prep.zone_start < led.line_prep.zone_stop) {
        size_t iname = led_fn_helper_fname_pos();
        lstr_app_start_stop(&led.line_write.sval, &led.line_prep.sval, led.line_prep.zone_start, iname);

        int wasword = TRUE;
        int isfirst = TRUE;
        for (; iname < led.line_prep.zone_stop; iname++) {
            char c = lstr_char_at(&led.line_prep.sval, iname);
            if (isalnum(lstr_last_char(&led.line_write.sval)) && c == '.') {
                lstr_app_char(&led.line_write.sval, c);
                isfirst = TRUE;
            }
            else {
                int isword = isalnum(c);
                if (isword) {
                    if (wasword || isfirst) lstr_app_char(&led.line_write.sval, tolower(c));
                    else lstr_app_char(&led.line_write.sval, toupper(c));
                    isfirst = FALSE;
                }
                wasword = isword;
            }
        }
    }
    led_zone_post_process();
}

void led_fn_impl_fname_snake(led_fn_t* pfunc) {
    led_zone_pre_process(pfunc);

    if (led.line_prep.zone_start < led.line_prep.zone_stop) {
        size_t iname = led_fn_helper_fname_pos();
        lstr_app_start_stop(&led.line_write.sval, &led.line_prep.sval, led.line_prep.zone_start, iname);

        for (; iname < led.line_prep.zone_stop; iname++) {
            char c = lstr_char_at(&led.line_prep.sval, iname);
            char lc = lstr_last_char(&led.line_write.sval);
            if (isalnum(c))
                lstr_app_char(&led.line_write.sval, tolower(c));
            else if (c == '.') {
                lstr_unapp_char(&led.line_write.sval, '.');
                lstr_unapp_char(&led.line_write.sval, '_');
                lstr_app_char(&led.line_write.sval, '.');
            }
            else if (lc != '\0' && lc != '.') {
                lstr_unapp_char(&led.line_write.sval, '_');
                lstr_app_char(&led.line_write.sval, '_');
            }
        }
    }
    led_zone_post_process();
}

void led_fn_impl_generate(led_fn_t* pfunc) {
    led_zone_pre_process(pfunc);
    size_t n = pfunc->arg[1].uval > 0 ? led.line_prep.zone_start + pfunc->arg[1].uval : led.line_prep.zone_stop;
    char c = lstr_first_char(&pfunc->arg[0].sval);

    for (size_t i = led.line_prep.zone_start; i < n; i++) {
        lstr_app_char(&led.line_write.sval, c);
    }

    led_zone_post_process();
}

led_fn_desc_t LED_FN_TABLE[] = {
    { "s", "substitute", &led_fn_impl_substitute, "Ss", "Substitute", "s/[regex]/replace[/opts]" },
    { "d", "delete", &led_fn_impl_delete, "", "Delete line", "d/" },
    { "i", "insert", &led_fn_impl_insert, "Sp", "Insert line", "i/[regex]/<string>[/N]" },
    { "a", "append", &led_fn_impl_append, "Sp", "Append line", "a/[regex]/<string>[/N]" },
    { "j", "join", &led_fn_impl_join, "", "Join lines (only with pack mode)", "j/" },
    { "db", "delete_blank", &led_fn_impl_delete_blank, "", "Delete blank/empty lines", "db/" },
    { "tr", "translate", &led_fn_impl_translate, "SS", "Translate", "tr/[regex]/chars/chars" },
    { "cl", "case_lower", &led_fn_impl_case_lower, "", "Case to lower", "cl/[regex]" },
    { "cu", "case_upper", &led_fn_impl_case_upper, "", "Case to upper", "cu/[regex]" },
    { "cf", "case_first", &led_fn_impl_case_first, "", "Case first upper", "cf/[regex]" },
    { "cc", "case_camel", &led_fn_impl_case_camel, "", "Case to camel style", "cc/[regex]" },
    { "cs", "case_snake", &led_fn_impl_case_snake, "", "Case to snake style", "cs/[regex]" },
    { "qt", "quote_simple", &led_fn_impl_quote_simple, "", "Quote simple", "qt/[regex]" },
    { "qtd", "quote_double", &led_fn_impl_quote_double, "", "Quote double", "qd/[regex]" },
    { "qtb", "quote_back", &led_fn_impl_quote_back, "", "Quote back", "qb/[regex]" },
    { "qtr", "quote_remove", &led_fn_impl_quote_remove, "", "Quote remove", "qr/[regex]" },
    { "sp", "split", &led_fn_impl_split, "", "Split using characters", "sp/[regex]/chars" },
    { "spc", "split_csv", &led_fn_impl_split_csv, "", "Split using comma", "spc/[regex]" },
    { "sps", "split_space", &led_fn_impl_split_space, "", "Split using space", "sps/[regex]" },
    { "spm", "split_mixed", &led_fn_impl_split_mixed, "", "Split using comma and space", "spm/[regex]" },
    { "tm", "trim", &led_fn_impl_trim, "", "Trim", "tm/[regex]" },
    { "tml", "trim_left", &led_fn_impl_trim_left, "", "Trim left", "tml/[regex]" },
    { "tmr", "trim_right", &led_fn_impl_trim_right, "", "Trim right", "tmr/[regex]" },
    { "rv", "revert", &led_fn_impl_revert, "", "Revert", "rv/[regex]" },
    { "fld", "field", &led_fn_impl_field, "PSp", "Extract field with separator chars", "fld/[regex]/N/sep[/count]" },
    { "fls", "field_space", &led_fn_impl_field_space, "Pp", "Extract field separated by space", "fls/[regex]/N[/count]" },
    { "flc", "field_csv", &led_fn_impl_field_csv, "Pp", "Extract field separated by comma", "flc/[regex]/N[/count]" },
    { "flm", "field_mixed", &led_fn_impl_field_mixed, "Pp", "Extract field separated by space or comma", "flm/[regex]/N[/count]" },
    { "b64e", "base64_encode", &led_fn_impl_base64_encode, "", "Encode base64", "b64e/[regex]" },
    { "b64d", "base64_decode", &led_fn_impl_base64_decode, "", "Decode base64", "b64d/[regex]" },
    { "urle", "url_encode", &led_fn_impl_url_encode, "", "Encode URL", "urle/[regex]" },
    { "she", "shell_escape", &led_fn_impl_shell_escape, "", "Shell encode", "she/[regex]" },
    { "shu", "shell_unescape", &led_fn_impl_shell_unescape, "", "Shell decode", "shu/[regex]" },
    { "rp", "realpath", &led_fn_impl_realpath, "", "Convert to real path (canonical)", "rp/[regex]" },
    { "dn", "dirname", &led_fn_impl_dirname, "", "Extract last dir of the path", "dn/[regex]" },
    { "bn", "basename", &led_fn_impl_basename, "", "Extract file of the path", "bn/[regex]" },
    { "fnl", "fname_lower", &led_fn_impl_fname_lower, "", "simplify file name using lower case", "fnl/[regex]" },
    { "fnu", "fname_upper", &led_fn_impl_fname_upper, "", "simplify file name using upper case", "fnu/[regex]" },
    { "fnc", "fname_camel", &led_fn_impl_fname_camel, "", "simplify file name using camel case", "fnc/[regex]" },
    { "fns", "fname_snake", &led_fn_impl_fname_snake, "", "simplify file name using snake case", "fnc/[regex]" },
    { "rzn", "randomize_num", &led_fn_impl_randomize_num, "", "Randomize numeric values", "rzn/[regex]" },
    { "rza", "randomize_alpha", &led_fn_impl_randomize_alpha, "", "Randomize alpha values", "rza/[regex]" },
    { "rzan", "randomize_alnum", &led_fn_impl_randomize_alnum, "", "Randomize alpha numeric values", "rzan/[regex]" },
    { "rzh", "randomize_hexa", &led_fn_impl_randomize_hexa, "", "Randomize alpha numeric values", "rzh/[regex]" },
    { "rzm", "randomize_mixed", &led_fn_impl_randomize_mixed, "", "Randomize alpha numeric and custom chars", "rzm/[regex]" },
    { "gen", "generate", &led_fn_impl_generate, "Sp", "Generate chars", "gen/[regex]/<char>[/N]" },
    { "rn", "range_sel", &led_fn_impl_range_sel, "Np", "Range select", "rn/[regex]/start[/count]" },
    { "rnu", "range_unsel", &led_fn_impl_range_unsel, "Np", "Range unselect", "rnu/[regex]/start[/count]" },
    { "r", "register", &led_fn_impl_register, "p", "Register line content", "r/[regex][/N]" },
    { "rr", "register_recall", &led_fn_impl_register_recall, "p", "Register recall to line", "rr/[regex][/N]" },
};

#define LED_FN_TABLE_MAX sizeof(LED_FN_TABLE)/sizeof(led_fn_desc_t)

led_fn_desc_t* led_fn_table_descriptor(size_t id) {
    led_assert(id < LED_FN_TABLE_MAX, LED_ERR_INTERNAL, "Function index out of table");
    return LED_FN_TABLE + id;
}

size_t led_fn_table_size() {
    return LED_FN_TABLE_MAX;
}
