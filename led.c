#define PCRE2_CODE_UNIT_WIDTH 8

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <pcre2.h>

#define FALSE 0
#define TRUE 1

#define ARGS_SELECT 0
#define ARGS_FUNCT 1
#define ARGS_FILES 2

#define SEL_TYPE_NONE 0
#define SEL_TYPE_REGEX 1
#define SEL_TYPE_COUNT 2
#define SEL_COUNT 2

#define SEL_FUNC_LINE 0
#define SEL_FUNC_BLOCK 1

#define FUNC_NONE 0
#define FUNC_SUBSTITUTE 1
#define FUNC_EXECUTE 2
#define FUNC_REMOVE 3
#define FUNC_RANGE 4
#define FUNC_TRANSLATE 5
#define FUNC_CASE 6
#define FUNC_QUOTE 7
#define FUNC_TRIM 8
#define FUNC_SPLIT 9
#define FUNC_REVERT 10
#define FUNC_FIELD 11
#define FUNC_JOIN 12
#define FUNC_CRYPT 13
#define FUNC_URLENCODE 14
#define FUNC_PATH 15

#define LED_SUCCESS 0
#define LED_ERR_ARG 1
#define LED_ERR_FILE 2

#define LED_SEL_MAX 2
#define LED_FARG_MAX 3
#define LED_LINE_MAX 4096
#define LED_FNAME_MAX 4096
#define LED_MSG_MAX 4096

#define LED_EXIT_STD 0
#define LED_EXIT_VAL 1

#define LED_FILE_OUT_NONE 0
#define LED_FILE_OUT_INPLACE 1
#define LED_FILE_OUT_WRITE 2
#define LED_FILE_OUT_APPEND 3
#define LED_FILE_OUT_NEWEXT 4



struct {
    // section while cli is decoded
    int     argsection;

    // options
    int     o_verbose;
    int     o_summary;
    int     o_quiet;
    int     o_zero;
    int     o_exit_mode;
    int     o_sel_not;
    int     o_sel_block;
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
        long     count;
    } sel[LED_SEL_MAX];

    // processor function
    int     func;
    const char*   func_arg[LED_FARG_MAX];

    // files
    char**  file_names;
    int     file_count;


    int     stdin_ispipe;
    int     stdout_ispipe;

    // runtime variables
    char*   cur_fname;
    FILE*   cur_file;
    char*   cur_line;
    int     cur_line_len;
    int     cur_line_count;
    int     cur_line_count_sel;
    int     cur_sel;

    // runtime buffers
    char   buf_fname[LED_FNAME_MAX];
    char   buf_line[LED_LINE_MAX];
    char   buf_message[LED_MSG_MAX];

} led;

int led_assert(int cond, int code, const char* message, ...) {
    if (!cond) {
        if (message) {
            va_list args;

            va_start(args, message);
            vsnprintf(led.buf_message, sizeof(led.buf_message), message, args);
            va_end(args);

            fprintf(stderr, "#LED %s\n", led.buf_message);
        }
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
    if (led.o_verbose) {
        va_list args;

        va_start(args, message);
        vsnprintf(led.buf_message, LED_MSG_MAX, message, args);
        va_end(args);

        fprintf(stderr, "#LED %s\n", led.buf_message);
    }
}


int led_str_trim(char* line) {
    int len = strlen(line);
    int last = len - 1;
    while (last >= 0 && (led.cur_fname[last] == '\n' || led.cur_fname[last] == ' ' || led.cur_fname[last] == '\t') ) {
        last--;
    }
    len = last +1;
    line[len] = '\0';
    return len;
}

int led_str_equal(const char* str1, const char* str2) {
    return strcmp(str1, str2) == 0;
}

int led_str_startwith(const char* str1, const char* str2) {
    return strncmp(str1, str2, strlen(str2)) == 0;
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

    memset(&led, 0, sizeof(led));
    led.stdin_ispipe = !isatty(fileno(stdin));
    led.stdout_ispipe = !isatty(fileno(stdout));

    for (int argi=1; argi < argc; argi++) {
        const char* arg = argv[argi];

        if (led.argsection == ARGS_FILES ) {
            led.file_names = argv + argi;
            led.file_count = argc - argi;
            break;
        }
        else if (led.argsection < ARGS_FILES && led_str_startwith(arg, "-") ) {
            int opti = 1;
            int optl = strlen(arg);
            while (opti < optl) {
                led_verbose("options: %c", arg[opti]);
                switch (arg[opti]) {
                case 'z':
                    led.o_zero = TRUE;
                    break;
                case 'v':
                    led.o_verbose = TRUE;
                    break;
                case 'q':
                    led.o_quiet = TRUE;
                    break;
                case 's':
                    led.o_summary = TRUE;
                    break;
                case 'e':
                    led.o_exit_mode = LED_EXIT_VAL;
                    break;
                case 'n':
                    led.o_sel_not = TRUE;
                    break;
                case 'b':
                    led.o_sel_block = TRUE;
                    break;
                case 'f':
                    led.o_file_in = TRUE;
                    led.argsection = ARGS_FILES;
                    break;
                case 'F':
                    led.o_file_out = TRUE;
                    break;
                case 'I':
                    led_assert(!led.o_file_out_mode, LED_ERR_ARG, "Bad option %c, output file mode already set", arg[opti]);
                    led.o_file_out_mode = LED_FILE_OUT_INPLACE;
                    break;
                case 'W':
                    led_assert(!led.o_file_out_mode, LED_ERR_ARG, "Bad option %c, output file mode already set", arg[opti]);
                    led.o_file_out_mode = LED_FILE_OUT_WRITE;
                    led.o_file_out_path = arg + opti + 1;
                    opti = optl;
                    break;
                case 'A':
                    led_assert(!led.o_file_out_mode, LED_ERR_ARG, "Bad option %c, output file mode already set", arg[opti]);
                    led.o_file_out_mode = LED_FILE_OUT_APPEND;
                    led.o_file_out_path = arg + opti + 1;
                    opti = optl;
                    break;
                case 'E':
                    led_assert(!led.o_file_out_mode, LED_ERR_ARG, "Bad option %c, output file mode already set", arg[opti]);
                    led.o_file_out_mode = LED_FILE_OUT_NEWEXT;
                    led.o_file_out_extn = atoi(arg + opti + 1);
                    if ( led.o_file_out_extn <= 0 )
                        led.o_file_out_ext = arg + opti + 1;
                    opti = optl;
                    break;
                case 'D':
                    led.o_file_out_dir = arg + opti + 1;
                    opti = optl;
                    break;
                case 'U':
                    led.o_file_out_unchanged = TRUE;
                    break;
                default:
                    led_assert(FALSE, LED_ERR_ARG, "Unknown option: %c", arg[opti]);
                }
                opti++;
            }
        }
        else if (led.argsection == ARGS_FUNCT && !led.func_arg[LED_FARG_MAX - 1]) {
            for (int i = 0; i < LED_FARG_MAX; i++) {
                if ( !led.func_arg[i] ) {
                    led.func_arg[i] = arg;
                    break;
                }
            }
        }
        else if (led.argsection < ARGS_FUNCT && (led_str_equal(arg, "sb") || led_str_equal(arg, "substitute" ))) {
            led.func = FUNC_SUBSTITUTE;
            led.argsection = ARGS_FUNCT;
        }
        else if (led.argsection < ARGS_FUNCT && (led_str_equal(arg, "ex") || led_str_equal(arg, "execute" ))) {
            led.func = FUNC_EXECUTE;
            led.argsection = ARGS_FUNCT;
        }
        else if (led.argsection < ARGS_FUNCT && (led_str_equal(arg, "rm") || led_str_equal(arg, "remove" ))) {
            led.func = FUNC_REMOVE;
            led.argsection = ARGS_FUNCT;
        }
        else if (led.argsection < ARGS_FUNCT && (led_str_equal(arg, "rn") || led_str_equal(arg, "range" ))) {
            led.func = FUNC_RANGE;
            led.argsection = ARGS_FUNCT;
        }
        else if (led.argsection < ARGS_FUNCT && (led_str_equal(arg, "tr") || led_str_equal(arg, "translate" ))) {
            led.func = FUNC_TRANSLATE;
            led.argsection = ARGS_FUNCT;
        }
        else if (led.argsection < ARGS_FUNCT && (led_str_equal(arg, "cs") || led_str_equal(arg, "case" ))) {
            led.func = FUNC_CASE;
            led.argsection = ARGS_FUNCT;
        }
        else if (led.argsection < ARGS_FUNCT && (led_str_equal(arg, "qt") || led_str_equal(arg, "quote" ))) {
            led.func = FUNC_QUOTE;
            led.argsection = ARGS_FUNCT;
        }
        else if (led.argsection < ARGS_FUNCT && (led_str_equal(arg, "tm") || led_str_equal(arg, "trim" ))) {
            led.func = FUNC_TRIM;
            led.argsection = ARGS_FUNCT;
        }
        else if (led.argsection < ARGS_FUNCT && (led_str_equal(arg, "sp") || led_str_equal(arg, "split" ))) {
            led.func = FUNC_SPLIT;
            led.argsection = ARGS_FUNCT;
        }
        else if (led.argsection < ARGS_FUNCT && (led_str_equal(arg, "rv") || led_str_equal(arg, "revert" ))) {
            led.func = FUNC_REVERT;
            led.argsection = ARGS_FUNCT;
        }
        else if (led.argsection < ARGS_FUNCT && (led_str_equal(arg, "fl") || led_str_equal(arg, "field" ))) {
            led.func = FUNC_FIELD;
            led.argsection = ARGS_FUNCT;
        }
        else if (led.argsection < ARGS_FUNCT && (led_str_equal(arg, "jn") || led_str_equal(arg, "join" ))) {
            led.func = FUNC_JOIN;
            led.argsection = ARGS_FUNCT;
        }
        else if (led.argsection < ARGS_FUNCT && (led_str_equal(arg, "cr") || led_str_equal(arg, "crypt" ))) {
            led.func = FUNC_CRYPT;
            led.argsection = ARGS_FUNCT;
        }
        else if (led.argsection < ARGS_FUNCT && (led_str_equal(arg, "uc") || led_str_equal(arg, "urlencode" ))) {
            led.func = FUNC_URLENCODE;
            led.argsection = ARGS_FUNCT;
        }
        else if (led.argsection < ARGS_FUNCT && (led_str_equal(arg, "ph") || led_str_equal(arg, "path" ))) {
            led.func = FUNC_PATH;
            led.argsection = ARGS_FUNCT;
        }
        else if (led.argsection == ARGS_SELECT && !led.sel[LED_SEL_MAX-1].type ) {
            for (int i = 0; i < LED_SEL_MAX; i++) {
                if ( !led.sel[i].type ) {
                    char* str;
                    led.sel[i].type = SEL_TYPE_COUNT;
                    led.sel[i].count = strtol(arg, &str, 10);
                    if (led.sel[i].count == 0 && str == arg) {
                        led.sel[i].type = SEL_TYPE_REGEX;
                        led.sel[i].regex = led_regex_compile(arg);
                    }
                    led_verbose("Selector: %d, %d", i, led.sel[i].type);
                    break;
                }
            }
        }
        else {
            led_assert(FALSE, LED_ERR_ARG, "Unknown argument: %s", arg);
        }
    }

    led_verbose("Func: %d", led.func);
}
int led_next_file() {
    led_verbose("Next file");

    if ( led.o_file_in ) {
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
            led_str_trim(led.cur_fname);
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

void led_funct_substitute () {
}

void led_funct_execute () {
}

void led_process() {
    led_verbose("Process");
    switch (led.func) {
    case FUNC_SUBSTITUTE:
        led_funct_substitute();
        break;
    case FUNC_EXECUTE:
        led_funct_execute();
        break;
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
    led_assert(FALSE, LED_SUCCESS, NULL);
}
