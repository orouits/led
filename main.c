#define PCRE2_CODE_UNIT_WIDTH 8

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <pcre2.h>

#define FALSE 0
#define TRUE 1

#define CLIST_SELECT 0
#define CLIST_SELFORMAT 1
#define CLIST_FUNCT 2
#define CLIST_FILES 3

#define SEL_TYPE_NONE 0
#define SEL_TYPE_REGEX 1
#define SEL_TYPE_COUNT 2
#define SEL_COUNT 2

#define SEL_FUNC_LINE 0
#define SEL_FUNC_BLOCK 1

#define FUNC_PRINT 0

#define LED_SUCCESS 0
#define LED_ERR_ARG 1
#define LED_ERR_FILE 2

#define LED_LINE_MAX 4096
#define LED_FNAME_MAX 4096
#define LED_MSG_MAX 4096

struct {
    int     cli_st;
    int     verbose;

    struct {
        int     type;
        pcre2_code* regex;
        long     count;
    } sel[2];

    int     sel_func;

    int     func;
    char*   func_arg[3];

    int     file_mode;
    char**  file_names;
    int     file_count;

    int     stdin_ispipe;
    int     stdout_ispipe;

    char*   cur_fname;
    FILE*   cur_file;
    char*   cur_line;
    int     cur_line_len;
    int     cur_line_count;
    int     cur_line_count_sel;
    int     cur_sel;

    char   buf_fname[LED_FNAME_MAX];
    char   buf_line[LED_LINE_MAX];
    char   buf_message[LED_MSG_MAX];

} led;

int led_assert(int cond, int code, const char* message, ...) {
    if (!cond) {
        va_list args;

        va_start(args, message);
        vsnprintf(led.buf_message, sizeof(led.buf_message), message, args);
        va_end(args);

        fprintf(stderr, "#LED %s\n", led.buf_message);
        if ( led.cur_file ) {
            fclose(led.cur_file);
            led.cur_file = NULL;
            led.cur_fname = NULL;
        }
        for (int i = 0; i < 2; i++ ) {
            if ( led.sel[i].regex != NULL) {
                pcre2_code_free(led.sel[i].regex);
                led.sel[i].regex = NULL;
            }
        }
        exit(code);
    }
    return cond;
}

void led_verbose(const char* message, ...) {
    if (led.verbose) {
        va_list args;

        va_start(args, message);
        vsnprintf(led.buf_message, LED_MSG_MAX, message, args);
        va_end(args);

        fprintf(stderr, "#LED %s\n", led.buf_message);
    }
}


int led_trim(char* line) {
    int len = strlen(line);
    int last = len - 1;
    while (last >= 0 && (led.cur_fname[last] == '\n' || led.cur_fname[last] == ' ' || led.cur_fname[last] == '\t') ) {
        last--;
    }
    len = last +1;
    line[len] = '\0';
    return len;
}

pcre2_code* led_regex_compile(const char* pattern) {
    int pcre_err;
    PCRE2_SIZE pcre_erroff;
    PCRE2_UCHAR pcre_errbuf[256];
    pcre2_code* regex = pcre2_compile((PCRE2_SPTR)pattern, PCRE2_ZERO_TERMINATED, 0, &pcre_err, &pcre_erroff, NULL);
    pcre2_get_error_message(pcre_err, pcre_errbuf, sizeof(pcre_errbuf));
    led_assert(regex != NULL, LED_ERR_ARG, "Regex error \"%s\" offset %d: %s", pattern, pcre_erroff, pcre_errbuf);
    return regex;
}

int led_regex_match(pcre2_code* regex, const char* line, int len) {
    pcre2_match_data* match_data = pcre2_match_data_create_from_pattern(regex, NULL);
    int rc = pcre2_match(regex, (PCRE2_SPTR)line, len, 0, 0, match_data, NULL);
    pcre2_match_data_free(match_data);
    return rc >= 0;
}

void led_init(int argc, char* argv[]) {
    led_verbose("Init");

    led.verbose = FALSE;
    led.cli_st = CLIST_SELECT;
    led.sel[0].type = SEL_TYPE_NONE;
    led.sel[0].regex = NULL;
    led.sel[0].count = 0;
    led.sel[1].type = SEL_TYPE_NONE;
    led.sel[1].regex = NULL;
    led.sel[1].count = 0;
    led.sel_func = SEL_FUNC_LINE;
    led.func = FUNC_PRINT;
    led.func_arg[0] = NULL;
    led.func_arg[1] = NULL;
    led.func_arg[2] = NULL;
    led.file_mode = FALSE;
    led.file_names = NULL;
    led.file_count = 0;
    led.stdin_ispipe = !isatty(fileno(stdin));
    led.stdout_ispipe = !isatty(fileno(stdout));
    led.cur_fname = NULL;
    led.cur_file = NULL;
    led.cur_line = NULL;
    led.cur_line_len = 0;
    led.cur_line_count = 0;
    led.cur_line_count_sel = 0;
    led.cur_sel = FALSE;

    for (int argi=1; argi < argc; argi++) {

        if (led.cli_st == CLIST_FILES ) {
            led.file_names = argv + argi;
            led.file_count = argc - argi;
            break;
        }
        else if (led.cli_st < CLIST_FILES && strcmp(argv[argi], "-v") == 0 ) {
            led.verbose = TRUE;
        }
        else if (led.cli_st < CLIST_FILES && strcmp(argv[argi], "-f") == 0 ) {
            led.cli_st = CLIST_FILES;
            led.file_mode = TRUE;
        }
        else if (led.cli_st < CLIST_FUNCT && strcmp(argv[argi], "print") == 0 ) {
            led.cli_st = CLIST_FUNCT;
            led.func = FUNC_PRINT;
        }
        else if (led.cli_st == CLIST_FUNCT && led.func_arg[0] == NULL ) {
            led.func_arg[0] = argv[argi];
        }
        else if (led.cli_st == CLIST_FUNCT && led.func_arg[1] == NULL ) {
            led.func_arg[1] = argv[argi];
        }
        else if (led.cli_st == CLIST_FUNCT && led.func_arg[2] == NULL ) {
            led.func_arg[2] = argv[argi];
        }
        else if (led.cli_st < CLIST_SELFORMAT && strcmp(argv[argi], "line") == 0 ) {
            led.cli_st = CLIST_SELFORMAT;
            led.func = SEL_FUNC_LINE;
        }
        else if (led.cli_st < CLIST_SELFORMAT && strcmp(argv[argi], "block") == 0 ) {
            led.cli_st = CLIST_SELFORMAT;
            led.func = SEL_FUNC_BLOCK;
        }
        else if (led.cli_st == CLIST_SELECT && ! (led.sel[0].type && led.sel[1].type) ) {
            char* str;
            int i = led.sel[0].type != SEL_TYPE_NONE ? 1 : 0;
            led.sel[i].type = SEL_TYPE_COUNT;
            led.sel[i].count = strtol(argv[argi], &str, 10);
            if (led.sel[i].count == 0 && str == argv[argi]) {
                led.sel[i].type = SEL_TYPE_REGEX;
                led.sel[i].regex = led_regex_compile(argv[argi]);
            }
        }
        else {
            led_assert(FALSE, LED_ERR_ARG, "Unknown argument: %s", argv[argi]);
        }
    }

    led_verbose("Status: %d", led.cli_st);
}
int led_next_file() {
    led_verbose("Next file");

    if ( led.file_mode ) {
        if ( led.cur_file ) {
            fclose(led.cur_file);
            led.cur_file = NULL;
            led.cur_fname = NULL;
        }
        if ( led.file_count ) {
            led.cur_fname = led.file_names[0];
            led.file_names++;
            led.file_count--;
            led.cur_file = fopen(led.cur_fname, "r");
            led_assert(led.cur_file != NULL, LED_ERR_FILE, "File not found: %s", led.cur_fname);
        }
        else if (led.stdin_ispipe && !feof(stdin)) {
            led.cur_fname = fgets(led.buf_fname, LED_FNAME_MAX, stdin);
            led_assert(led.cur_fname != NULL, LED_ERR_FILE, "STDIN not readable: %s", led.cur_fname);
            led_trim(led.cur_fname);
            led.cur_file = fopen(led.cur_fname, "r");
            led_assert(led.cur_file != NULL, LED_ERR_FILE, "File not found: %s", led.cur_fname);
        }
    }
    else {
        if ( led.cur_file ) {
            led.cur_file = NULL;
            led.cur_fname = NULL;
        }
        else if (led.stdin_ispipe){
            led.cur_file = stdin;
            led.cur_fname = "STDIN";
        }
    }
    led.cur_line_count = 0;
    led.cur_line_count_sel = 0;
    return led.cur_file != NULL;
}

int led_read_line() {
    led_verbose("Read line");

    if (feof(led.cur_file)) return 0;
    led.cur_line = fgets(led.buf_line, LED_LINE_MAX, led.cur_file);
    if (led.cur_line == NULL) return 0;
    led.cur_line_len = strlen(led.cur_line);
    led.cur_line_count++;
    return led.cur_line_len;
}

void led_write_line() {
    led_verbose("Write line");
    fwrite(led.cur_line, sizeof(char), led.cur_line_len, stdout);
}

int led_select() {
    // no begin selector => no filter
    // no end selector => filter only matching line
    if (led.sel[0].type == SEL_TYPE_NONE )
        led.cur_sel = TRUE;
    else if (led.sel[0].type != SEL_TYPE_NONE && led.sel[1].type == SEL_TYPE_NONE && led.cur_sel )
        led.cur_sel = FALSE;

    if ( (led.sel[0].type == SEL_TYPE_COUNT && led.cur_line_count == led.sel[0].count)
         || (led.sel[0].type == SEL_TYPE_REGEX && led_regex_match(led.sel[0].regex, led.cur_line, led.cur_line_len)) ) {
        led.cur_sel = TRUE;
        led.cur_line_count_sel = 0;
    }
    else if ( (led.sel[1].type == SEL_TYPE_COUNT && led.cur_line_count_sel >= led.sel[1].count)
              || (led.sel[1].type == SEL_TYPE_REGEX && led_regex_match(led.sel[1].regex, led.cur_line, led.cur_line_len)) ) {
        led.cur_sel = FALSE;
    }

    if (led.cur_sel) led.cur_line_count_sel++;

    led_verbose("Select: beg %d -  end %d - sel %d - count %d", led.sel[0].type, led.sel[1].type, led.cur_sel, led.cur_line_count);
    return led.cur_sel;
}

void led_funct_print () {
}

void led_process() {
    led_verbose("Process");
    switch (led.func) {
        case FUNC_PRINT: led_funct_print();
    }
}

int main(int argc, char* argv[]) {
    led_init(argc, argv);

    while (led_next_file()) {
        while (led_read_line()) {
            if (led_select()) {
                led_process();
                led_write_line();
            }
        }
    }
    led_assert(FALSE, LED_SUCCESS, "Done");
}
