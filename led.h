#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <ctype.h>

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
#define LED_LINE_MAX 4096
#define LED_FNAME_MAX 4096
#define LED_MSG_MAX 4096

//-----------------------------------------------
// LED runtime data structure
//-----------------------------------------------

typedef struct {
    // options
    int     o_help;
    int     o_verbose;
    int     o_report;
    int     o_quiet;
    int     o_zero;
    int     o_exit_mode;
    int     o_sel_invert;
    int     o_sel_block;
    int     o_output_selected;
    int     o_filter_empty;
    int     o_file_in;
    int     o_file_out;
    int     o_file_out_unchanged;
    int     o_file_out_mode;
    int     o_file_out_extn;
    const char* o_file_out_ext;
    const char* o_file_out_dir;
    const char* o_file_out_path;

    // selector
    struct {
        int     type;
        pcre2_code* regex;
        long     val;
    } sel[LED_SEL_MAX];

    // processor function Id
    int fn_id;

    struct {
        const unsigned char* str;
        int len;
        long val;
        pcre2_code* regex;
    } fn_arg[LED_FARG_MAX];

    // files
    char**  file_names;
    int     file_count;

    int     stdin_ispipe;
    int     stdout_ispipe;

    // runtime variables
    struct {
        char* name;
        FILE* file;
    } curfile;

    struct {
        unsigned char* str;
        int len;
        long count;
        long count_sel;
        int selected;
    } curline;

    // runtime buffers
    unsigned char buf_fname[LED_FNAME_MAX];
    unsigned char buf_line[LED_LINE_MAX];
    unsigned char buf_line_trans[LED_LINE_MAX];
    unsigned char buf_message[LED_MSG_MAX];

} led_struct;

extern led_struct led;

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

led_fn_struct* led_fn_table_descriptor(int fn_id);
int led_fn_table_size();

//-----------------------------------------------
// LED trace, error and assertions
//-----------------------------------------------

void led_free();
void led_assert(int cond, int code, const char* message, ...);
void led_assert_pcre(int rc);
void led_debug(const char* message, ...);

//-----------------------------------------------
// LED string utilities
//-----------------------------------------------

int led_str_trim(char* line);
int led_str_equal(const char* str1, const char* str2);
int led_str_equal_len(const char* str1, const char* str2, int len);
pcre2_code* led_regex_compile(const char* pattern);
int led_regex_match(pcre2_code* regex, const char* line, int len);
int led_regex_match_offset(pcre2_code* regex, const char* line, int len, int* offset, int* length);
int led_str_match(const char* str, const char* regex);
