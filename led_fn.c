/***************************************************************************
 Copyright (C) 2024 - Olivier ROUITS <olivier.rouits@free.fr>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 USA
 ***************************************************************************/

#include "led.h"

#include <b64/cencode.h>
#include <b64/cdecode.h>

//-----------------------------------------------
// LED functions utilities
//-----------------------------------------------

#define countof(a) (sizeof(a)/sizeof(a[0]))

bool led_zn_pre_process(led_fn_t* pfunc) {
    bool rc;
    led_line_init(&led.line_write);

    if (pfunc->regex) {
        led.line_prep.zone_start = led.line_prep.zone_stop = led_str_len(&led.line_prep.lstr);
        rc = led_str_match_offset(&led.line_prep.lstr, pfunc->regex, &led.line_prep.zone_start, &led.line_prep.zone_stop);
        if (!led.opt.output_match)
            led_str_app_zn(&led.line_write.lstr, &led.line_prep.lstr, 0, led.line_prep.zone_start);
    }
    else {
        led.line_prep.zone_start = 0;
        led.line_prep.zone_stop = led_str_len(&led.line_prep.lstr);
        rc = true;
    }

    return rc;
}

void led_zn_post_process() {
    if (!led.opt.output_match)
        led_str_app_zn(&led.line_write.lstr, &led.line_prep.lstr, led.line_prep.zone_stop, led.line_prep.lstr.len);
}

//-----------------------------------------------
// LED functions
//-----------------------------------------------

void led_fn_impl_register(led_fn_t* pfunc) {
    // register is a passtrough function, line stays unchanged
    led_line_cpy(&led.line_write, &led.line_prep);

    if (pfunc->regex) {
        pcre2_match_data* match_data = pcre2_match_data_create_from_pattern(pfunc->regex, NULL);
        int rc = pcre2_match(pfunc->regex, (PCRE2_SPTR)led_str_str(&led.line_prep.lstr), led_str_len(&led.line_prep.lstr), 0, 0, match_data, NULL);
        PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(match_data);
        led_debug("led_fn_impl_register: match_count %d ", rc);

        if (pfunc->arg_count > 0) {
            // usecase with fixed register ID argument
            size_t ir = pfunc->arg[0].uval;
            led_assert(ir < LED_REG_MAX, LED_ERR_ARG, "Register ID %lu exeed maximum register ID %d", ir, LED_REG_MAX-1);
            if( rc > 0) {
                int iv = (rc - 1) * 2;
                led_debug("led_fn_impl_register: match_offset values %d %d", ovector[iv], ovector[iv+1]);
                led_line_init(&led.line_reg[ir]);
                led_str_app_zn(&led.line_reg[ir].lstr, &led.line_prep.lstr, ovector[iv], ovector[iv+1]);
                led_debug("led_fn_impl_register: register value %d (%s)", ir, led_str_str(&led.line_reg[ir].lstr));
            }
        }
        else {
            // usecase with unfixed register ID, catch all groups and distribute into registers, R0 is the global matching zone
            led_foreach_int(rc)
                if (foreach.i < LED_REG_MAX) {
                    int iv = foreach.i * 2;
                    led_debug("led_fn_impl_register: match_offset values %d %d", ovector[iv], ovector[iv+1]);
                    led_line_init(&led.line_reg[foreach.i]);
                    led_str_app_zn(&led.line_reg[foreach.i].lstr, &led.line_prep.lstr, ovector[iv], ovector[iv+1]);
                    led_debug("led_fn_impl_register: register value %d (%s)", foreach.i, led_str_str(&led.line_reg[foreach.i].lstr));
                }
        }
        pcre2_match_data_free(match_data);
    }
    else {
        led_debug("led_fn_impl_register: no regx, match all");
        size_t ir = pfunc->arg_count > 0 ? pfunc->arg[0].uval : 0;
        led_assert(ir < LED_REG_MAX, LED_ERR_ARG, "Register ID %lu exeed maximum register ID %d", ir, LED_REG_MAX-1);
        led_line_cpy(&led.line_reg[ir], &led.line_prep);
    }
}

void led_fn_impl_register_recall(led_fn_t* pfunc) {
    size_t ir = pfunc->arg_count > 0 ? pfunc->arg[0].uval : 0;
    led_assert(ir < LED_REG_MAX, LED_ERR_ARG, "Register ID %lu exeed maximum register ID %d", ir, LED_REG_MAX-1);

    if (led_line_isinit(&led.line_reg[ir])) {
        if (pfunc->regex) {
            led.line_reg[ir].zone_start = led.line_reg[ir].zone_stop = led_str_len(&led.line_reg[ir].lstr);
            led_str_match_offset(&led.line_reg[ir].lstr, pfunc->regex, &led.line_reg[ir].zone_start, &led.line_reg[ir].zone_stop);
        }
        else {
            led.line_reg[ir].zone_start = 0;
            led.line_reg[ir].zone_stop = led_str_len(&led.line_reg[ir].lstr);
        }
        led_line_init(&led.line_write);
        led_str_app_zn(&led.line_write.lstr, &led.line_reg[ir].lstr, led.line_reg[ir].zone_start, led.line_reg[ir].zone_stop);
    }
    else {
        // no change to current line if register is not init
        led_line_cpy(&led.line_write, &led.line_prep);
    }
}

void led_fn_helper_substitute(led_fn_t* pfunc, led_str_t* sinput, led_str_t* soutput) {
    led_str_decl(sreplace, LED_BUF_MAX);
    led_debug("led_fn_helper_substitute: Replace registers in substitute string (len=%d) %s", led_str_len(&pfunc->arg[0].lstr), led_str_str(&pfunc->arg[0].lstr));

    led_str_foreach_uchar(&pfunc->arg[0].lstr) {
        if (led_str_isfull(&sreplace)) break;

        if (foreach.uc == '$' && led_str_uchar_at(&pfunc->arg[0].lstr, foreach.i_next) == 'R') {
            size_t ir = 0;
            // set next position after "$R".
            foreach.i_next++;
            // check for the register ID if given.
            if ( foreach.i_next < led_str_len(&pfunc->arg[0].lstr) && led_uchar_isdigit(led_str_uchar_at(&pfunc->arg[0].lstr, foreach.i_next)) ) {
                // get register ID and increment next position.
                ir = led_str_uchar_at(&pfunc->arg[0].lstr, foreach.i_next++) - '0';
            }
            led_debug("led_fn_helper_substitute: Replace register %lu found at %lu next chars at %lu", ir, foreach.i, foreach.i_next) ;
            led_str_foreach_uchar(&led.line_reg[ir].lstr)
                led_str_app_uchar(&sreplace, foreach.uc);
        }
        else {
            // led_debug("led_fn_helper_substitute: append to sreplace %c", c);
            led_str_app_uchar(&sreplace, foreach.uc);
        }
    }

    //TODO: must be optimized, should be done at initialization
    uint32_t opts = 0;
    if (pfunc->arg_count > 1) {
        led_str_foreach_uchar(&pfunc->arg[1].lstr)
            switch (foreach.uc) {
                case 'g':
                    opts |= PCRE2_SUBSTITUTE_GLOBAL;
                    break;
                case 'e':
                    opts |= PCRE2_SUBSTITUTE_EXTENDED;
                    break;
                case 'l':
                    opts |= PCRE2_SUBSTITUTE_LITERAL;
                    break;
                default:
                    break;
            }
    }

    if (!pfunc->regex)
        pfunc->regex = led.opt.pack_selected ? LED_REGEX_ALL_PACKEDLINES: LED_REGEX_ALL_LINE;

    led_debug("led_fn_helper_substitute: Substitute input line (len=%d) to sreplace (len=%d)", led_str_len(sinput), led_str_len(&sreplace));
    PCRE2_SIZE len = led_str_size(soutput);
    int rc = pcre2_substitute(
        pfunc->regex,
        (PCRE2_UCHAR*)led_str_str(sinput),
        led_str_len(sinput),
        0,
        opts,
        NULL,
        NULL,
        (PCRE2_UCHAR*)led_str_str(&sreplace),
        led_str_len(&sreplace),
        (PCRE2_UCHAR*)led_str_str(soutput),
        &len);
    led_assert_pcre(rc);
    soutput->len = len;
}

void led_fn_impl_substitute(led_fn_t* pfunc) {
    led_fn_helper_substitute(pfunc, &led.line_prep.lstr, led_str_init_buf(&led.line_write.lstr,led.line_write.buf));
}

void led_fn_impl_delete(led_fn_t* pfunc) {
    led_zn_pre_process(pfunc);

    if (led.line_prep.zone_start == 0 && led.line_prep.zone_stop == led_str_len(&led.line_prep.lstr))
        // delete all the line if it all match
        led_line_reset(&led.line_write);
    else
        // only remove matching zone
        led_zn_post_process();
}

void led_fn_impl_delete_blank(led_fn_t* pfunc) {
    (void) pfunc;
    if (led_str_isempty(&led.line_prep.lstr) || led_str_isblank(&led.line_prep.lstr))
        led_line_reset(&led.line_write);
    else
        led_str_cpy(&led.line_write.lstr, &led.line_prep.lstr);
}

void led_fn_impl_insert(led_fn_t* pfunc) {
    led_str_decl(newline, LED_BUF_MAX);
    led_fn_helper_substitute(pfunc, &led.line_prep.lstr, &newline);

    led_str_init_buf(&led.line_write.lstr, led.line_write.buf);
    led_str_empty(&led.line_write.lstr);
    size_t lcount = pfunc->arg_count > 1 ? pfunc->arg[1].uval : 1;
    led_foreach_int(lcount) {
        led_str_app(&led.line_write.lstr, &newline);
        led_str_app_uchar(&led.line_write.lstr, '\n');
    }
    led_str_app(&led.line_write.lstr, &led.line_prep.lstr);
}

void led_fn_impl_append(led_fn_t* pfunc) {
    led_str_decl(newline, LED_BUF_MAX);
    led_fn_helper_substitute(pfunc, &led.line_prep.lstr, &newline);

    led_str_init_buf(&led.line_write.lstr,led.line_write.buf);
    led_str_cpy(&led.line_write.lstr, &led.line_prep.lstr);
    size_t lcount = pfunc->arg_count > 1 ? pfunc->arg[1].uval : 1;
    led_foreach_int(lcount) {
        led_str_app_uchar(&led.line_write.lstr, '\n');
        led_str_app(&led.line_write.lstr, &newline);;
    }
}

void led_fn_impl_range_sel(led_fn_t* pfunc) {
    led_line_init(&led.line_write);

    if (led_str_iscontent(&pfunc->arg[0].lstr)) {
        long val = pfunc->arg[0].val;
        size_t uval = pfunc->arg[0].uval;
        if (val >= 0) {
            led.line_prep.zone_start = 0;
            led_foreach_int(uval)
                if (led.line_prep.zone_start < led_str_len(&led.line_prep.lstr))
                    led_str_uchar_next(&led.line_prep.lstr, led.line_prep.zone_start, &led.line_prep.zone_start);
        }
        else {
            led.line_prep.zone_start = led_str_len(&led.line_prep.lstr);
            led_foreach_int(uval)
                if (led.line_prep.zone_start > 0)
                    led_str_uchar_prev(&led.line_prep.lstr, led.line_prep.zone_start, &led.line_prep.zone_start);
        }
    }
    if (led_str_iscontent(&pfunc->arg[1].lstr)) {
        size_t uval = pfunc->arg[1].uval;
        led.line_prep.zone_stop = led.line_prep.zone_start;
        led_foreach_int(uval)
            if (led.line_prep.zone_stop < led_str_len(&led.line_prep.lstr))
                led_str_uchar_next(&led.line_prep.lstr, led.line_prep.zone_stop, &led.line_prep.zone_stop);
    }
    else
        led.line_prep.zone_stop = led_str_len(&led.line_prep.lstr);

    led_line_append_zn(&led.line_write, &led.line_prep);
}

void led_fn_impl_range_unsel(led_fn_t* pfunc) {
    led_line_init(&led.line_write);

    if (led_str_iscontent(&pfunc->arg[0].lstr)) {
        long val = pfunc->arg[0].val;
        size_t uval = pfunc->arg[0].uval;
        if (val >= 0) {
            led.line_prep.zone_start = 0;
            led_foreach_int(uval)
                if (led.line_prep.zone_start < led_str_len(&led.line_prep.lstr))
                    led_str_uchar_next(&led.line_prep.lstr, led.line_prep.zone_start, &led.line_prep.zone_start);
        }
        else {
            led.line_prep.zone_start = led_str_len(&led.line_prep.lstr);
            led_foreach_int(uval)
                if (led.line_prep.zone_start > 0)
                    led_str_uchar_prev(&led.line_prep.lstr, led.line_prep.zone_start, &led.line_prep.zone_start);
        }
    }
    if (led_str_iscontent(&pfunc->arg[1].lstr)) {
        size_t uval = pfunc->arg[1].uval;
        led.line_prep.zone_stop = led.line_prep.zone_start;
        led_foreach_int(uval)
            if (led.line_prep.zone_stop < led_str_len(&led.line_prep.lstr))
                led_str_uchar_next(&led.line_prep.lstr, led.line_prep.zone_stop, &led.line_prep.zone_stop);
    }
    else
        led.line_prep.zone_stop = led_str_len(&led.line_prep.lstr);

    led_line_append_before_zn(&led.line_write, &led.line_prep);
    led_line_append_after_zn(&led.line_write, &led.line_prep);
}

void led_fn_impl_translate(led_fn_t* pfunc) {
    led_zn_pre_process(pfunc);

    led_str_foreach_uchar_zn(&led.line_prep.lstr, led.line_prep.zone_start, led.line_prep.zone_stop) {
        led_uchar_t uc = foreach.uc;
        led_uchar_t uct = '\0';
        led_str_foreach_uchar(&pfunc->arg[0].lstr) {
            if (foreach.uc == uc) {
                if ((uct = led_str_uchar_n(&pfunc->arg[1].lstr, foreach.uc_count)))
                    led_str_app_uchar(&led.line_write.lstr, uct);
                break;
            }
        }
        if (!uct)
            led_str_app_uchar(&led.line_write.lstr, uc);
    }

    led_zn_post_process();
}

void led_fn_impl_case_lower(led_fn_t* pfunc) {
    led_zn_pre_process(pfunc);

    led_str_foreach_uchar_zn(&led.line_prep.lstr, led.line_prep.zone_start, led.line_prep.zone_stop)
        led_str_app_uchar(&led.line_write.lstr, led_uchar_tolower(foreach.uc));

    led_zn_post_process();
}

void led_fn_impl_case_upper(led_fn_t* pfunc) {
    led_zn_pre_process(pfunc);

    led_str_foreach_uchar_zn(&led.line_prep.lstr, led.line_prep.zone_start, led.line_prep.zone_stop)
        led_str_app_uchar(&led.line_write.lstr, led_uchar_toupper(foreach.uc));

    led_zn_post_process();
}

void led_fn_impl_case_first(led_fn_t* pfunc) {
    led_zn_pre_process(pfunc);

    led_str_app_uchar(&led.line_write.lstr, led_uchar_toupper(led_str_uchar_next(&led.line_prep.lstr, led.line_prep.zone_start, &led.line_prep.zone_start)));

    led_str_foreach_uchar_zn(&led.line_prep.lstr, led.line_prep.zone_start, led.line_prep.zone_stop)
        led_str_app_uchar(&led.line_write.lstr, led_uchar_tolower(foreach.uc));

    led_zn_post_process();
}

void led_fn_impl_case_camel(led_fn_t* pfunc) {
    led_zn_pre_process(pfunc);

    bool wasword = false;
    led_str_foreach_uchar_zn(&led.line_prep.lstr, led.line_prep.zone_start, led.line_prep.zone_stop) {
        bool isword = led_uchar_isalnum(foreach.uc) || foreach.uc == '_';
        if (isword) {
            if (wasword) led_str_app_uchar(&led.line_write.lstr, led_uchar_tolower(foreach.uc));
            else led_str_app_uchar(&led.line_write.lstr, led_uchar_toupper(foreach.uc));
        }
        wasword = isword;
    }

    led_zn_post_process();
}

void led_fn_impl_case_snake(led_fn_t* pfunc) {
    led_zn_pre_process(pfunc);

    led_str_foreach_uchar_zn(&led.line_prep.lstr, led.line_prep.zone_start, led.line_prep.zone_stop) {
        led_uchar_t ucnext = led_str_uchar_at(&led.line_write.lstr, foreach.i_next);
        if (led_uchar_isalnum(foreach.uc))
            led_str_app_uchar(&led.line_write.lstr, led_uchar_tolower(foreach.uc));
        else if (ucnext != '_')
            led_str_app_uchar(&led.line_write.lstr, '_');
    }

    led_zn_post_process();
}

void led_fn_impl_quote_base(led_fn_t* pfunc, led_uchar_t q) {
    led_zn_pre_process(pfunc);

    if (! (led_str_uchar_at(&led.line_prep.lstr, led.line_prep.zone_start) == q && led_str_uchar_at(&led.line_prep.lstr, led.line_prep.zone_stop - 1) == q) ) {
        led_debug("led_fn_impl_quote_base: quote active");
        led_str_app_uchar(&led.line_write.lstr, q);
        led_line_append_zn(&led.line_write, &led.line_prep);
        led_str_app_uchar(&led.line_write.lstr, q);
    }
    else
        led_line_append_zn(&led.line_write, &led.line_prep);

    led_zn_post_process();
}

void led_fn_impl_quote_simple(led_fn_t* pfunc) { led_fn_impl_quote_base(pfunc, '\''); }
void led_fn_impl_quote_double(led_fn_t* pfunc) { led_fn_impl_quote_base(pfunc, '"'); }
void led_fn_impl_quote_back(led_fn_t* pfunc) { led_fn_impl_quote_base(pfunc, '`'); }

void led_fn_impl_quote_remove(led_fn_t* pfunc) {
    const char QUOTES[] = "'\"`";
    led_zn_pre_process(pfunc);

    bool found = false;
    led_foreach_char(QUOTES)
        if (led_str_uchar_at(&led.line_prep.lstr, led.line_prep.zone_start) == (led_uchar_t)foreach.c
            && led_str_uchar_at(&led.line_prep.lstr, led.line_prep.zone_stop - 1) == (led_uchar_t)foreach.c) {
            found = true;
            led_debug("led_fn_impl_quote_remove: quote found %c", foreach.c);
            break;
        }
    if (found)
        led_str_app_zn(&led.line_write.lstr, &led.line_prep.lstr, led.line_prep.zone_start + 1, led.line_prep.zone_stop - 1);
    else
        led_line_append_zn(&led.line_write, &led.line_prep);

    led_zn_post_process();
}

//TODO use led_str trim functions

void led_fn_impl_trim(led_fn_t* pfunc) {
    led_zn_pre_process(pfunc);

    led_str_foreach_uchar_zn(&led.line_prep.lstr, led.line_prep.zone_start, led.line_prep.zone_stop) {
        if (!led_uchar_isspace(foreach.uc)) {
            led_str_app_zn(&led.line_write.lstr, &led.line_prep.lstr, foreach.i, led.line_prep.zone_stop);
            break;
        }
    }

    led_str_foreach_uchar_zn_r(&led.line_prep.lstr, led.line_prep.zone_start, led.line_prep.zone_stop) {
        if (!led_uchar_isspace(foreach.uc)) {
            led_str_app_zn(&led.line_write.lstr, &led.line_prep.lstr, led.line_prep.zone_start, foreach.i_next);
            break;
        }
    }

    led_zn_post_process();
}

void led_fn_impl_trim_left(led_fn_t* pfunc) {
    led_zn_pre_process(pfunc);

    led_str_foreach_uchar_zn(&led.line_prep.lstr, led.line_prep.zone_start, led.line_prep.zone_stop) {
        if (!led_uchar_isspace(foreach.uc)) {
            led_str_app_zn(&led.line_write.lstr, &led.line_prep.lstr, foreach.i, led.line_prep.zone_stop);
            break;
        }
    }

    led_zn_post_process();
}

void led_fn_impl_trim_right(led_fn_t* pfunc) {
    led_zn_pre_process(pfunc);

    led_str_foreach_uchar_zn_r(&led.line_prep.lstr, led.line_prep.zone_start, led.line_prep.zone_stop) {
        if (!led_uchar_isspace(foreach.uc)) {
            led_str_app_zn(&led.line_write.lstr, &led.line_prep.lstr, led.line_prep.zone_start, foreach.i_next);
            break;
        }
    }

    led_zn_post_process();
}

void led_fn_impl_base64_encode(led_fn_t* pfunc) {
    led_zn_pre_process(pfunc);

    char b64buf[LED_BUF_MAX];
    base64_encodestate base64_state;
	size_t count = 0;

	base64_init_encodestate(&base64_state);
	count = base64_encode_block(
        led_str_str_at(&led.line_prep.lstr, led.line_prep.zone_start),
        led.line_prep.zone_stop - led.line_prep.zone_start,
        b64buf,
        &base64_state);
	count += base64_encode_blockend(
        b64buf + count,
        &base64_state);
    // remove newline and final 0
    b64buf[count - 1] = '\0';

    led_str_app_str(&led.line_write.lstr, b64buf);
    led_zn_post_process();
}

void led_fn_impl_base64_decode(led_fn_t* pfunc) {
    led_zn_pre_process(pfunc);

    char b64buf[LED_BUF_MAX];
	base64_decodestate base64_state;
	size_t count = 0;

	base64_init_decodestate(&base64_state);
	count = base64_decode_block(
        led_str_str_at(&led.line_prep.lstr, led.line_prep.zone_start),
        led.line_prep.zone_stop - led.line_prep.zone_start,
        b64buf,
        &base64_state);
    b64buf[count] = '\0';

    led_str_app_str(&led.line_write.lstr, b64buf);
    led_zn_post_process();
}

void led_fn_impl_url_encode(led_fn_t* pfunc) {
    static const char NOTRESERVED[] = "-._~";
    static const char HEX[] = "0123456789ABCDEF";
    char pcbuf[4] = "%00";

    led_zn_pre_process(pfunc);

    led_str_foreach_uchar_zn(&led.line_prep.lstr, led.line_prep.zone_start, led.line_prep.zone_stop)
        // we encode UTF8 chars byte per byte.
        if (led_uchar_isalnum(foreach.uc) || led_uchar_in_str(foreach.uc, NOTRESERVED))
            led_str_app_uchar(&led.line_write.lstr, foreach.uc);
        else {
            led_uchar_t uc = foreach.uc;
            size_t uc_size = foreach.uc_size;
            led_foreach_int(uc_size) {
                uint8_t cbyte = uc >> (8*(uc_size-1 - foreach.i));
                pcbuf[1] = HEX[(cbyte >> 4) & 0x0F];
                pcbuf[2] = HEX[cbyte & 0x0F];
                led_str_app_str(&led.line_write.lstr, pcbuf);
            }
        }
    led_zn_post_process();
}

void led_fn_impl_shell_escape(led_fn_t* pfunc) {
    static const char fname_stdchar_table[] = "/._-~:=%";
    led_str_decl_str(table, fname_stdchar_table);

    led_zn_pre_process(pfunc);

    led_str_foreach_uchar_zn(&led.line_prep.lstr, led.line_prep.zone_start, led.line_prep.zone_stop)
        if (led_uchar_isalnum(foreach.uc) || led_str_has_uchar(&table, foreach.uc))
            led_str_app_uchar(&led.line_write.lstr, foreach.uc);
        else {
            led_str_app_uchar(&led.line_write.lstr, '\\');
            led_str_app_uchar(&led.line_write.lstr, foreach.uc);
        }

    led_zn_post_process();
}

void led_fn_impl_shell_unescape(led_fn_t* pfunc) {
    led_zn_pre_process(pfunc);

    bool prev_is_esc = false;
    led_str_foreach_uchar_zn(&led.line_prep.lstr, led.line_prep.zone_start, led.line_prep.zone_stop) {
        if (!prev_is_esc && foreach.uc == '\\')
            prev_is_esc = true;
        else
            led_str_app_uchar(&led.line_write.lstr, foreach.uc);
    }
    led_zn_post_process();
}

void led_fn_impl_realpath(led_fn_t* pfunc) {
    led_zn_pre_process(pfunc);

    char c = led.line_prep.buf[led.line_prep.zone_stop]; // temporary save this char for realpath function
    led.line_prep.buf[led.line_prep.zone_stop] = '\0';
    if (realpath(led_str_str_at(&led.line_prep.lstr, led.line_prep.zone_start), led.line_write.buf + led_str_len(&led.line_write.lstr)) != NULL ) {
        led.line_prep.buf[led.line_prep.zone_stop] = c;
        led_str_init_buf(&led.line_write.lstr, led.line_write.buf);
    }
    else {
        led.line_prep.buf[led.line_prep.zone_stop] = c;
        led_line_append_zn(&led.line_write, &led.line_prep);
    }
    led_zn_post_process();
}

void led_fn_impl_dirname(led_fn_t* pfunc) {
    led_zn_pre_process(pfunc);

    const char* dir = dirname(led_str_str_at(&led.line_prep.lstr, led.line_prep.zone_start));
    if (dir != NULL) led_str_app_str(&led.line_write.lstr, dir);
    else led_line_append_zn(&led.line_write, &led.line_prep);

    led_zn_post_process();
}

void led_fn_impl_basename(led_fn_t* pfunc) {
    led_zn_pre_process(pfunc);

    const char* fname = basename(led_str_str_at(&led.line_prep.lstr, led.line_prep.zone_start));
    if (fname != NULL) led_str_app_str(&led.line_write.lstr, fname);
    else led_line_append_zn(&led.line_write, &led.line_prep);

    led_zn_post_process();
}

void led_fn_impl_revert(led_fn_t* pfunc) {
    led_zn_pre_process(pfunc);

    led_str_foreach_uchar_zn_r(&led.line_prep.lstr, led.line_prep.zone_start, led.line_prep.zone_stop)
        led_str_app_uchar(&led.line_write.lstr, foreach.uc);

    led_zn_post_process();
}

void led_fn_impl_field_base(led_fn_t* pfunc, const char* field_sep) {
    led_zn_pre_process(pfunc);

    led_str_decl_str(sepsval, field_sep);
    size_t field_n = pfunc->arg[0].uval;
    size_t n = 0;
    bool was_sep = false;

    led_str_foreach_uchar_zn(&led.line_prep.lstr, led.line_prep.zone_start, led.line_prep.zone_stop) {
        bool is_sep = led_str_has_uchar(&sepsval, foreach.uc);
        led_debug("i=%lu sep=%d was=%d char=%c n=%lu", foreach.i, is_sep, was_sep, foreach.uc, n);
        if (was_sep && !is_sep) n++;
        if (n == field_n) {
            if (is_sep) break;
            led_str_app_uchar(&led.line_write.lstr, foreach.uc);
        }
        was_sep = is_sep;
    }

    led_zn_post_process();
}

void led_fn_impl_field(led_fn_t* pfunc) { led_fn_impl_field_base(pfunc, led_str_str(&pfunc->arg[1].lstr)); }
void led_fn_impl_field_csv(led_fn_t* pfunc) { led_fn_impl_field_base(pfunc, ",;"); }
void led_fn_impl_field_space(led_fn_t* pfunc) { led_fn_impl_field_base(pfunc, " \t\n"); }
void led_fn_impl_field_mixed(led_fn_t* pfunc) { led_fn_impl_field_base(pfunc, ",; \t\n"); }

void led_fn_impl_join(led_fn_t* pfunc) {
    (void) pfunc;
    led_str_foreach_uchar(&led.line_prep.lstr) {
        if (foreach.uc != '\n') led_str_app_uchar(&led.line_write.lstr, foreach.uc);
    }
}

void led_fn_impl_split_base(led_fn_t* pfunc, const char* field_sep) {
    led_str_decl_str(sepsval, field_sep);
    led_zn_pre_process(pfunc);

    led_str_foreach_uchar_zn(&led.line_prep.lstr, led.line_prep.zone_start, led_str_len(&led.line_prep.lstr)) {
        if ( led_str_has_uchar(&sepsval, foreach.uc) ) foreach.uc = '\n';
        led_str_app_uchar(&led.line_write.lstr, foreach.uc);
    }
    led_zn_post_process();
}

void led_fn_impl_split(led_fn_t* pfunc) { led_fn_impl_split_base(pfunc, led_str_str(&pfunc->arg[0].lstr)); }
void led_fn_impl_split_space(led_fn_t* pfunc) { led_fn_impl_split_base(pfunc, " \t\n"); }
void led_fn_impl_split_csv(led_fn_t* pfunc) { led_fn_impl_split_base(pfunc, ",;"); }
void led_fn_impl_split_mixed(led_fn_t* pfunc) { led_fn_impl_split_base(pfunc, ",; \t\n"); }

void led_fn_impl_randomize_base(led_fn_t* pfunc, const char* charset, size_t len) {
    led_zn_pre_process(pfunc);

    led_str_foreach_uchar_zn(&led.line_prep.lstr, led.line_prep.zone_start, led.line_prep.zone_stop)
        led_str_app_uchar(&led.line_write.lstr, charset[rand() % len]);

    led_zn_post_process();
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
    size_t iname = led_str_rfind_uchar_zn(&led.line_prep.lstr, '/', led.line_prep.zone_start, led.line_prep.zone_stop);
    if (iname == led_str_len(&led.line_prep.lstr)) iname = led.line_prep.zone_start;
    else iname++;
    led_debug("led_fn_helper_fname_pos: iname: %u %s", iname, led_str_str_at(&led.line_prep.lstr, iname));
    return iname;
}

void led_fn_impl_fname_lower(led_fn_t* pfunc) {
    led_zn_pre_process(pfunc);

    if (led.line_prep.zone_start < led.line_prep.zone_stop) {
        size_t iname = led_fn_helper_fname_pos();
        led_str_app_zn(&led.line_write.lstr, &led.line_prep.lstr, led.line_prep.zone_start, iname);

        led_str_foreach_uchar_zn(&led.line_prep.lstr, iname, led.line_prep.zone_stop) {
            led_uchar_t luc = led_str_uchar_last(&led.line_write.lstr);
            if (led_uchar_isalnum(foreach.uc))
                led_str_app_uchar(&led.line_write.lstr, led_uchar_tolower(foreach.uc));
            else if (foreach.uc == '.') {
                if (!led_uchar_isalnum(luc))
                    led_str_trunk_uchar_last(&led.line_write.lstr);
                led_str_app_uchar(&led.line_write.lstr, foreach.uc);
            }
            else {
                if (led_uchar_isalnum(luc))
                    led_str_app_uchar(&led.line_write.lstr, '_');
            }
        }
    }
    led_zn_post_process();
}

void led_fn_impl_fname_upper(led_fn_t* pfunc) {
    led_zn_pre_process(pfunc);

    if (led.line_prep.zone_start < led.line_prep.zone_stop) {
        size_t iname = led_fn_helper_fname_pos();
        led_str_app_zn(&led.line_write.lstr, &led.line_prep.lstr, led.line_prep.zone_start, iname);

        led_str_foreach_uchar_zn(&led.line_prep.lstr, iname, led.line_prep.zone_stop) {
            led_uchar_t luc = led_str_uchar_last(&led.line_write.lstr);
            if (led_uchar_isalnum(foreach.uc))
                led_str_app_uchar(&led.line_write.lstr, led_uchar_toupper(foreach.uc));
            else if (foreach.uc == '.') {
                if (!led_uchar_isalnum(luc))
                    led_str_trunk_uchar_last(&led.line_write.lstr);
                led_str_app_uchar(&led.line_write.lstr, foreach.uc);
            }
            else {
                if (led_uchar_isalnum(luc))
                    led_str_app_uchar(&led.line_write.lstr, '_');
            }
        }
    }
    led_zn_post_process();
}

void led_fn_impl_fname_camel(led_fn_t* pfunc) {
    led_zn_pre_process(pfunc);

    if (led.line_prep.zone_start < led.line_prep.zone_stop) {
        size_t iname = led_fn_helper_fname_pos();
        led_str_app_zn(&led.line_write.lstr, &led.line_prep.lstr, led.line_prep.zone_start, iname);

        bool wasword = true;
        bool isfirst = true;
        led_str_foreach_uchar_zn(&led.line_prep.lstr, iname, led.line_prep.zone_stop) {
            led_uchar_t luc = led_str_uchar_last(&led.line_write.lstr);
            if (led_uchar_isalnum(luc) && foreach.uc == '.') {
                led_str_app_uchar(&led.line_write.lstr, foreach.uc);
                isfirst = true;
            }
            else {
                bool isword = led_uchar_isalnum(foreach.uc);
                if (isword) {
                    if (wasword || isfirst) led_str_app_uchar(&led.line_write.lstr, led_uchar_tolower(foreach.uc));
                    else led_str_app_uchar(&led.line_write.lstr, led_uchar_toupper(foreach.uc));
                    isfirst = false;
                }
                wasword = isword;
            }
        }
    }
    led_zn_post_process();
}

void led_fn_impl_fname_snake(led_fn_t* pfunc) {
    led_zn_pre_process(pfunc);

    if (led.line_prep.zone_start < led.line_prep.zone_stop) {
        size_t iname = led_fn_helper_fname_pos();
        led_str_app_zn(&led.line_write.lstr, &led.line_prep.lstr, led.line_prep.zone_start, iname);

        led_str_foreach_uchar_zn(&led.line_prep.lstr, iname, led.line_prep.zone_stop) {
            led_uchar_t luc = led_str_uchar_last(&led.line_write.lstr);
            if (led_uchar_isalnum(foreach.uc))
                led_str_app_uchar(&led.line_write.lstr, led_uchar_tolower(foreach.uc));
            else if (foreach.uc == '.') {
                led_str_trunk_uchar(&led.line_write.lstr, '.');
                led_str_trunk_uchar(&led.line_write.lstr, '_');
                led_str_app_uchar(&led.line_write.lstr, '.');
            }
            else if (luc != '\0' && luc != '.') {
                led_str_trunk_uchar(&led.line_write.lstr, '_');
                led_str_app_uchar(&led.line_write.lstr, '_');
            }
        }
    }
    led_zn_post_process();
}

void led_fn_impl_generate(led_fn_t* pfunc) {
    led_zn_pre_process(pfunc);

    led_uchar_t cdup = led_str_uchar_first(&pfunc->arg[0].lstr);

    if ( pfunc->arg[1].uval > 0  ) {
        led_foreach_int(pfunc->arg[1].uval)
            led_str_app_uchar(&led.line_write.lstr, cdup);
    }
    else {
        led_str_foreach_uchar_zn(&led.line_prep.lstr, led.line_prep.zone_start, led.line_prep.zone_stop)
            led_str_app_uchar(&led.line_write.lstr, cdup);
    }

    led_zn_post_process();
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
    { "qtd", "quote_double", &led_fn_impl_quote_double, "", "Quote double", "qtd/[regex]" },
    { "qtb", "quote_back", &led_fn_impl_quote_back, "", "Quote back", "qtb/[regex]" },
    { "qtr", "quote_remove", &led_fn_impl_quote_remove, "", "Quote remove", "qtr/[regex]" },
    { "sp", "split", &led_fn_impl_split, "S", "Split using characters", "sp/[regex]/chars" },
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
