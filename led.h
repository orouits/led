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

#define LED_SUCCESS 0
#define LED_ERR_ARG 1
#define LED_ERR_PCRE 2
#define LED_ERR_FILE 3
#define LED_ERR_MAXLINE 4
#define LED_ERR_INTERNAL 5

#define LED_SEL_MAX 2
#define LED_FARG_MAX 3
#define LED_BUF_MAX 0x8000
#define LED_FNAME_MAX 0x1000
#define LED_MSG_MAX 0x1000

#define LED_RGX_NO_MATCH 0
#define LED_RGX_STR_MATCH 1
#define LED_RGX_GROUP_MATCH 2

//-----------------------------------------------
// LED runtime data structure
//-----------------------------------------------

typedef struct {
    char* str;
    char buf[LED_BUF_MAX];
    size_t len;
    size_t zone_start;
    size_t zone_stop;
    int selected;
} led_line_struct;

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
        const char* file_out_ext;
        const char* file_out_dir;
        const char* file_out_path;
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

    // processor function Id
    size_t fn_id;
    pcre2_code* fn_regex;

    struct {
        const char* str;
        size_t len;
        long val;
        size_t uval;
        pcre2_code* regex;
    } fn_arg[LED_FARG_MAX];

    // files
    char**  file_names;
    size_t  file_count;
    int     stdin_ispipe;
    int     stdout_ispipe;

    // runtime variables
    struct {
        char name[LED_FNAME_MAX+1];
        FILE* file;
    } file_in;
    struct {
        char name[LED_FNAME_MAX+1];
        FILE* file;
    } file_out;

    led_line_struct line_read;
    led_line_struct line_prep;
    led_line_struct line_write;

    PCRE2_UCHAR8 buf_message[LED_MSG_MAX+1];

} led_struct;

extern led_struct led;

int led_line_defined(led_line_struct* pline);
int led_line_len(led_line_struct* pline);
int led_line_selected(led_line_struct* pline);
int led_line_isempty(led_line_struct* pline);
int led_line_isblank(led_line_struct* pline);

char led_line_last_char(led_line_struct* pline);
int led_line_select(led_line_struct* pline, int selected);

size_t led_line_reset(led_line_struct* pline);
size_t led_line_init(led_line_struct* pline);
size_t led_line_copy(led_line_struct* pline, led_line_struct* pline_src);
size_t led_line_append(led_line_struct* pline, led_line_struct* pline_src);
size_t led_line_append_zone(led_line_struct* pline, led_line_struct* pline_src);
size_t led_line_append_before_zone(led_line_struct* pline, led_line_struct* pline_src);
size_t led_line_append_after_zone(led_line_struct* pline, led_line_struct* pline_src);
size_t led_line_append_str(led_line_struct* pline, const char* str);
size_t led_line_append_str_len(led_line_struct* pline, const char* str, size_t len);
size_t led_line_append_char(led_line_struct* pline, const char c);
size_t led_line_append_str_start_len(led_line_struct* pline, const char* str, size_t start, size_t len);
size_t led_line_append_str_start_stop(led_line_struct* pline, const char* str, size_t start, size_t stop);
size_t led_line_unappend_char(led_line_struct* pline, char c);
size_t led_line_search_fist(led_line_struct* pline, char c, size_t start, size_t stop);
size_t led_line_search_last(led_line_struct* pline, char c, size_t start, size_t stop);

//-----------------------------------------------
// LED function management
//-----------------------------------------------

typedef void (*led_fn_impl)();

typedef struct {
    const char* short_name;
    const char* long_name;
    led_fn_impl impl;
    const char* args_fmt;
    const char* help_desc;
    const char* help_format;
} led_fn_struct;

void led_fn_config();

led_fn_struct* led_fn_table_descriptor(size_t fn_id);
size_t led_fn_table_size();

//-----------------------------------------------
// LED trace, error and assertions
//-----------------------------------------------

void led_free();
void led_assert(int cond, int code, const char* message, ...);
void led_assert_pcre(int rc);
void led_debug(const char* message, ...);

//-----------------------------------------------
// LED utilities
//-----------------------------------------------

extern pcre2_code* LED_REGEX_BLANK_LINE;

char* led_str_empty(char* str);
char* led_str_cpy(char* dest, const char* src, int maxlen);
char* led_str_app(char* dest, const char* src, int maxlen);
char* led_str_trunc(char* dest, int size);
char* led_str_trim(char* str);
int led_str_isempty(char* str);
int led_str_equal(const char* str1, const char* str2);
int led_str_equal_len(const char* str1, const char* str2, int len);
int led_char_in_str(char c, const char* str);
int led_char_pos_str(char c, const char* str);
void led_regex_init();
void led_regex_free();
pcre2_code* led_regex_compile(const char* pattern);
int led_regex_match(pcre2_code* regex, const char* str, int len);
int led_regex_match_offset(pcre2_code* regex, const char* str, int len, size_t* offset, size_t* length);
int led_str_match(const char* str_regex, const char* str);
