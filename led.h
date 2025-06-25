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

#include <limits.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <locale.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <libgen.h>
#include <stdbool.h>

#ifdef WIN32
#define realpath(N,R) _fullpath((R),(N),PATH_MAX)
#endif

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

//-----------------------------------------------
// LED error management
//-----------------------------------------------
#define LED_SUCCESS 0
#define LED_ERR_ARG 1
#define LED_ERR_PCRE 2
#define LED_ERR_FILE 3
#define LED_ERR_MAXLINE 4
#define LED_ERR_INTERNAL 5

#define LED_MSG_MAX 0x1000

void led_assert(bool cond, int code, const char* message, ...);
void led_assert_pcre(int rc);
void led_debug(const char* message, ...);

//------------------------------------------------------------------------------
// generic loops
//------------------------------------------------------------------------------

#define led_foreach_char(STR) \
    for (struct{size_t i; char c;} foreach = {0, (STR)[0]};\
        foreach.c;\
        foreach.c = (STR)[++foreach.i])

#define led_foreach_int_range(START, STOP) \
    for (struct{size_t i;} foreach = {0};\
        foreach.i < (size_t)STOP;\
        foreach.i++)

#define led_foreach_int(LEN) led_foreach_int_range(0, LEN)

#define led_foreach_pval_len(ARRAY, LEN) \
    for (struct{size_t i; typeof(*(ARRAY))* pval;} foreach = {0, ARRAY};\
        foreach.i < LEN;\
        foreach.pval = &((ARRAY)[++foreach.i]))

#define led_foreach_pval(ARRAY) led_foreach_pval_len(ARRAY, sizeof(ARRAY)/sizeof(*(ARRAY)))

#define led_foreach_val_len(ARRAY, LEN) \
    for (struct{size_t i; typeof(*(ARRAY)) val;} foreach = {0, ARRAY[0]};\
        foreach.i < LEN;\
        foreach.val = (ARRAY)[++foreach.i])

#define led_foreach_val(ARRAY) led_foreach_val_len(ARRAY, sizeof(ARRAY)/sizeof(*(ARRAY)))

//------------------------------------------------------------------------------
// LED UTF8 support
//------------------------------------------------------------------------------

typedef uint32_t led_uchar_t;

#define led_foreach_uchar(STR) \
    for (struct{size_t i; led_uchar_t uc; size_t uc_len;} foreach = {0, led_uchar_of_str(STR), led_uchar_size_str(STR)};\
        foreach.uc;\
        foreach.i += foreach.uc_len, foreach.uc_len = led_uchar_from_str(STR + foreach.i, &foreach.uc ))

extern const size_t led_uchar_size_table[];

inline char led_uchar_to_char(led_uchar_t uc) {
    return (char)(uc & 0xFF);
}

inline size_t led_uchar_size_str(const char* str) {
    return led_uchar_size_table[(((uint8_t *)(str))[0] & 0xFF) >> 4];
}

inline size_t led_uchar_size(led_uchar_t uc) {
    return 1 + ((uc & 0x8000)>>15) + ((uc & 0x800000)>>23) + ((uc & 0x80000000)>>31);
}

inline bool led_uchar_iscont(char c) {
    return (c & 0xC0) == 0x80;
}

inline size_t led_uchar_pos_next(const char* str, size_t idx) {
    return idx + led_uchar_size_str(str + idx);
}

inline size_t led_uchar_pos_prev(const char* str, size_t idx) {
    if (idx)
        for (idx--; idx && led_uchar_iscont(str[idx]); idx--);
    return idx;
}

inline bool led_uchar_isalnum(led_uchar_t uc) {
    return isalnum(led_uchar_to_char(uc));
}

inline bool led_uchar_isdigit(led_uchar_t uc) {
    return isdigit(led_uchar_to_char(uc));
}

inline bool led_uchar_isspace(led_uchar_t uc) {
    return isspace(led_uchar_to_char(uc));
}

inline led_uchar_t led_uchar_tolower(led_uchar_t uc) {
    if (uc >= 'A' && uc <= 'Z') uc = tolower(led_uchar_to_char(uc));
    return uc;
}
inline led_uchar_t led_uchar_toupper(led_uchar_t uc) {
    if (uc >= 'a' && uc <= 'z') uc = toupper(led_uchar_to_char(uc));
    return uc;
}

size_t led_uchar_to_str(char* str, led_uchar_t uc);
size_t led_uchar_from_str(const char* str, led_uchar_t* puchar);

inline led_uchar_t led_uchar_of_str(const char* str) {
    led_uchar_t uc;
    led_uchar_from_str(str, &uc);
    return uc;
}

inline bool led_uchar_in_str(led_uchar_t uc, const char* str) {
    led_foreach_uchar(str)
        if (uc == foreach.uc) return true;
    return false;
}

/* codepoints UFT-8 functions are not necessary but we let it if needed.

bool led_uchar_isvalid(led_uchar_t uc);
led_uchar_t led_uchar_encode(uint32_t code);
uint32_t led_uchar_decode(led_uchar_t uc);

*/

//------------------------------------------------------------------------------
// Led poor & simple string management without any memory allocation.
// Led strings only wraps buffers declared statically or in the stack
// to make utf-8 string management easyer.
//------------------------------------------------------------------------------

typedef struct {
    char* str;
    size_t len;
    size_t size;
} led_str_t;

#define led_str_init_buf(VAR,BUF) led_str_init(VAR,BUF,sizeof(BUF))
#define led_str_init_str(VAR,STR) led_str_init(VAR,(char*)STR,0)

#define led_str_decl_str(VAR,STR) \
    led_str_t VAR; \
    led_str_init_str(&VAR,STR)

#define led_str_decl(VAR,LEN) \
    led_str_t VAR; \
    char VAR##_buf[LEN]; \
    VAR##_buf[0] = '\0'; \
    led_str_init_buf(&VAR,VAR##_buf)

#define led_str_decl_cpy(VAR,SRC) \
    led_str_t VAR; \
    char VAR##_buf[SRC.len]; \
    led_str_init(&VAR,VAR##_buf,SRC.len); \
    led_str_cpy(&VAR, &SRC)

#define led_str_foreach_char_zn(VAR, START, STOP) \
    for (struct{size_t i; char c;} foreach = {START, led_str_str(VAR)[START]};\
        foreach.i < STOP;\
        foreach.c = led_str_str(VAR)[++foreach.i])

#define led_str_foreach_char(VAR) led_str_foreach_char_zn(VAR, 0, led_str_len(VAR))

#define led_str_foreach_uchar_zn(VAR, START, STOP) \
    for (struct{size_t i; size_t i_next; led_uchar_t uc; size_t uc_count; size_t uc_size;} foreach = {START, led_str_pos_uchar_next(VAR, START), led_str_uchar_at(VAR, START), 1, led_str_uchar_size_at(VAR, START)};\
        foreach.i < STOP;\
        foreach.i = foreach.i_next, foreach.uc = led_str_uchar_next(VAR, foreach.i_next, &foreach.i_next), foreach.uc_count++, foreach.uc_size = foreach.i_next - foreach.i)

#define led_str_foreach_uchar(VAR) led_str_foreach_uchar_zn(VAR, 0, led_str_len(VAR))

#define led_str_foreach_uchar_zn_r(VAR, START, STOP) \
    for (struct{size_t i; size_t i_next; led_uchar_t uc; size_t uc_count; size_t uc_size;} foreach = {led_str_pos_uchar_prev(VAR, STOP), STOP, led_str_uchar_prev(VAR, STOP, NULL), 1, STOP - led_str_pos_uchar_prev(VAR, STOP)};\
        foreach.i_next > START;\
        foreach.i_next = foreach.i, foreach.uc = led_str_uchar_prev(VAR, foreach.i, &foreach.i), foreach.uc_count++, foreach.uc_size = foreach.i_next - foreach.i)

#define led_str_foreach_uchar_r(VAR) led_str_foreach_uchar_zn_r(VAR, 0, led_str_len(VAR))

inline size_t led_str_len(led_str_t* lstr) {
    return lstr->len;
}

inline char* led_str_str(led_str_t* lstr) {
    return lstr->str;
}

inline size_t led_str_size(led_str_t* lstr) {
    return lstr->size;
}

inline bool led_str_isinit(led_str_t* lstr) {
    return lstr->str != NULL;
}

inline bool led_str_isempty(led_str_t* lstr) {
    return led_str_isinit(lstr) && lstr->len == 0;
}

inline bool led_str_iscontent(led_str_t* lstr) {
    return led_str_isinit(lstr) && lstr->len > 0;
}

inline bool led_str_isfull(led_str_t* lstr) {
    return led_str_isinit(lstr) && lstr->len + 1 == lstr->size;
}

inline led_str_t* led_str_reset(led_str_t* lstr) {
    memset(lstr, 0, sizeof(*lstr));
    return lstr;
}

led_str_t* led_str_init(led_str_t* lstr, char* buf, size_t size);

inline led_str_t* led_str_empty(led_str_t* lstr) {
    lstr->str[0] = '\0';
    lstr->len = 0;
    return lstr;
}

inline led_str_t* led_str_clone(led_str_t* lstr, led_str_t* lstr_src) {
    lstr->str = lstr_src->str;
    lstr->len = lstr_src->len;
    lstr->size = lstr_src->size;
    return lstr;
}

inline led_str_t* led_str_cpy(led_str_t* lstr, led_str_t* lstr_src) {
    lstr->len = 0;
    led_str_foreach_char(lstr_src)
        if (lstr->len+1 < lstr->size )
            lstr->str[lstr->len++] = foreach.c;
        else break;
    lstr->str[lstr->len] = '\0';
    return lstr;
}

inline led_str_t* led_str_cpy_str(led_str_t* lstr, const char* str) {
    lstr->len = 0;
    led_foreach_char(str)
        if (lstr->len+1 < lstr->size )
            lstr->str[lstr->len++] = foreach.c;
        else break;
    lstr->str[lstr->len] = '\0';
    return lstr;
}

inline led_str_t* led_str_app(led_str_t* lstr, led_str_t* lstr_src) {
    led_str_foreach_char(lstr_src)
        if (lstr->len+1 < lstr->size )
            lstr->str[lstr->len++] = foreach.c;
        else break;
    lstr->str[lstr->len] = '\0';
    return lstr;
}

inline led_str_t* led_str_app_str(led_str_t* lstr, const char* str) {
    led_foreach_char(str)
        if (lstr->len+1 < lstr->size )
            lstr->str[lstr->len++] = foreach.c;
        else break;
    lstr->str[lstr->len] = '\0';
    return lstr;
}

inline led_str_t* led_str_app_zn(led_str_t* lstr, led_str_t* lstr_src, size_t start, size_t stop) {
    led_str_foreach_char_zn(lstr_src, start, stop)
        if (lstr->len+1 < lstr->size )
            lstr->str[lstr->len++] = foreach.c;
        else break;
    lstr->str[lstr->len] = '\0';
    return lstr;
}

inline led_str_t* led_str_app_uchar(led_str_t* lstr, led_uchar_t uc) {
    size_t uc_size = led_uchar_size(uc);
    if (lstr->len + uc_size < lstr->size) {
        led_uchar_to_str(lstr->str + lstr->len, uc);
        lstr->len += uc_size;
        lstr->str[lstr->len] = '\0';
    }
    return lstr;
}

inline led_str_t* led_str_trunk_uchar(led_str_t* lstr, led_uchar_t uc) {
    size_t idx = led_uchar_pos_prev(lstr->str, lstr->len);
    if (uc == led_uchar_of_str(lstr->str + idx)) {
        lstr->len = idx;
        lstr->str[lstr->len] = '\0';
    }
    return lstr;
}

inline led_str_t* led_str_trunk_uchar_last(led_str_t* lstr) {
    while ( lstr->len > 0 && led_uchar_iscont(--(lstr->len)) );
    lstr->str[lstr->len] = '\0';
    return lstr;
}

inline led_str_t* led_str_trunk(led_str_t* lstr, size_t len) {
    if (len < lstr->len) {
        lstr->len = len;
        lstr->str[lstr->len] = '\0';
    }
    return lstr;
}

inline led_str_t* led_str_trunk_end(led_str_t* lstr, size_t len) {
    if (len < lstr->len) {
        lstr->len -= len;
        lstr->str[lstr->len] = '\0';
    }
    return lstr;
}

inline led_str_t* led_str_rtrim(led_str_t* lstr) {
    while (lstr->len > 0 && isspace(lstr->str[lstr->len-1])) lstr->len--;
    lstr->str[lstr->len] = '\0';
    return lstr;
}

inline led_str_t* led_str_ltrim(led_str_t* lstr) {
    size_t i=0,j=0;
    for (; i < lstr->len && isspace(lstr->str[i]); i++);
    for (; i < lstr->len; i++,j++) lstr->str[j] = lstr->str[i];
    lstr->len = j;
    lstr->str[lstr->len] = '\0';
    return lstr;
}

inline led_str_t* led_str_trim(led_str_t* lstr) {
    return led_str_ltrim(led_str_rtrim(lstr));
}

led_str_t* led_str_cut_next(led_str_t* lstr, led_uchar_t uc, led_str_t* stok);

inline led_uchar_t led_str_uchar_at(led_str_t* lstr, size_t idx) {
    if (led_uchar_iscont(lstr->str[idx])) return '\0';
    led_uchar_t uc;
    led_uchar_from_str(lstr->str + idx, &uc);
    return uc;
}

inline led_uchar_t led_str_uchar_size_at(led_str_t* lstr, size_t idx) {
    return idx <= lstr->len ? led_uchar_size_str(lstr->str + idx): 0;
}

inline led_uchar_t led_str_uchar_first(led_str_t* lstr) {
    return led_str_uchar_at(lstr, 0);
}

inline led_uchar_t led_str_uchar_last(led_str_t* lstr) {
    size_t idx = led_uchar_pos_prev(lstr->str, lstr->len);
    return led_uchar_of_str(lstr->str + idx);
}

inline led_uchar_t led_str_uchar_next(led_str_t* lstr, size_t idx, size_t* newidx) {
    led_uchar_t uc;
    size_t ucharlen = led_uchar_from_str(lstr->str + idx, &uc);
    if (newidx) *newidx += ucharlen;
    return uc;
}

inline size_t led_str_pos_uchar_next(led_str_t* lstr, size_t idx) {
    return led_uchar_pos_next(lstr->str, idx);
}

inline led_uchar_t led_str_uchar_prev(led_str_t* lstr, size_t idx, size_t* newidx) {
    idx = led_uchar_pos_prev(lstr->str, idx);
    if (newidx) *newidx = idx;
    return led_uchar_of_str(lstr->str + idx);
}

inline size_t led_str_pos_uchar_prev(led_str_t* lstr, size_t idx) {
    return led_uchar_pos_prev(lstr->str, idx);
}


inline led_uchar_t led_str_uchar_n(led_str_t* lstr, size_t n) {
    led_str_foreach_uchar(lstr)
        if (foreach.uc_count == n)
            return foreach.uc;
    return '\0';
}
inline char* led_str_str_at(led_str_t* lstr, size_t idx) {
    if (led_uchar_iscont(lstr->str[idx])) return '\0';
    return lstr->str + idx;
}

inline bool led_str_equal(led_str_t* lstr1, led_str_t* lstr2) {
    return lstr1->len == lstr2->len && strcmp(lstr1->str, lstr2->str) == 0;
}

inline bool led_str_equal_str(led_str_t* lstr, const char* str) {
    return strcmp(lstr->str, str) == 0;
}

inline bool led_str_equal_str_at(led_str_t* lstr, const char* str, size_t idx) {
    if ( idx > lstr->len ) return false;
    return strcmp(lstr->str + idx, str) == 0;
}

inline bool led_str_startswith_at(led_str_t* lstr1, led_str_t* lstr2, size_t start) {
    led_str_foreach_char(lstr2) {
        size_t i = foreach.i + start;
        if (i >= led_str_len(lstr1) || foreach.c != led_str_str(lstr1)[i]) return false;
    }
    return true;
}

inline bool led_str_startswith(led_str_t* lstr1, led_str_t* lstr2) {
    return led_str_startswith_at(lstr1, lstr2, 0);
}

inline bool led_str_startswith_str_at(led_str_t* lstr, const char* str, size_t start) {
    led_str_decl_str(lstr2, str);
    return led_str_startswith_at(lstr, &lstr2, start);
}

inline bool led_str_startswith_str(led_str_t* lstr, const char* str) {
    return led_str_startswith_str_at(lstr, str, 0);
}

inline size_t led_str_find_uchar_zn(led_str_t* lstr, led_uchar_t uc, size_t start, size_t stop) {
    led_str_foreach_uchar_zn(lstr, start, stop)
        if (foreach.uc == uc) return foreach.i;
    return led_str_len(lstr);
}

inline size_t led_str_find_uchar(led_str_t* lstr, led_uchar_t uc) {
    return led_str_find_uchar_zn(lstr, uc, 0, lstr->len);
}

inline size_t led_str_rfind_uchar_zn(led_str_t* lstr, led_uchar_t uc, size_t start, size_t stop) {
    led_str_foreach_uchar_zn_r(lstr, start, stop)
        if (foreach.uc == uc) return foreach.i;
    return led_str_len(lstr);
}

inline size_t led_str_rfind_uchar(led_str_t* lstr, led_uchar_t uc) {
    return led_str_rfind_uchar_zn(lstr, uc, 0, lstr->len);
}

inline bool led_str_has_uchar(led_str_t* lstr, led_uchar_t uc) {
    return led_str_find_uchar(lstr, uc) < lstr->len;
}

inline size_t led_str_find(led_str_t* lstr, led_str_t* lstr_sub) {
    size_t isub=0;
    led_str_foreach_char(lstr)
        if (isub == lstr_sub->len)
            return foreach.i - isub;
        else if (lstr_sub->str[isub] == foreach.c)
            isub++;
        else {
            foreach.i -= isub;
            isub = 0;
        }
    return lstr->len;
}

inline size_t led_str_find_str(led_str_t* lstr, const char* str_sub) {
    size_t isub=0;
    led_str_foreach_char(lstr)
        if (!str_sub[isub])
            return foreach.i - isub;
        else if (str_sub[isub] == foreach.c)
            isub++;
        else {
            foreach.i -= isub;
            isub = 0;
        }
    return lstr->len;
}

inline led_str_t* led_str_basename(led_str_t* lstr) {
    lstr->str = basename(lstr->str);
    lstr->len = strlen(lstr->str);
    lstr->size = lstr->len + 1;
    return lstr;
}

inline led_str_t* led_str_dirname(led_str_t* lstr) {
    lstr->str = basename(lstr->str);
    lstr->len = strlen(lstr->str);
    lstr->size = lstr->len + 1;
    return lstr;
}

//-----------------------------------------------
// LED string pcre management
//-----------------------------------------------

#define LED_RGX_NO_MATCH 0
#define LED_RGX_STR_MATCH 1
#define LED_RGX_GROUP_MATCH 2

extern pcre2_code* LED_REGEX_ALL_LINE;
extern pcre2_code* LED_REGEX_ALL_MULTILINE;
extern pcre2_code* LED_REGEX_BLANK_LINE;
extern pcre2_code* LED_REGEX_INTEGER;
extern pcre2_code* LED_REGEX_REGISTER;
extern pcre2_code* LED_REGEX_FUNC;
extern pcre2_code* LED_REGEX_FUNC2;

void led_regex_init();
void led_regex_free();

pcre2_code* led_regex_compile(const char* pat, size_t opt);
bool led_str_match(led_str_t* lstr, pcre2_code* regex);
bool led_str_match_offset(led_str_t* lstr, pcre2_code* regex, size_t* pzone_start, size_t* pzone_stop);

inline pcre2_code* led_str_regex_compile(led_str_t* pat, size_t opt) {
    return led_regex_compile(pat->str, opt);
}

inline bool led_str_match_pat(led_str_t* lstr, const char* pat) {
    // this function is allways used for single line match
    return led_str_match(lstr, led_regex_compile(pat, 0));
}

inline bool led_str_isblank(led_str_t* lstr) {
    return led_str_match(lstr, LED_REGEX_BLANK_LINE) > 0;
}

//-----------------------------------------------
// LED constants
//-----------------------------------------------

#define LED_BUF_MAX 0x8000
#define LED_FARG_MAX 3
#define LED_SEL_MAX 2
#define LED_FUNC_MAX 16
#define LED_FNAME_MAX 0x1000
#define LED_REG_MAX 10

#define SEL_TYPE_NONE 0
#define SEL_TYPE_REGEX 1
#define SEL_TYPE_COUNT 2
#define SEL_COUNT 2

#define LED_EXIT_STD 0
#define LED_EXIT_VAL 1

#define LED_INPUT_STDIN 0
#define LED_INPUT_FILE 1

#define LED_OUTPUT_STDOUT 0
#define LED_OUTPUT_FILE_INPLACE 1
#define LED_OUTPUT_FILE_WRITE 2
#define LED_OUTPUT_FILE_APPEND 3
#define LED_OUTPUT_FILE_NEWEXT 4
#define LED_OUTPUT_FILE_DIR 5

#define ARGS_SEC_SELECT 0
#define ARGS_SEC_FUNCT 1
#define ARGS_SEC_FILES 2

//-----------------------------------------------
// LED line management
//-----------------------------------------------

typedef struct {
    led_str_t lstr;
    char buf[LED_BUF_MAX+1];
    size_t zone_start;
    size_t zone_stop;
    bool selected;
} led_line_t;

inline led_line_t* led_line_reset(led_line_t* pline) {
    memset(pline, 0, sizeof *pline);
    return pline;
}

inline led_line_t* led_line_init(led_line_t* pline) {
    led_line_reset(pline);
    led_str_init_buf(&pline->lstr, pline->buf);
    return pline;
}

inline led_line_t* led_line_cpy(led_line_t* pline, led_line_t* pline_src) {
    pline->buf[0] = '\0';
    if (led_str_isinit(&pline_src->lstr)) {
        led_str_init_buf(&pline->lstr, pline->buf);
        led_str_cpy(&pline->lstr, &pline_src->lstr);
    }
    else
        led_str_reset(&pline->lstr);
    pline->selected = pline_src->selected;
    pline->zone_start = 0;
    pline->zone_stop = led_str_len(&pline_src->lstr);
    return pline;
}

inline bool led_line_isinit(led_line_t* pline) {
    return led_str_isinit(&pline->lstr);
}

inline bool led_line_select(led_line_t* pline, bool selected) {
    pline->selected = selected;
    return selected;
}

inline bool led_line_isselected(led_line_t* pline) {
    return pline->selected;
}

inline led_line_t* led_line_append_zn(led_line_t* pline, led_line_t* pline_src) {
    led_str_app_zn(&pline->lstr, &pline_src->lstr, pline_src->zone_start, pline_src->zone_stop);
    return pline;
}

inline led_line_t* led_line_append_before_zn(led_line_t* pline, led_line_t* pline_src) {
    led_str_app_zn(&pline->lstr, &pline_src->lstr, 0, pline_src->zone_start);
    return pline;
}

inline led_line_t* led_line_append_after_zn(led_line_t* pline, led_line_t* pline_src) {
    led_str_app_zn(&pline->lstr, &pline_src->lstr, pline_src->zone_stop, pline_src->lstr.len);
    return pline;
}

//-----------------------------------------------
// LED function management
//-----------------------------------------------

typedef struct {
    size_t id;
    pcre2_code* regex;
    char tmp_buf[LED_BUF_MAX+1];

    struct {
        led_str_t lstr;
        long val;
        size_t uval;
    } arg[LED_FARG_MAX];
    size_t arg_count;
} led_fn_t;

typedef void (*led_fn_impl)(led_fn_t*);

typedef struct {
    const char* short_name;
    const char* long_name;
    led_fn_impl impl;
    const char* args_fmt;
    const char* help_desc;
    const char* help_format;
} led_fn_desc_t;

void led_fn_config();

led_fn_desc_t* led_fn_table_descriptor(size_t fn_id);
size_t led_fn_table_size();

//-----------------------------------------------
// LED runtime
//-----------------------------------------------

void led_free();

typedef struct {
    // options
    struct {
        bool help;
        bool verbose;
        bool report;
        bool quiet;
        bool exit_mode;
        bool invert_selected;
        bool pack_selected;
        bool output_selected;
        bool output_match;
        bool filter_blank;
        int file_in;
        int file_out;
        bool file_out_unchanged;
        bool file_out_extn;
        bool exec;
        led_str_t file_out_ext;
        led_str_t file_out_dir;
        led_str_t file_out_path;
    } opt;

    // selector
    struct {
        int type_start;
        pcre2_code* regex_start;
        size_t val_start;

        int type_stop;
        pcre2_code* regex_stop;
        size_t val_stop;

        size_t total_count;
        size_t count;
        size_t shift;
        bool selected;
        bool inboundary;
    } sel;

    led_fn_t func_list[LED_FUNC_MAX];
    size_t func_count;

    struct {
        size_t line_read_count;
        size_t line_match_count;
        size_t line_write_count;
        size_t file_in_count;
        size_t file_out_count;
        size_t file_match_count;
    } report;

    // files
    char**  file_names;
    size_t  file_count;
    bool     stdin_ispipe;
    bool     stdout_ispipe;

    // runtime variables
    struct {
        led_str_t name;
        char buf_name[LED_FNAME_MAX+1];
        FILE* file;
    } file_in;
    struct {
        led_str_t name;
        char buf_name[LED_FNAME_MAX+1];
        FILE* file;
    } file_out;

    led_line_t line_read;
    led_line_t line_prep;
    led_line_t line_write;

    led_line_t line_reg[LED_REG_MAX];

    PCRE2_UCHAR8 buf_message[LED_MSG_MAX+1];

} led_t;

extern led_t led;
