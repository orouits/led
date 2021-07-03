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
        char*   regex;
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
        va_list ap;

        va_start(ap, message);
        vsnprintf(led.buf_message, LED_MSG_MAX, message, ap);
        va_end(ap);

        fprintf(stderr, "#LED %s\n", led.buf_message);
        if ( led.cur_file ) {
            fclose(led.cur_file);
            led.cur_file = NULL;
            led.cur_fname = NULL;
        }
        exit(code);
    }
    return cond;
}

void led_verbose(const char* message, ...) {
    if (led.verbose) {
        va_list ap;

        va_start(ap, message);
        vsnprintf(led.buf_message, LED_MSG_MAX, message, ap);
        va_end(ap);

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

    for (int i=1; i< argc; i++) {

        if (led.cli_st == CLIST_FILES ) {
            led.file_names = argv + i;
            led.file_count = argc - i;
            break;
        }
        else if (led.cli_st < CLIST_FILES && strcmp(argv[i], "-v") == 0 ) {
            led.verbose = TRUE;
        }
        else if (led.cli_st < CLIST_FILES && strcmp(argv[i], "-f") == 0 ) {
            led.cli_st = CLIST_FILES;
            led.file_mode = TRUE;
        }
        else if (led.cli_st < CLIST_FUNCT && strcmp(argv[i], "print") == 0 ) {
            led.cli_st = CLIST_FUNCT;
            led.func = FUNC_PRINT;
        }
        else if (led.cli_st == CLIST_FUNCT && led.func_arg[0] == NULL ) {
            led.func_arg[0] = argv[i];
        }
        else if (led.cli_st == CLIST_FUNCT && led.func_arg[1] == NULL ) {
            led.func_arg[1] = argv[i];
        }
        else if (led.cli_st == CLIST_FUNCT && led.func_arg[2] == NULL ) {
            led.func_arg[2] = argv[i];
        }
        else if (led.cli_st < CLIST_SELFORMAT && strcmp(argv[i], "line") == 0 ) {
            led.cli_st = CLIST_SELFORMAT;
            led.func = SEL_FUNC_LINE;
        }
        else if (led.cli_st < CLIST_SELFORMAT && strcmp(argv[i], "block") == 0 ) {
            led.cli_st = CLIST_SELFORMAT;
            led.func = SEL_FUNC_BLOCK;
        }
        else if (led.cli_st == CLIST_SELECT && !led.sel[0].type ) {
            char* str;
            led.sel[0].type = SEL_TYPE_COUNT;
            led.sel[0].count = strtol(argv[i], &str, 10);
            if (led.sel[0].count == 0 && str == argv[i]) {
                led.sel[0].type = SEL_TYPE_REGEX;
                led.sel[0].regex = argv[i];
            }
        }
        else if (led.cli_st == CLIST_SELECT && !led.sel[1].type) {
            char* str;
            led.sel[1].type = SEL_TYPE_COUNT;
            led.sel[1].count = strtol(argv[i], &str, 10);
            if (led.sel[1].count == 0 && str == argv[i]) {
                led.sel[1].type = SEL_TYPE_REGEX;
                led.sel[1].regex = argv[i];
            }
        }
        else {
            led_assert(FALSE, LED_ERR_ARG, "Unknown argument: %s", argv[i]);
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
    // if begin selector only, select only matching begin filter
    if (led.sel[0].type == SEL_TYPE_NONE )
        led.cur_sel = TRUE;
    else if (led.sel[0].type != SEL_TYPE_NONE && led.sel[1].type == SEL_TYPE_NONE && led.cur_sel )
        led.cur_sel = FALSE;
    else if (led.sel[0].type == SEL_TYPE_COUNT && led.cur_line_count == led.sel[0].count)
        led.cur_sel = TRUE;
    else if (led.sel[1].type == SEL_TYPE_COUNT && led.cur_line_count > led.sel[1].count)
        led.cur_sel = FALSE;

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
    led_verbose("Done");
}
