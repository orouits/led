#include <limits.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <libgen.h>

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

#define FALSE 0
#define TRUE 1

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

void led_assert(int cond, int code, const char* message, ...);
void led_assert_pcre(int rc);
void led_debug(const char* message, ...);

//-----------------------------------------------
// LED simple poor string management
//-----------------------------------------------

typedef struct {
    char* str;
    size_t len;
    size_t size;
} lstr;

#define lstr_init_buf(VAR,BUF) lstr_init(VAR,BUF,sizeof(BUF))
#define lstr_init_str(VAR,STR) lstr_init(VAR,(char*)STR,0)

#define lstr_decl_str(VAR,STR) \
    lstr VAR; \
    lstr_init_str(&VAR,STR)

#define lstr_decl(VAR,LEN) \
    lstr VAR; \
    char VAR##_buf[LEN]; \
    VAR##_buf[0] = '\0'; \
    lstr_init_buf(&VAR,VAR##_buf)

#define lstr_decl_cpy(VAR,SRC) \
    lstr VAR; \
    char VAR##_buf[SRC.len]; \
    lstr_init(&VAR,VAR##_buf,SRC.len); \
    lstr_cpy(&VAR, &SRC)

inline size_t lstr_len(lstr* sval) {
    return sval->len;
}

inline char* lstr_str(lstr* sval) {
    return sval->str;
}

inline size_t lstr_size(lstr* sval) {
    return sval->size;
}

inline int lstr_isinit(lstr* sval) {
    return sval->str != NULL;
}

inline int lstr_isempty(lstr* sval) {
    return lstr_isinit(sval) && sval->len == 0;
}

inline int lstr_iscontent(lstr* sval) {
    return lstr_isinit(sval) && sval->len > 0;
}

inline int lstr_isfull(lstr* sval) {
    return lstr_isinit(sval) && sval->len + 1 == sval->size;
}

inline lstr* lstr_reset(lstr* sval) {
    memset(sval, 0, sizeof(*sval));
    return sval;
}

inline lstr* lstr_init(lstr* sval, char* buf, size_t size) {
    sval->str = buf;
    if (!sval->str) {
        sval->len = 0;
        sval->size = 0;
    }
    else {
        sval->len = strlen(buf);
        sval->size = size > 0 ? size : sval->len + 1;
    }
    return sval;
}

inline lstr* lstr_empty(lstr* sval) {
    sval->str[0] = '\0';
    sval->len = 0;
    return sval;
}

inline lstr* lstr_clone(lstr* sval, lstr* src) {
    sval->str = src->str;
    sval->len = src->len;
    sval->size = src->size;
    return sval;
}

inline lstr* lstr_cpy(lstr* sval, lstr* src) {
    for(sval->len = 0; sval->len < src->len && sval->len + 1 < sval->size; sval->len++)
        sval->str[sval->len] = src->str[sval->len];
    sval->str[sval->len] = '\0';
    return sval;
}

inline lstr* lstr_cpy_chars(lstr* sval, const char* str) {
    for(sval->len=0; str[sval->len] && sval->len+1 < sval->size; sval->len++)
        sval->str[sval->len]=str[sval->len];
    sval->str[sval->len] = '\0';
    return sval;
}

inline lstr* lstr_app(lstr* sval, lstr* src) {
    for(size_t i = 0; i<src->len && sval->len+1 < sval->size; i++, sval->len++)
        sval->str[sval->len] = src->str[i];
    sval->str[sval->len] = '\0';
    return sval;
}

inline lstr* lstr_app_str(lstr* sval, const char* str) {
    for(size_t i = 0; str[i] && sval->len+1 < sval->size; i++, sval->len++)
        sval->str[sval->len] = str[i];
    sval->str[sval->len] = '\0';
    return sval;
}

inline lstr* lstr_app_start_len(lstr* sval, lstr* src, size_t start, size_t len) {
    for(size_t i = start; i < start+len && src->str[i] && sval->len+1 < sval->size; i++, sval->len++)
        sval->str[sval->len] = src->str[i];
    sval->str[sval->len] = '\0';
    return sval;
}

inline lstr* lstr_app_start_stop(lstr* sval, lstr* src, size_t start, size_t stop) {
    for(size_t i = start; i < stop && src->str[i] && sval->len+1 < sval->size; i++, sval->len++)
        sval->str[sval->len] = src->str[i];
    sval->str[sval->len] = '\0';
    return sval;
}

inline lstr* lstr_app_char(lstr* sval, const char c) {
    if (sval->len+1 < sval->size) {
        sval->str[sval->len++] = c;
        sval->str[sval->len] = '\0';
    }
    return sval;
}

inline lstr* lstr_set_char_at(lstr* sval, const char c, size_t i) {
    if (sval->len > i) sval->str[i] = c;
    return sval;
}

inline lstr* lstr_set_last_char(lstr* sval, const char c) {
    if (sval->len > 0) sval->str[sval->len - 1] = c;
    return sval;
}

inline lstr* lstr_set_first_char(lstr* sval, const char c) {
    if (sval->len > 0) sval->str[0] = c;
    return sval;
}

inline lstr* lstr_unapp_char(lstr* sval, const char c) {
    if (sval->len > 0 && sval->str[sval->len-1] == c) {
        sval->len--;
        sval->str[sval->len] = '\0';
    }
    return sval;
}

inline lstr* lstr_trunk(lstr* sval, size_t len) {
    if (len < sval->len) {
        sval->len = len;
        sval->str[sval->len] = '\0';
    }
    return sval;
}

inline lstr* lstr_trunk_end(lstr* sval, size_t len) {
    if (len <= sval->len) {
        sval->len -= len;
        sval->str[sval->len] = '\0';
    }
    return sval;
}

inline lstr* lstr_rtrim(lstr* sval) {
    while(sval->len > 0 && isspace(sval->str[sval->len-1])) sval->len--;
    sval->str[sval->len] = '\0';
    return sval;
}

inline lstr* lstr_ltrim(lstr* sval) {
    size_t i=0,j=0;
    for(; i < sval->len && isspace(sval->str[i]); i++);
    for(; i < sval->len; i++,j++)
        sval->str[j] = sval->str[i];
    sval->len = j;
    sval->str[sval->len] = '\0';
    return sval;
}

inline lstr* lstr_trim(lstr* sval) {
    return lstr_ltrim(lstr_rtrim(sval));
}

inline lstr* lstr_cut_next(lstr* sval, char c, lstr* stok) {
    lstr_clone(stok, sval);
    for(size_t i = 0; i < sval->len; i++)
        if (sval->str[i] == c) {
            stok->len = i;
            stok->str[i++] = '\0';
            sval->len -= i;
            sval->str += i;
            return sval;
        }
    stok->len = sval->len;
    sval->str = sval->str + sval->len;
    sval->len = 0;
    return sval;
}

inline char lstr_char_at(lstr* sval, size_t idx) {
    return sval->str[idx];
}

inline char lstr_first_char(lstr* sval) {
    return sval->str[0];
}

inline char lstr_last_char(lstr* sval) {
    if (sval->len == 0) return '\0';
    return sval->str[sval->len - 1];
}

inline char* lstr_str_at(lstr* sval, size_t idx) {
    return sval->str + idx;
}

inline int lstr_equal(lstr* sval1, lstr* sval2) {
    return sval1->len == sval2->len && strcmp(sval1->str, sval2->str) == 0;
}

inline int lstr_equal_str(lstr* sval, const char* str) {
    return strcmp(sval->str, str) == 0;
}

inline int lstr_equal_str_at(lstr* sval, const char* str, size_t i) {
    if ( i > sval->len ) return FALSE;
    return strcmp(sval->str + i, str) == 0;
}

inline int lstr_startswith(lstr* sval1, lstr* sval2) {
    size_t i = 0;
    for (; i < sval1->len && sval2->str[i] && sval1->str[i] == sval2->str[i]; i++);
    return sval2->str[i] == '\0';
}

inline int lstr_startswith_at(lstr* sval1, lstr* sval2, size_t start) {
    size_t i = 0;
    for (; start < sval1->len && sval2->str[i] && sval1->str[start] == sval2->str[i]; i++, start++);
    return sval2->str[i] == '\0';
}

inline int lstr_startswith_str(lstr* sval, const char* str) {
    size_t i = 0;
    for (; i < sval->len && str[i] && sval->str[i] == str[i]; i++);
    return str[i] == '\0';
}

inline int lstr_startswith_str_at(lstr* sval, const char* str, size_t start) {
    size_t i = 0;
    for (; start < sval->len && str[i] && sval->str[start] == str[i]; i++, start++);
    return str[i] == '\0';
}

inline size_t lstr_find_char_start_stop(lstr* sval, char c, size_t start, size_t stop) {
    for(size_t i = start; i < stop; i++)
        if (sval->str[i] == c) return i;
    return sval->len;
}

inline size_t lstr_find_char(lstr* sval, char c) {
    return lstr_find_char_start_stop(sval, c, 0, sval->len);
}

inline size_t lstr_rfind_char_start_stop(lstr* sval, char c, size_t start, size_t stop) {
    size_t i = stop;
    while( i > start )
        if (sval->str[--i] == c) return i;
    return sval->len;
}

inline size_t lstr_rfind_char(lstr* sval, char c) {
    return lstr_rfind_char_start_stop(sval, c, 0, sval->len);
}

inline int lstr_ischar(lstr* sval, char c) {
    return lstr_find_char(sval, c) < sval->len;
}

inline int lstr_find(lstr* sval1, lstr* sval2) {
    size_t i=0, j=0;
    for(; i < sval1->len && sval2->str[j]; i++)
        if (sval1->str[i] == sval2->str[j]) j++;
        else j = 0;
    return sval2->str[j] ? sval1->len: i - j;
}

inline lstr* lstr_basename(lstr* sval) {
    sval->str = basename(sval->str);
    sval->len = strlen(sval->str);
    sval->size = sval->len + 1;
    return sval;
}

inline lstr* lstr_dirname(lstr* sval) {
    sval->str = basename(sval->str);
    sval->len = strlen(sval->str);
    sval->size = sval->len + 1;
    return sval;
}

//-----------------------------------------------
// LED string pcre management
//-----------------------------------------------

#define LED_RGX_NO_MATCH 0
#define LED_RGX_STR_MATCH 1
#define LED_RGX_GROUP_MATCH 2

extern pcre2_code* LED_REGEX_ALL_LINE;
extern pcre2_code* LED_REGEX_BLANK_LINE;
extern pcre2_code* LED_REGEX_INTEGER;
extern pcre2_code* LED_REGEX_REGISTER;
extern pcre2_code* LED_REGEX_FUNC;
extern pcre2_code* LED_REGEX_FUNC2;

void led_regex_init();
void led_regex_free();

pcre2_code* led_regex_compile(const char* pat);
int lstr_match(lstr* sval, pcre2_code* regex);
int lstr_match_offset(lstr* sval, pcre2_code* regex, size_t* pzone_start, size_t* pzone_stop);

inline pcre2_code* lstr_regex_compile(lstr* pat) {
    return led_regex_compile(pat->str);
}

inline int lstr_match_pat(lstr* sval, const char* pat) {
    return lstr_match(sval, led_regex_compile(pat));
}

inline int lstr_isblank(lstr* sval) {
    return lstr_match(sval, LED_REGEX_BLANK_LINE) > 0;
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
    lstr sval;
    char buf[LED_BUF_MAX+1];
    size_t zone_start;
    size_t zone_stop;
    int selected;
} led_line_t;

inline led_line_t* led_line_reset(led_line_t* pline) {
    memset(pline, 0, sizeof *pline);
    return pline;
}

inline led_line_t* led_line_init(led_line_t* pline) {
    led_line_reset(pline);
    lstr_init_buf(&pline->sval, pline->buf);
    return pline;
}

inline led_line_t* led_line_cpy(led_line_t* pline, led_line_t* pline_src) {
    pline->buf[0] = '\0';
    if (lstr_isinit(&pline_src->sval)) {
        lstr_init_buf(&pline->sval, pline->buf);
        lstr_cpy(&pline->sval, &pline_src->sval);
    }
    else
        lstr_reset(&pline->sval);
    pline->selected = pline_src->selected;
    pline->zone_start = 0;
    pline->zone_stop = lstr_len(&pline_src->sval);
    return pline;
}

inline int led_line_isinit(led_line_t* pline) {
    return lstr_isinit(&pline->sval);
}

inline int led_line_select(led_line_t* pline, int selected) {
    pline->selected = selected;
    return selected;
}

inline int led_line_selected(led_line_t* pline) {
    return pline->selected;
}

inline led_line_t* led_line_append_zone(led_line_t* pline, led_line_t* pline_src) {
    lstr_app_start_stop(&pline->sval, &pline_src->sval, pline_src->zone_start, pline_src->zone_stop);
    return pline;
}

inline led_line_t* led_line_append_before_zone(led_line_t* pline, led_line_t* pline_src) {
    lstr_app_start_stop(&pline->sval, &pline_src->sval, 0, pline_src->zone_start);
    return pline;
}

inline led_line_t* led_line_append_after_zone(led_line_t* pline, led_line_t* pline_src) {
    lstr_app_start_stop(&pline->sval, &pline_src->sval, pline_src->zone_stop, pline_src->sval.len);
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
        lstr sval;
        long val;
        size_t uval;
        pcre2_code* regex;
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
        int help;
        int verbose;
        int report;
        int quiet;
        int exit_mode;
        int invert_selected;
        int pack_selected;
        int output_selected;
        int output_match;
        int filter_blank;
        int file_in;
        int file_out;
        int file_out_unchanged;
        int file_out_extn;
        int exec;
        lstr file_out_ext;
        lstr file_out_dir;
        lstr file_out_path;
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
        int selected;
        int inboundary;
    } sel;

    led_fn_t func_list[LED_FUNC_MAX];
    size_t func_count;

    struct {
        size_t line_match_count;
        size_t file_in_count;
        size_t file_out_count;
        size_t file_match_count;
    } report;

    // files
    char**  file_names;
    size_t  file_count;
    int     stdin_ispipe;
    int     stdout_ispipe;

    // runtime variables
    struct {
        lstr name;
        char buf_name[LED_FNAME_MAX+1];
        FILE* file;
    } file_in;
    struct {
        lstr name;
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
