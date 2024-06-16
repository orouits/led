#include "led.h"

#include <b64/cencode.h>
#include <b64/cdecode.h>

//-----------------------------------------------
// LED functions utilities
//-----------------------------------------------

#define countof(a) (sizeof(a)/sizeof(a[0]))

bool led_zone_pre_process(led_fn_t* pfunc) {
    led_line_init(&led.line_write);

    led.line_prep.zone_start = led.line_prep.zone_stop = led_u8s_len(&led.line_prep.lstr);
    bool rc = led_u8s_match_offset(&led.line_prep.lstr, pfunc->regex, &led.line_prep.zone_start, &led.line_prep.zone_stop);

    if (!led.opt.output_match)
        led_u8s_app_zn(&led.line_write.lstr, &led.line_prep.lstr, 0, led.line_prep.zone_start);

    return rc;
}

void led_zone_post_process() {
    if (!led.opt.output_match)
        led_u8s_app_zn(&led.line_write.lstr, &led.line_prep.lstr, led.line_prep.zone_stop, led.line_prep.lstr.len);
}

//-----------------------------------------------
// LED functions
//-----------------------------------------------

void led_fn_impl_register(led_fn_t* pfunc) {
    // register is a passtrough function, line stays unchanged
    led_line_cpy(&led.line_write, &led.line_prep);

    pcre2_match_data* match_data = pcre2_match_data_create_from_pattern(pfunc->regex, NULL);
    int rc = pcre2_match(pfunc->regex, (PCRE2_SPTR)led_u8s_str(&led.line_prep.lstr), led_u8s_len(&led.line_prep.lstr), 0, 0, match_data, NULL);
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
            led_u8s_app_zn(&led.line_reg[ir].lstr, &led.line_prep.lstr, ovector[iv], ovector[iv+1]);
            led_debug("register value %d (%s)", ir, led_u8s_str(&led.line_reg[ir].lstr));
        }
    }
    else {
        // usecase with unfixed register ID, catch all groups and distribute into registers, R0 is the global matching zone
        for (int ir = 0; ir < rc && ir < LED_REG_MAX; ir++) {
            int iv = ir * 2;
            led_debug("match_offset values %d %d", ovector[iv], ovector[iv+1]);
            led_line_init(&led.line_reg[ir]);
            led_u8s_app_zn(&led.line_reg[ir].lstr, &led.line_prep.lstr, ovector[iv], ovector[iv+1]);
            led_debug("register value %d (%s)", ir, led_u8s_str(&led.line_reg[ir].lstr));
        }
    }
    pcre2_match_data_free(match_data);
}

void led_fn_impl_register_recall(led_fn_t* pfunc) {
    size_t ir = pfunc->arg_count > 0 ? pfunc->arg[0].uval : 0;
    led_assert(ir < LED_REG_MAX, LED_ERR_ARG, "Register ID %lu exeed maximum register ID %d", ir, LED_REG_MAX-1);

    if (led_line_isinit(&led.line_reg[ir])) {
        led.line_reg[ir].zone_start = led.line_reg[ir].zone_stop = led_u8s_len(&led.line_reg[ir].lstr);
        led_u8s_match_offset(&led.line_reg[ir].lstr, pfunc->regex, &led.line_reg[ir].zone_start, &led.line_reg[ir].zone_stop);
        led_line_init(&led.line_write);
        led_u8s_app_zn(&led.line_write.lstr, &led.line_reg[ir].lstr, led.line_reg[ir].zone_start, led.line_reg[ir].zone_stop);
    }
    else {
        // no change to current line if register is not init
        led_line_cpy(&led.line_write, &led.line_prep);
    }
}

void led_fn_helper_substitute(led_fn_t* pfunc, led_u8s_t* sinput, led_u8s_t* soutput) {
    led_u8s_decl(rsval, LED_BUF_MAX);
    led_debug("led_fn_helper_substitute: Replace registers in substitute string (len=%d) %s", led_u8s_len(&pfunc->arg[0].lstr), led_u8s_str(&pfunc->arg[0].lstr));

    size_t i = 0;
    while ( i < led_u8s_len(&pfunc->arg[0].lstr) ) {
        if (led_u8s_isfull(&rsval)) break;
        if (led_u8s_startswith_str_at(&pfunc->arg[0].lstr, "$R", i)) {
            size_t ir = 0;
            size_t in = i+2; // position of of register ID if given.
            if ( in < led_u8s_len(&pfunc->arg[0].lstr) && led_u8c_isdigit(led_u8s_char_at(&pfunc->arg[0].lstr, in)) ) {
                ir = led_u8s_char_at(&pfunc->arg[0].lstr, in++) - '0';
                in++;
            }
            led_debug("led_fn_helper_substitute: Replace register %d found at %d", ir, i);
            size_t j = 0;
            while (j < led_u8s_len(&led.line_reg[ir].lstr)) {
                u8c_t c = led_u8s_char_next(&led.line_reg[ir].lstr, &j);
                if (c == '\\') // double anti slash to make it a true character
                    led_u8s_app_char(&rsval, c);
                led_u8s_app_char(&rsval, c);
            }
            i = in; // position "i" at end of register mark
        }
        else {
            u8c_t c = led_u8s_char_next(&pfunc->arg[0].lstr, &i);
            led_debug("led_fn_helper_substitute: append to rsval %c", c);
            led_u8s_app_char(&rsval, c);
        }
    }

    led_debug("Substitute input line (len=%d) to rsval (len=%d)", led_u8s_len(sinput), led_u8s_len(&rsval));
    PCRE2_SIZE len = led_u8s_size(soutput);
    int rc = pcre2_substitute(
                pfunc->regex,
                (PCRE2_UCHAR8*)led_u8s_str(sinput),
                led_u8s_len(sinput),
                0,
                PCRE2_SUBSTITUTE_EXTENDED|PCRE2_SUBSTITUTE_GLOBAL,
                NULL,
                NULL,
                (PCRE2_UCHAR8*)led_u8s_str(&rsval),
                led_u8s_len(&rsval),
                (PCRE2_UCHAR8*)led_u8s_str(soutput),
                &len);
    led_assert_pcre(rc);
    soutput->len = len;
}

void led_fn_impl_substitute(led_fn_t* pfunc) {
    led_fn_helper_substitute(pfunc, &led.line_prep.lstr, led_u8s_init_buf(&led.line_write.lstr,led.line_write.buf));
}

void led_fn_impl_delete(led_fn_t* pfunc) {
    led_zone_pre_process(pfunc);

    if (led.line_prep.zone_start == 0 && led.line_prep.zone_stop == led_u8s_len(&led.line_prep.lstr))
        // delete all the line if it all match
        led_line_reset(&led.line_write);
    else
        // only remove matching zone
        led_zone_post_process();
}

void led_fn_impl_delete_blank(led_fn_t*) {
    if (led_u8s_isempty(&led.line_prep.lstr) || led_u8s_isblank(&led.line_prep.lstr))
        led_line_reset(&led.line_write);
    else
        led_u8s_cpy(&led.line_write.lstr, &led.line_prep.lstr);
}

void led_fn_impl_insert(led_fn_t* pfunc) {
    led_u8s_decl(newline, LED_BUF_MAX);
    led_fn_helper_substitute(pfunc, &led.line_prep.lstr, &newline);

    led_u8s_init_buf(&led.line_write.lstr, led.line_write.buf);
    led_u8s_empty(&led.line_write.lstr);
    size_t lcount = pfunc->arg_count > 1 ? pfunc->arg[1].uval : 1;
    for (size_t i = 0; i < lcount; i++) {
        led_u8s_app(&led.line_write.lstr, &newline);
        led_u8s_app_char(&led.line_write.lstr, '\n');
    }
    led_u8s_app(&led.line_write.lstr, &led.line_prep.lstr);
}

void led_fn_impl_append(led_fn_t* pfunc) {
    led_u8s_decl(newline, LED_BUF_MAX);
    led_fn_helper_substitute(pfunc, &led.line_prep.lstr, &newline);

    led_u8s_init_buf(&led.line_write.lstr,led.line_write.buf);
    led_u8s_cpy(&led.line_write.lstr, &led.line_prep.lstr);
    size_t lcount = pfunc->arg_count > 1 ? pfunc->arg[1].uval : 1;
    for (size_t i = 0; i < lcount; i++) {
        led_u8s_app_char(&led.line_write.lstr, '\n');
        led_u8s_app(&led.line_write.lstr, &newline);;
    }
}

void led_fn_impl_range_sel(led_fn_t* pfunc) {
    led_line_init(&led.line_write);

    if (led_u8s_iscontent(&pfunc->arg[0].lstr)) {
        long val = pfunc->arg[0].val;
        size_t uval = pfunc->arg[0].uval;
        if (val > 0)
            led.line_prep.zone_start = uval > led_u8s_len(&led.line_prep.lstr) ? led_u8s_len(&led.line_prep.lstr) : uval;
        else
            led.line_prep.zone_start = uval > led_u8s_len(&led.line_prep.lstr) ? 0 : led_u8s_len(&led.line_prep.lstr) - uval;
    }
    if (led_u8s_iscontent(&pfunc->arg[1].lstr)) {
        size_t uval = pfunc->arg[1].uval;
        led.line_prep.zone_stop = led.line_prep.zone_start + uval > led_u8s_len(&led.line_prep.lstr) ? led_u8s_len(&led.line_prep.lstr) : led.line_prep.zone_start + uval;
    }
    else
        led.line_prep.zone_stop = led_u8s_len(&led.line_prep.lstr);

    led_line_append_zone(&led.line_write, &led.line_prep);
}

void led_fn_impl_range_unsel(led_fn_t* pfunc) {
    if (led_u8s_iscontent(&pfunc->arg[0].lstr)) {
        long val = pfunc->arg[0].val;
        size_t uval = pfunc->arg[0].uval;
        if (val > 0) {
            led.line_prep.zone_start = uval > led_u8s_len(&led.line_prep.lstr) ? led_u8s_len(&led.line_prep.lstr) : uval;
        }
        else {
            led.line_prep.zone_start = uval > led_u8s_len(&led.line_prep.lstr) ? 0 : led_u8s_len(&led.line_prep.lstr) - uval;
        }
    }
    if (led_u8s_iscontent(&pfunc->arg[1].lstr)) {
        size_t uval = (size_t)pfunc->arg[1].val;
        led.line_prep.zone_stop = led.line_prep.zone_start + uval > led_u8s_len(&led.line_prep.lstr) ? led_u8s_len(&led.line_prep.lstr) : led.line_prep.zone_start + uval;
    }
    else
        led.line_prep.zone_stop = led_u8s_len(&led.line_prep.lstr);

    led_line_append_before_zone(&led.line_write, &led.line_prep);
    led_line_append_after_zone(&led.line_write, &led.line_prep);
}

void led_fn_impl_translate(led_fn_t* pfunc) {
    led_zone_pre_process(pfunc);
    size_t i = led.line_prep.zone_start;
    while ( i < led.line_prep.zone_stop ) {
        u8c_t c = led_u8s_char_next(&led.line_prep.lstr, &i);
        size_t j=0,k=0;
        bool sub = false;
        while ( j < led_u8s_len(&pfunc->arg[0].lstr) && k < led_u8s_len(&pfunc->arg[1].lstr) ) {
            u8c_t ct = led_u8s_char_next(&pfunc->arg[1].lstr, &k);
            if (led_u8s_char_next(&pfunc->arg[0].lstr, &j) == c) {
                led_u8s_app_char(&led.line_write.lstr, ct);
                sub = true;
                break;
            }
        }
        // if no substitution has been done
        if (!sub)
            led_u8s_app_char(&led.line_write.lstr, c);
    }
    led_zone_post_process();
}

void led_fn_impl_case_lower(led_fn_t* pfunc) {
    led_zone_pre_process(pfunc);

    size_t i = led.line_prep.zone_start;
    while ( i < led.line_prep.zone_stop ) {
        size_t ic = i;
        u8c_t c = led_u8s_char_next(&led.line_prep.lstr, &i);
        led_u8s_app_char(&led.line_write.lstr, led_u8c_tolower(led_u8s_char_at(&led.line_prep.lstr, ic)));
    }

    led_zone_post_process();
}

void led_fn_impl_case_upper(led_fn_t* pfunc) {
    led_zone_pre_process(pfunc);

    size_t i = led.line_prep.zone_start;
    while ( i < led.line_prep.zone_stop ) {
        size_t ic = i;
        u8c_t c = led_u8s_char_next(&led.line_prep.lstr, &i);
        led_u8s_app_char(&led.line_write.lstr, led_u8c_toupper(led_u8s_char_at(&led.line_prep.lstr, ic)));
    }

    led_zone_post_process();
}

void led_fn_impl_case_first(led_fn_t* pfunc) {
    led_zone_pre_process(pfunc);

    led_u8s_app_char(&led.line_write.lstr, toupper(led_u8s_char_at(&led.line_prep.lstr, led.line_prep.zone_start)));
    for (size_t i = led.line_prep.zone_start + 1; i<led.line_prep.zone_stop; i++)
        led_u8s_app_char(&led.line_write.lstr, tolower(led_u8s_char_at(&led.line_prep.lstr, i)));

    led_zone_post_process();
}

void led_fn_impl_case_camel(led_fn_t* pfunc) {
    led_zone_pre_process(pfunc);

    bool wasword = false;
    for (size_t i=led.line_prep.zone_start; i<led.line_prep.zone_stop; i++) {
        int c = led_u8s_char_at(&led.line_prep.lstr, i);
        bool isword = isalnum(c) || c == '_';
        if (isword) {
            if (wasword) led_u8s_app_char(&led.line_write.lstr, tolower(c));
            else led_u8s_app_char(&led.line_write.lstr, toupper(c));
        }
        wasword = isword;
    }

    led_zone_post_process();
}

void led_fn_impl_case_snake(led_fn_t* pfunc) {
    led_zone_pre_process(pfunc);

    for (size_t i=led.line_prep.zone_start; i<led.line_prep.zone_stop; i++) {
        char c = led_u8s_char_at(&led.line_prep.lstr, i);
        char lc = led_u8s_char_last(&led.line_write.lstr);
        if (isalnum(c))
            led_u8s_app_char(&led.line_write.lstr, tolower(c));
        else if (lc != '_')
            led_u8s_app_char(&led.line_write.lstr, '_');
    }

    led_zone_post_process();
}

void led_fn_impl_quote_base(led_fn_t* pfunc, u8c_t q) {
    led_zone_pre_process(pfunc);

    if (! (led_u8s_char_at(&led.line_prep.lstr, led.line_prep.zone_start) == q && led_u8s_char_at(&led.line_prep.lstr, led.line_prep.zone_stop - 1) == q) ) {
        led_debug("quote active");
        led_u8s_app_char(&led.line_write.lstr, q);
        led_line_append_zone(&led.line_write, &led.line_prep);
        led_u8s_app_char(&led.line_write.lstr, q);
    }
    else
        led_line_append_zone(&led.line_write, &led.line_prep);

    led_zone_post_process();
}

void led_fn_impl_quote_simple(led_fn_t* pfunc) { led_fn_impl_quote_base(pfunc, '\''); }
void led_fn_impl_quote_double(led_fn_t* pfunc) { led_fn_impl_quote_base(pfunc, '"'); }
void led_fn_impl_quote_back(led_fn_t* pfunc) { led_fn_impl_quote_base(pfunc, '`'); }

void led_fn_impl_quote_remove(led_fn_t* pfunc) {
    const char* QUOTES = "'\"`";
    led_zone_pre_process(pfunc);

    char q = QUOTES[0];
    for(size_t i = 0; q != '\0'; i++, q = QUOTES[i]) {
        if (led_u8s_char_at(&led.line_prep.lstr, led.line_prep.zone_start) == (u8c_t)q && led_u8s_char_at(&led.line_prep.lstr, led.line_prep.zone_stop - 1) == (u8c_t)q) break;
    }

    if (q) {
        led_debug("quotes found: %c", q);
        led_u8s_app_zn(&led.line_write.lstr, &led.line_prep.lstr, led.line_prep.zone_start + 1, led.line_prep.zone_stop - 1);
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
        if (!isspace(led_u8s_char_at(&led.line_prep.lstr, str_start))) break;
    }
    for (; str_stop > str_start; str_stop--) {
        if (!isspace(led_u8s_char_at(&led.line_prep.lstr, str_stop - 1))) break;
    }
    led_u8s_app_zn(&led.line_write.lstr, &led.line_prep.lstr, str_start, str_stop);

    led_zone_post_process();
}

void led_fn_impl_trim_left(led_fn_t* pfunc) {
    led_zone_pre_process(pfunc);

    size_t str_start = led.line_prep.zone_start;
    for (; str_start < led.line_prep.zone_stop; str_start++) {
        if (!isspace(led_u8s_char_at(&led.line_prep.lstr, str_start))) break;
    }
    led_u8s_app_zn(&led.line_write.lstr, &led.line_prep.lstr, str_start, led.line_prep.zone_stop);

    led_zone_post_process();
}

void led_fn_impl_trim_right(led_fn_t* pfunc) {
    led_zone_pre_process(pfunc);

    size_t str_stop = led.line_prep.zone_stop;
    for (; str_stop > led.line_prep.zone_start; str_stop--)
        if (!isspace(led_u8s_char_at(&led.line_prep.lstr, str_stop - 1))) break;
    led_u8s_app_zn(&led.line_write.lstr, &led.line_prep.lstr, led.line_prep.zone_start, str_stop);

    led_zone_post_process();
}

void led_fn_impl_base64_encode(led_fn_t* pfunc) {
    led_zone_pre_process(pfunc);

    char b64buf[LED_BUF_MAX];
    base64_encodestate base64_state;
	size_t count = 0;

	base64_init_encodestate(&base64_state);
	count = base64_encode_block(
        led_u8s_str_at(&led.line_prep.lstr, led.line_prep.zone_start),
        led.line_prep.zone_stop - led.line_prep.zone_start,
        b64buf,
        &base64_state);
	count += base64_encode_blockend(
        b64buf + count,
        &base64_state);
    // remove newline and final 0
    b64buf[count - 1] = '\0';

    led_u8s_app_str(&led.line_write.lstr, b64buf);
    led_zone_post_process();
}

void led_fn_impl_base64_decode(led_fn_t* pfunc) {
    led_zone_pre_process(pfunc);

    char b64buf[LED_BUF_MAX];
	base64_decodestate base64_state;
	size_t count = 0;

	base64_init_decodestate(&base64_state);
	count = base64_decode_block(
        led_u8s_str_at(&led.line_prep.lstr, led.line_prep.zone_start),
        led.line_prep.zone_stop - led.line_prep.zone_start,
        b64buf,
        &base64_state);
    b64buf[count] = '\0';

    led_u8s_app_str(&led.line_write.lstr, b64buf);
    led_zone_post_process();
}

void led_fn_impl_url_encode(led_fn_t* pfunc) {
    led_zone_pre_process(pfunc);

    static const char HEX[] = "0123456789ABCDEF";
    char pcbuf[4] = "%00";

    size_t i = led.line_prep.zone_start;
    while ( i < led.line_prep.zone_stop ) {
        u8c_t c = led_u8s_char_next(&led.line_prep.lstr, &i);
        if (led_u8c_isalnum(c))
            led_u8s_app_char(&led.line_write.lstr, c);
        else {
            pcbuf[1] = HEX[(c >> 4) & 0x0F];
            pcbuf[2] = HEX[c & 0x0F];
            led_u8s_app_str(&led.line_write.lstr, pcbuf);
        }
    }

    led_zone_post_process();
}

void led_fn_impl_shell_escape(led_fn_t* pfunc) {
    static const char fname_stdchar_table[] = "/._-~:=%";
    led_u8s_decl_str(table, fname_stdchar_table);

    led_zone_pre_process(pfunc);
    size_t i = led.line_prep.zone_start;
    while ( i < led.line_prep.zone_stop ) {
        u8c_t c = led_u8s_char_next(&led.line_prep.lstr, &i);
        if (led_u8c_isalnum(c) || led_u8s_ischar(&table, c))
            led_u8s_app_char(&led.line_write.lstr, c);
        else {
            led_u8s_app_char(&led.line_write.lstr, '\\');
            led_u8s_app_char(&led.line_write.lstr, c);
        }
    }

    led_zone_post_process();
}

void led_fn_impl_shell_unescape(led_fn_t* pfunc) {
    led_zone_pre_process(pfunc);

    for (size_t i = led.line_prep.zone_start; i < led.line_prep.zone_stop; i++) {
        bool wasesc = false;
        char c = led_u8s_char_at(&led.line_prep.lstr, i);
        if (!wasesc && c == '\\')
            wasesc = true;
        else
            led_u8s_app_char(&led.line_write.lstr, c);
    }

    led_zone_post_process();
}

void led_fn_impl_realpath(led_fn_t* pfunc) {
    led_zone_pre_process(pfunc);

    char c = led.line_prep.buf[led.line_prep.zone_stop]; // temporary save this char for realpath function
    led.line_prep.buf[led.line_prep.zone_stop] = '\0';
    if (realpath(led_u8s_str_at(&led.line_prep.lstr, led.line_prep.zone_start), led.line_write.buf + led_u8s_len(&led.line_write.lstr)) != NULL ) {
        led.line_prep.buf[led.line_prep.zone_stop] = c;
        led_u8s_init_buf(&led.line_write.lstr, led.line_write.buf);
    }
    else {
        led.line_prep.buf[led.line_prep.zone_stop] = c;
        led_line_append_zone(&led.line_write, &led.line_prep);
    }

    led_zone_post_process();
}

void led_fn_impl_dirname(led_fn_t* pfunc) {
    led_zone_pre_process(pfunc);

    const char* dir = dirname(led_u8s_str_at(&led.line_prep.lstr, led.line_prep.zone_start));
    if (dir != NULL) led_u8s_app_str(&led.line_write.lstr, dir);
    else led_line_append_zone(&led.line_write, &led.line_prep);

    led_zone_post_process();
}

void led_fn_impl_basename(led_fn_t* pfunc) {
    led_zone_pre_process(pfunc);

    const char* fname = basename(led_u8s_str_at(&led.line_prep.lstr, led.line_prep.zone_start));
    if (fname != NULL) led_u8s_app_str(&led.line_write.lstr, fname);
    else led_line_append_zone(&led.line_write, &led.line_prep);

    led_zone_post_process();
}

void led_fn_impl_revert(led_fn_t* pfunc) {
    led_zone_pre_process(pfunc);

    for (size_t i = led.line_prep.zone_stop; i > led.line_prep.zone_start; i--)
        led_u8s_app_char(&led.line_write.lstr, led_u8s_char_at(&led.line_prep.lstr, i - 1));

    led_zone_post_process();
}

void led_fn_impl_field_base(led_fn_t* pfunc, const char* field_sep) {
    led_u8s_decl_str(sepsval, field_sep);
    led_zone_pre_process(pfunc);
    size_t field_n = pfunc->arg[0].uval;
    size_t n = 0;
    bool was_sep = true;
    size_t str_start = led.line_prep.zone_start;
    size_t str_stop = led.line_prep.zone_stop;
    for (; str_start < led.line_prep.zone_stop; str_start++ ) {
        bool is_sep = led_u8s_ischar(&sepsval, led_u8s_char_at(&led.line_prep.lstr, str_start));
        if (was_sep && !is_sep) {
            n++;
            if (n == field_n) break;
        }
        was_sep = is_sep;
    }
    if (n == field_n) {
        was_sep = false;
        for (str_stop = str_start; str_stop < led.line_prep.zone_stop; str_stop++ ) {
            bool is_sep = led_u8s_ischar(&sepsval, led_u8s_char_at(&led.line_prep.lstr, str_stop));
            if (!was_sep && is_sep) break;
        }
        led_u8s_app_zn(&led.line_write.lstr, &led.line_prep.lstr, str_start, str_stop);
    }

    led_zone_post_process();
}

void led_fn_impl_field(led_fn_t* pfunc) { led_fn_impl_field_base(pfunc, led_u8s_str(&pfunc->arg[1].lstr)); }
void led_fn_impl_field_csv(led_fn_t* pfunc) { led_fn_impl_field_base(pfunc, ",;"); }
void led_fn_impl_field_space(led_fn_t* pfunc) { led_fn_impl_field_base(pfunc, " \t\n"); }
void led_fn_impl_field_mixed(led_fn_t* pfunc) { led_fn_impl_field_base(pfunc, ",; \t\n"); }

void led_fn_impl_join(led_fn_t*) {
   for (size_t i = 0; i < led_u8s_len(&led.line_prep.lstr); i++) {
        char c = led_u8s_char_at(&led.line_prep.lstr, i);
        if ( c != '\n') led_u8s_app_char(&led.line_write.lstr, c);
   }
}

void led_fn_impl_split_base(led_fn_t* pfunc, const char* field_sep) {
    led_u8s_decl_str(sepsval, field_sep);
    led_zone_pre_process(pfunc);

    for (size_t i = led.line_prep.zone_start; i < led.line_prep.zone_stop; i++) {
        char c = led_u8s_char_at(&led.line_prep.lstr, i);
        if ( led_u8s_ischar(&sepsval, c) ) c = '\n';
        led_u8s_app_char(&led.line_write.lstr, c);
    }
    led_zone_post_process();
}

void led_fn_impl_split(led_fn_t* pfunc) { led_fn_impl_split_base(pfunc, led_u8s_str(&pfunc->arg[0].lstr)); }
void led_fn_impl_split_space(led_fn_t* pfunc) { led_fn_impl_split_base(pfunc, " \t\n"); }
void led_fn_impl_split_csv(led_fn_t* pfunc) { led_fn_impl_split_base(pfunc, ",;"); }
void led_fn_impl_split_mixed(led_fn_t* pfunc) { led_fn_impl_split_base(pfunc, ",; \t\n"); }

void led_fn_impl_randomize_base(led_fn_t* pfunc, const char* charset, size_t len) {
    led_zone_pre_process(pfunc);

    for (size_t i = led.line_prep.zone_start; i < led.line_prep.zone_stop; i++) {
        char c = charset[rand() % len];
        led_u8s_app_char(&led.line_write.lstr, c);
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
    size_t iname = led_u8s_rfind_char_zn(&led.line_prep.lstr, '/', led.line_prep.zone_start, led.line_prep.zone_stop);
    if (iname == led_u8s_len(&led.line_prep.lstr)) iname = led.line_prep.zone_start;
    else iname++;
    led_debug("led_fn_helper_fname_pos iname: %u %s", iname, led_u8s_str_at(&led.line_prep.lstr, iname));
    return iname;
}

void led_fn_impl_fname_lower(led_fn_t* pfunc) {
    led_zone_pre_process(pfunc);

    if (led.line_prep.zone_start < led.line_prep.zone_stop) {
        size_t iname = led_fn_helper_fname_pos();
        led_u8s_app_zn(&led.line_write.lstr, &led.line_prep.lstr, led.line_prep.zone_start, iname);

        for (; iname < led.line_prep.zone_stop; iname++) {
            char c = led_u8s_char_at(&led.line_prep.lstr, iname);
            if (isalnum(c))
                led_u8s_app_char(&led.line_write.lstr, tolower(c));
            else if (c == '.') {
                if (!isalnum(led_u8s_char_last(&led.line_write.lstr)))
                    led_str_trunk_char_last(&led.line_write.lstr);
                led_u8s_app_char(&led.line_write.lstr, c);
            }
            else {
                if (isalnum(led_u8s_char_last(&led.line_write.lstr))) led_u8s_app_char(&led.line_write.lstr, '_');
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
        led_u8s_app_zn(&led.line_write.lstr, &led.line_prep.lstr, led.line_prep.zone_start, iname);

        for (; iname < led.line_prep.zone_stop; iname++) {
            char c = led_u8s_char_at(&led.line_prep.lstr, iname);
            if (isalnum(c))
                led_u8s_app_char(&led.line_write.lstr, toupper(c));
            else if (c == '.') {
                if (!isalnum(led_u8s_char_last(&led.line_write.lstr)))
                    led_str_trunk_char_last(&led.line_write.lstr);
                led_u8s_app_char(&led.line_write.lstr, c);
            }
            else {
                if (isalnum(led_u8s_char_last(&led.line_write.lstr))) led_u8s_app_char(&led.line_write.lstr, '_');
            }
        }
    }
    led_zone_post_process();
}

void led_fn_impl_fname_camel(led_fn_t* pfunc) {
    led_zone_pre_process(pfunc);

    if (led.line_prep.zone_start < led.line_prep.zone_stop) {
        size_t iname = led_fn_helper_fname_pos();
        led_u8s_app_zn(&led.line_write.lstr, &led.line_prep.lstr, led.line_prep.zone_start, iname);

        bool wasword = true;
        bool isfirst = true;
        for (; iname < led.line_prep.zone_stop; iname++) {
            char c = led_u8s_char_at(&led.line_prep.lstr, iname);
            if (isalnum(led_u8s_char_last(&led.line_write.lstr)) && c == '.') {
                led_u8s_app_char(&led.line_write.lstr, c);
                isfirst = true;
            }
            else {
                bool isword = isalnum(c);
                if (isword) {
                    if (wasword || isfirst) led_u8s_app_char(&led.line_write.lstr, tolower(c));
                    else led_u8s_app_char(&led.line_write.lstr, toupper(c));
                    isfirst = false;
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
        led_u8s_app_zn(&led.line_write.lstr, &led.line_prep.lstr, led.line_prep.zone_start, iname);

        for (; iname < led.line_prep.zone_stop; iname++) {
            char c = led_u8s_char_at(&led.line_prep.lstr, iname);
            char lc = led_u8s_char_last(&led.line_write.lstr);
            if (isalnum(c))
                led_u8s_app_char(&led.line_write.lstr, tolower(c));
            else if (c == '.') {
                led_str_trunk_char(&led.line_write.lstr, '.');
                led_str_trunk_char(&led.line_write.lstr, '_');
                led_u8s_app_char(&led.line_write.lstr, '.');
            }
            else if (lc != '\0' && lc != '.') {
                led_str_trunk_char(&led.line_write.lstr, '_');
                led_u8s_app_char(&led.line_write.lstr, '_');
            }
        }
    }
    led_zone_post_process();
}

void led_fn_impl_generate(led_fn_t* pfunc) {
    led_zone_pre_process(pfunc);
    size_t n = pfunc->arg[1].uval > 0 ? led.line_prep.zone_start + pfunc->arg[1].uval : led.line_prep.zone_stop;
    char c = led_u8s_char_first(&pfunc->arg[0].lstr);

    for (size_t i = led.line_prep.zone_start; i < n; i++) {
        led_u8s_app_char(&led.line_write.lstr, c);
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
    { "she", "shell_escape", &led_fn_impl_shell_escape, "", "Shell escape", "she/[regex]" },
    { "shu", "shell_unescape", &led_fn_impl_shell_unescape, "", "Shell un-escape", "shu/[regex]" },
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
