#include <limits.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <libgen.h>
#include <stdbool.h>

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
// LED simple poor string management
// thanks to very clear explanations from
// https://dev.to/rdentato/utf-8-strings-in-c-2-3-3kp1
//------------------------------------------------------------------------------

typedef uint32_t u8c_t;

extern const size_t led_u8c_size_table[];

inline size_t led_u8c_size(char* str) {
    return led_u8c_size_table[(((uint8_t *)(str))[0] & 0xFF) >> 4];
}

inline bool led_u8c_iscont(char c) {
    return (c & 0xC0) == 0x80;
}

inline bool led_u8c_isalnum(u8c_t c) {
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

inline bool led_u8c_isdigit(u8c_t c) {
    return (c >= '0' && c <= '9');
}

inline u8c_t led_u8c_tolower(u8c_t c) {
    if (c >= 'A' && c <= 'Z') c = tolower((char)c);
    return c;
}
inline u8c_t led_u8c_toupper(u8c_t c) {
    if (c >= 'a' && c <= 'z') c = toupper((char)c);
    return c;
}

bool led_u8c_isvalid(u8c_t c);

u8c_t led_u8c_encode(uint32_t code);
uint32_t led_u8c_decode(u8c_t c);

size_t led_u8c_from_str(char* str, u8c_t* u8chr);
size_t led_u8c_from_rstr(char* str, size_t len, u8c_t* u8chr);
size_t led_u8c_to_str(char* str, u8c_t u8chr);


//------------------------------------------------------------------------------
// LED poor & simple string management without memmory allocation.
// led strings only wraps buffers declared statically or in the stack
// to offer various management functions easyer.
//------------------------------------------------------------------------------

typedef struct {
    char* str;
    size_t len;
    size_t size;
} led_u8s_t;

#define led_u8s_init_buf(VAR,BUF) led_u8s_init(VAR,BUF,sizeof(BUF))
#define led_u8s_init_str(VAR,STR) led_u8s_init(VAR,(char*)STR,0)

#define led_u8s_decl_str(VAR,STR) \
    led_u8s_t VAR; \
    led_u8s_init_str(&VAR,STR)

#define led_u8s_decl(VAR,LEN) \
    led_u8s_t VAR; \
    char VAR##_buf[LEN]; \
    VAR##_buf[0] = '\0'; \
    led_u8s_init_buf(&VAR,VAR##_buf)

#define led_u8s_decl_cpy(VAR,SRC) \
    led_u8s_t VAR; \
    char VAR##_buf[SRC.len]; \
    led_u8s_init(&VAR,VAR##_buf,SRC.len); \
    led_u8s_cpy(&VAR, &SRC)

inline size_t led_u8s_len(led_u8s_t* lstr) {
    return lstr->len;
}

inline char* led_u8s_str(led_u8s_t* lstr) {
    return lstr->str;
}

inline size_t led_u8s_size(led_u8s_t* lstr) {
    return lstr->size;
}

inline bool led_u8s_isinit(led_u8s_t* lstr) {
    return lstr->str != NULL;
}

inline bool led_u8s_isempty(led_u8s_t* lstr) {
    return led_u8s_isinit(lstr) && lstr->len == 0;
}

inline bool led_u8s_iscontent(led_u8s_t* lstr) {
    return led_u8s_isinit(lstr) && lstr->len > 0;
}

inline bool led_u8s_isfull(led_u8s_t* lstr) {
    return led_u8s_isinit(lstr) && lstr->len + 1 == lstr->size;
}

inline led_u8s_t* led_u8s_reset(led_u8s_t* lstr) {
    memset(lstr, 0, sizeof(*lstr));
    return lstr;
}

led_u8s_t* led_u8s_init(led_u8s_t* lstr, char* buf, size_t size);

inline led_u8s_t* led_u8s_empty(led_u8s_t* lstr) {
    lstr->str[0] = '\0';
    lstr->len = 0;
    return lstr;
}

inline led_u8s_t* led_u8s_clone(led_u8s_t* lstr, led_u8s_t* lstr_src) {
    lstr->str = lstr_src->str;
    lstr->len = lstr_src->len;
    lstr->size = lstr_src->size;
    return lstr;
}

inline led_u8s_t* led_u8s_cpy(led_u8s_t* lstr, led_u8s_t* lstr_src) {
    for(lstr->len = 0; lstr->len < lstr_src->len && lstr->len + 1 < lstr->size; lstr->len++)
        lstr->str[lstr->len] = lstr_src->str[lstr->len];
    lstr->str[lstr->len] = '\0';
    return lstr;
}

inline led_u8s_t* led_u8s_cpy_chars(led_u8s_t* lstr, const char* str) {
    for(lstr->len=0; str[lstr->len] && lstr->len+1 < lstr->size; lstr->len++)
        lstr->str[lstr->len]=str[lstr->len];
    lstr->str[lstr->len] = '\0';
    return lstr;
}

inline led_u8s_t* led_u8s_app(led_u8s_t* lstr, led_u8s_t* lstr_src) {
    for(size_t i = 0; i<lstr_src->len && lstr->len+1 < lstr->size; i++, lstr->len++)
        lstr->str[lstr->len] = lstr_src->str[i];
    lstr->str[lstr->len] = '\0';
    return lstr;
}

inline led_u8s_t* led_u8s_app_str(led_u8s_t* lstr, const char* str) {
    for(size_t i = 0; str[i] && lstr->len+1 < lstr->size; i++, lstr->len++)
        lstr->str[lstr->len] = str[i];
    lstr->str[lstr->len] = '\0';
    return lstr;
}

inline led_u8s_t* led_u8s_app_zn(led_u8s_t* lstr, led_u8s_t* lstr_src, size_t start, size_t stop) {
    for(size_t i = start; i < stop && lstr_src->str[i] && lstr->len+1 < lstr->size; i++, lstr->len++)
        lstr->str[lstr->len] = lstr_src->str[i];
    lstr->str[lstr->len] = '\0';
    return lstr;
}

inline led_u8s_t* led_u8s_app_char(led_u8s_t* lstr, u8c_t u8chr) {
    char buf[4];
    char* str = buf;
    size_t u8chr_len = led_u8c_to_str(str, u8chr);
    if (lstr->len + u8chr_len < lstr->size) {
        while (u8chr_len) {
            lstr->str[lstr->len++] = *(str++);
            u8chr_len--;
        }
        lstr->str[lstr->len] = '\0';
    }
    return lstr;
}

inline led_u8s_t* led_str_trunk_char(led_u8s_t* lstr, u8c_t u8chr) {
    u8c_t c = 0;
    size_t u8chr_len = led_u8c_from_rstr(lstr->str, lstr->len, &c);
    // led_debug("led_str_trunk_char - len=%lu", u8chr_len);
    if (u8chr == c) {
        lstr->len -= u8chr_len;
        lstr->str[lstr->len] = '\0';
    }
    return lstr;
}

inline led_u8s_t* led_str_trunk_char_last(led_u8s_t* lstr) {
    while ( lstr->len > 0 && led_u8c_iscont(--(lstr->len)) );
    lstr->str[lstr->len] = '\0';
    return lstr;
}

inline led_u8s_t* led_str_trunk(led_u8s_t* lstr, size_t len) {
    if (len < lstr->len) {
        lstr->len = len;
        lstr->str[lstr->len] = '\0';
    }
    return lstr;
}

inline led_u8s_t* led_str_trunk_end(led_u8s_t* lstr, size_t len) {
    if (len < lstr->len) {
        lstr->len -= len;
        lstr->str[lstr->len] = '\0';
    }
    return lstr;
}

inline led_u8s_t* led_u8s_rtrim(led_u8s_t* lstr) {
    while(lstr->len > 0 && isspace(lstr->str[lstr->len-1])) lstr->len--;
    lstr->str[lstr->len] = '\0';
    return lstr;
}

inline led_u8s_t* led_u8s_ltrim(led_u8s_t* lstr) {
    size_t i=0,j=0;
    for(; i < lstr->len && isspace(lstr->str[i]); i++);
    for(; i < lstr->len; i++,j++)
        lstr->str[j] = lstr->str[i];
    lstr->len = j;
    lstr->str[lstr->len] = '\0';
    return lstr;
}

inline led_u8s_t* led_str_trim(led_u8s_t* lstr) {
    return led_u8s_ltrim(led_u8s_rtrim(lstr));
}

inline led_u8s_t* led_u8s_cut_next(led_u8s_t* lstr, u8c_t u8chr, led_u8s_t* stok) {
    led_u8s_clone(stok, lstr);
    // led_debug("led_u8s_cut_next - lstr=%s tok=%s", lstr->str, stok->str);
    u8c_t c;
    size_t i = 0;
    while(i < lstr->len) {
        size_t l = led_u8c_from_str(lstr->str + i, &c);
        // led_debug("led_u8s_cut_next - i=%u c=%x l=%u", i, c, l);
        if ( c == u8chr ) {
            stok->len = i;
            stok->str[i] = '\0';
            lstr->len -= i+l;
            lstr->str += i+1;
            // led_debug("led_u8s_cut_next - lstr=%s tok=%s", lstr->str, stok->str);
            return lstr;
        }
        i += l;
    }
    stok->len = lstr->len;
    lstr->str = lstr->str + lstr->len;
    lstr->len = 0;
    return lstr;
}

inline u8c_t led_u8s_char_at(led_u8s_t* lstr, size_t idx) {
    if (led_u8c_iscont(lstr->str[idx])) return '\0';
    u8c_t u8chr;
    led_u8c_from_str(lstr->str + idx, &u8chr);
    return u8chr;
}

inline u8c_t led_u8s_char_first(led_u8s_t* lstr) {
    return led_u8s_char_at(lstr, 0);
}

inline u8c_t led_u8s_char_last(led_u8s_t* lstr) {
    if (lstr->len == 0) return '\0';
    u8c_t u8chr = 0;
    size_t idx = lstr->len;
    while ( idx > 0 && led_u8c_iscont(lstr->str[--idx]) );
    // led_debug("led_u8s_char_last - len=%lu idx=%lu", lstr->len, idx);
    led_u8c_from_str(lstr->str + idx, &u8chr);
    return u8chr;
}

inline u8c_t led_u8s_char_next(led_u8s_t* lstr, size_t* idx) {
    u8c_t u8chr;
    *idx  += led_u8c_from_str(lstr->str + *idx, &u8chr);
    return u8chr;
}

inline char* led_u8s_str_at(led_u8s_t* lstr, size_t idx) {
    if (led_u8c_iscont(lstr->str[idx])) return '\0';
    return lstr->str + idx;
}

inline bool led_u8s_equal(led_u8s_t* lstr1, led_u8s_t* lstr2) {
    return lstr1->len == lstr2->len && strcmp(lstr1->str, lstr2->str) == 0;
}

inline bool led_u8s_equal_str(led_u8s_t* lstr, const char* str) {
    return strcmp(lstr->str, str) == 0;
}

inline bool led_u8s_equal_str_at(led_u8s_t* lstr, const char* str, size_t idx) {
    if ( idx > lstr->len ) return false;
    return strcmp(lstr->str + idx, str) == 0;
}

inline bool led_u8s_startswith(led_u8s_t* lstr1, led_u8s_t* lstr2) {
    size_t i = 0;
    for (; i < lstr1->len && lstr2->str[i] && lstr1->str[i] == lstr2->str[i]; i++);
    return lstr2->str[i] == '\0';
}

inline bool led_u8s_startswith_at(led_u8s_t* lstr1, led_u8s_t* lstr2, size_t start) {
    if ( led_u8c_iscont(lstr1->str[start]) ) return false;
    size_t i = 0;
    for (; start < lstr1->len && i < lstr2->len && lstr1->str[start] == lstr2->str[i]; i++, start++);
    return lstr2->str[i] == '\0';
}

inline bool led_u8s_startswith_str(led_u8s_t* lstr, const char* str) {
    size_t i = 0;
    for (; i < lstr->len && str[i] && lstr->str[i] == str[i]; i++);
    return str[i] == '\0';
}

inline bool led_u8s_startswith_str_at(led_u8s_t* lstr, const char* str, size_t start) {
    if ( led_u8c_iscont(lstr->str[start]) ) return false;
    size_t i = 0;
    for (; start < lstr->len && str[i] && lstr->str[start] == str[i]; i++, start++);
    return str[i] == '\0';
}

inline size_t led_u8s_find_char_zn(led_u8s_t* lstr, u8c_t c, size_t start, size_t stop) {
    while( start < stop ) {
        size_t pos = start;
        if (led_u8s_char_next(lstr, &start) == c) return pos;
    }
    return lstr->len;
}

inline size_t led_u8s_find_char(led_u8s_t* lstr, u8c_t c) {
    return led_u8s_find_char_zn(lstr, c, 0, lstr->len);
}

inline size_t led_u8s_rfind_char_zn(led_u8s_t* lstr, u8c_t c, size_t start, size_t stop) {
    u8c_t u8chr;
    while( stop > start )
        if ( !led_u8c_iscont(lstr->str[--stop]) ) {
            led_u8c_from_str(lstr->str + stop, &u8chr);
            if ( u8chr == c ) return stop;
        }
    return lstr->len;
}

inline size_t led_u8s_rfind_char(led_u8s_t* lstr, u8c_t c) {
    return led_u8s_rfind_char_zn(lstr, c, 0, lstr->len);
}

inline bool led_u8s_ischar(led_u8s_t* lstr, u8c_t c) {
    return led_u8s_find_char(lstr, c) < lstr->len;
}

inline size_t led_u8s_find(led_u8s_t* lstr1, led_u8s_t* lstr2) {
    size_t i=0, j=0;
    for(; i < lstr1->len && lstr2->str[j]; i++)
        if (lstr1->str[i] == lstr2->str[j]) j++;
        else j = 0;
    return lstr2->str[j] ? lstr1->len: i - j;
}

inline led_u8s_t* led_u8s_basename(led_u8s_t* lstr) {
    lstr->str = basename(lstr->str);
    lstr->len = strlen(lstr->str);
    lstr->size = lstr->len + 1;
    return lstr;
}

inline led_u8s_t* led_u8s_dirname(led_u8s_t* lstr) {
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
extern pcre2_code* LED_REGEX_BLANK_LINE;
extern pcre2_code* LED_REGEX_INTEGER;
extern pcre2_code* LED_REGEX_REGISTER;
extern pcre2_code* LED_REGEX_FUNC;
extern pcre2_code* LED_REGEX_FUNC2;

void led_regex_init();
void led_regex_free();

pcre2_code* led_regex_compile(const char* pat);
bool led_u8s_match(led_u8s_t* lstr, pcre2_code* regex);
bool led_u8s_match_offset(led_u8s_t* lstr, pcre2_code* regex, size_t* pzone_start, size_t* pzone_stop);

inline pcre2_code* led_u8s_regex_compile(led_u8s_t* pat) {
    return led_regex_compile(pat->str);
}

inline bool led_u8s_match_pat(led_u8s_t* lstr, const char* pat) {
    return led_u8s_match(lstr, led_regex_compile(pat));
}

inline bool led_u8s_isblank(led_u8s_t* lstr) {
    return led_u8s_match(lstr, LED_REGEX_BLANK_LINE) > 0;
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
    led_u8s_t lstr;
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
    led_u8s_init_buf(&pline->lstr, pline->buf);
    return pline;
}

inline led_line_t* led_line_cpy(led_line_t* pline, led_line_t* pline_src) {
    pline->buf[0] = '\0';
    if (led_u8s_isinit(&pline_src->lstr)) {
        led_u8s_init_buf(&pline->lstr, pline->buf);
        led_u8s_cpy(&pline->lstr, &pline_src->lstr);
    }
    else
        led_u8s_reset(&pline->lstr);
    pline->selected = pline_src->selected;
    pline->zone_start = 0;
    pline->zone_stop = led_u8s_len(&pline_src->lstr);
    return pline;
}

inline bool led_line_isinit(led_line_t* pline) {
    return led_u8s_isinit(&pline->lstr);
}

inline bool led_line_select(led_line_t* pline, bool selected) {
    pline->selected = selected;
    return selected;
}

inline bool led_line_isselected(led_line_t* pline) {
    return pline->selected;
}

inline led_line_t* led_line_append_zone(led_line_t* pline, led_line_t* pline_src) {
    led_u8s_app_zn(&pline->lstr, &pline_src->lstr, pline_src->zone_start, pline_src->zone_stop);
    return pline;
}

inline led_line_t* led_line_append_before_zone(led_line_t* pline, led_line_t* pline_src) {
    led_u8s_app_zn(&pline->lstr, &pline_src->lstr, 0, pline_src->zone_start);
    return pline;
}

inline led_line_t* led_line_append_after_zone(led_line_t* pline, led_line_t* pline_src) {
    led_u8s_app_zn(&pline->lstr, &pline_src->lstr, pline_src->zone_stop, pline_src->lstr.len);
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
        led_u8s_t lstr;
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
        led_u8s_t file_out_ext;
        led_u8s_t file_out_dir;
        led_u8s_t file_out_path;
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
        size_t line_match_count;
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
        led_u8s_t name;
        char buf_name[LED_FNAME_MAX+1];
        FILE* file;
    } file_in;
    struct {
        led_u8s_t name;
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

void led_init(int argc, char* argv[]);
void led_free();
bool led_init_opt(led_u8s_t* arg);
bool led_init_func(led_u8s_t* arg);
bool led_init_sel(led_u8s_t* arg);
void led_init_config();
void led_help();

void led_file_open_in();
void led_file_close_in();
void led_file_stdin();
void led_file_open_out();
void led_file_close_out();
void led_file_print_out();
void led_file_stdout();
bool led_file_next();

bool led_process_read();
void led_process_write();
void led_process_exec();
bool led_process_selector();
void led_process_functions();
void led_report();