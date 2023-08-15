#include <limits.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

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
} led_line_struct;

typedef struct {
    // options
    int o_help;
    int o_verbose;
    int o_report;
    int o_quiet;
    int o_zero;
    int o_exit_mode;
    int o_sel_invert;
    int o_sel_block;
    int o_output_selected;
    int o_output_match;
    int o_filter_empty;
    int o_file_in;
    int o_file_out;
    int o_file_out_unchanged;
    int o_file_out_mode;
    int o_file_out_extn;
    const char* o_file_out_ext;
    const char* o_file_out_dir;
    const char* o_file_out_path;

    // selector
    struct {
        int type;
        pcre2_code* regex;
        size_t val;
    } sel[LED_SEL_MAX];

    // processor function Id
    size_t fn_id;

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
        char* name;
        FILE* file;
    } curfile;

    struct {
        size_t count;
        int selected;
        int sel_switch;
        size_t sel_count;
        size_t sel_shift;
    } curline;

    led_line_struct line_src;
    led_line_struct line_dst;

    // runtime buffers
    char buf_fname[LED_FNAME_MAX+1];
    PCRE2_UCHAR8 buf_message[LED_MSG_MAX+1];

} led_struct;

extern led_struct led;

int led_line_reset();
int led_line_init();
int led_line_copy();
int led_line_append();
int led_line_append_zone();
int led_line_append_before_zone();
int led_line_append_after_zone();
int led_line_append_str(const char* str);
int led_line_append_str_len(const char* str, size_t len);
int led_line_append_char(const char c);
int led_line_append_str_start_len(const char* str, size_t start, size_t len);
int led_line_append_str_start_stop(const char* str, size_t start, size_t stop);

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

int led_str_trim(char* line);
int led_str_equal(const char* str1, const char* str2);
int led_str_equal_len(const char* str1, const char* str2, int len);
void led_regex_init();
void led_regex_free();
pcre2_code* led_regex_compile(const char* pattern);
int led_regex_match(pcre2_code* regex, const char* line, int len);
int led_regex_match_offset(pcre2_code* regex, const char* line, int len, size_t* offset, size_t* length);
int led_str_match(const char* str_regex, const char* str);
