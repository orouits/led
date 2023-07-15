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

#define LED_SEL_MAX 2
#define LED_FARG_MAX 3
#define LED_LINE_MAX 4096
#define LED_FNAME_MAX 4096
#define LED_MSG_MAX 4096

void led_fn_config();

void led_fn_impl_none();
void led_fn_impl_substitute();
void led_fn_impl_remove();
void led_fn_impl_rangesel();
void led_fn_impl_translate();
void led_fn_impl_caselower();
void led_fn_impl_caseupper();
void led_fn_impl_casefirst();
void led_fn_impl_casecamel();
void led_fn_impl_insert();
void led_fn_impl_append();
void led_fn_impl_quotesimple();
void led_fn_impl_quotedouble();
void led_fn_impl_quoteback();

void led_free();
void led_assert(int cond, int code, const char* message, ...);
void led_assert_pcre(int rc);
void led_debug(const char* message, ...);

int led_str_trim(char* line);
int led_str_equal(const char* str1, const char* str2);
pcre2_code* led_regex_compile(const char* pattern);
int led_regex_match(pcre2_code* regex, const char* line, int len);
int led_regex_match_offset(pcre2_code* regex, const char* line, int len, int* offset, int* length);
int led_str_match(const char* str, const char* regex);

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