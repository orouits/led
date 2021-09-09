#define PCRE2_CODE_UNIT_WIDTH 8

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <pcre2.h>
#include <ctype.h>

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

//-----------------------------------------------
// LED runtime data struct
//-----------------------------------------------

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
        long     val;
    } sel[LED_SEL_MAX];

    // processor function
    int     func;
    struct {
        const unsigned char* str;
        int len;
        long val;
    } func_arg[LED_FARG_MAX];
    pcre2_code* func_regex;


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
        const unsigned char* str;
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

} led;

//-----------------------------------------------
// LED tech functions
//-----------------------------------------------

void led_free() {
    if ( led.curfile.file ) {
        fclose(led.curfile.file);
        led.curfile.file = NULL;
        led.curfile.name = NULL;
    }
    for (int i = 0; i < 2; i++ ) {
        if ( led.sel[i].regex != NULL) {
            pcre2_code_free(led.sel[i].regex);
            led.sel[i].regex = NULL;
        }
    }
    if (led.func_regex != NULL) {
        pcre2_code_free(led.func_regex);
        led.func_regex = NULL;
    }
}
int led_assert(int cond, int code, const char* message, ...) {
    if (!cond) {
        if (message) {
            va_list args;
            va_start(args, message);
            vsnprintf(led.buf_message, sizeof(led.buf_message), message, args);
            va_end(args);
            fprintf(stderr, "#LED %s\n", led.buf_message);
        }
        led_free();
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

//-----------------------------------------------
// LED str functions
//-----------------------------------------------

int led_str_trim(char* line) {
    int len = strlen(line);
    int last = len - 1;
    while (last >= 0 && isspace(led.curfile.name[last])) {
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
    return strstr(str1, str2) == str1;
}

pcre2_code* led_regex_compile(const char* pattern) {
    int pcre_err;
    PCRE2_SIZE pcre_erroff;
    PCRE2_UCHAR pcre_errbuf[256];
    led_assert(pattern != NULL, LED_ERR_ARG, "Missing regex");
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

//-----------------------------------------------
// LED init functions
//-----------------------------------------------

int led_init_opt(const char* arg) {
    int rc = led_str_startwith(arg, "-");
    if ( rc ) {
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
    return rc;
}

int led_init_func(const char* arg) {
    int rc = FALSE;
    char* labels[] = {
        "sb", "substitute",
        "ex", "execute",
        "rm", "remove",
        "rn", "range",
        "tr", "translate",
        "cs", "case",
        "qt", "quote",
        "tm", "trim",
        "sp", "split",
        "rv", "revert",
        "fl", "field",
        "jn", "join",
        "cr", "crypt",
        "uc", "urlencode",
        "ph", "path"
    };
    int labels_sz = sizeof(labels)/sizeof(char*);
    for (int i = 0; i < labels_sz; i+=2) {
        if ( led_str_equal(arg, labels[i]) || led_str_equal(arg, labels[i + 1]) ) {
            // match with define constant
            led.func = i/2 + 1;
            rc = TRUE;
            break;
        }
    }
    return rc;
}

int led_init_func_arg(const char* arg) {
    int rc = !led.func_arg[LED_FARG_MAX - 1].str;
    if (rc) {
        for (int i = 0; i < LED_FARG_MAX; i++) {
            if ( !led.func_arg[i].str ) {
                char* str;
                led.func_arg[i].str = arg;
                led.func_arg[i].len = strlen(arg);
                led.func_arg[i].val = strtol(arg, &str, 10);
                break;
            }
        }
    }
    return rc;
}

int led_init_sel(const char* arg) {
    int rc = !led.sel[LED_SEL_MAX-1].type;
    if ( rc ) {
        for (int i = 0; i < LED_SEL_MAX; i++) {
            if ( !led.sel[i].type ) {
                char* str;
                led.sel[i].type = SEL_TYPE_COUNT;
                led.sel[i].val = strtol(arg, &str, 10);
                if (led.sel[i].val == 0 && str == arg) {
                    led.sel[i].type = SEL_TYPE_REGEX;
                    led.sel[i].regex = led_regex_compile(arg);
                }
                led_verbose("Selector: %d, %d", i, led.sel[i].type);
                break;
            }
        }
    }
    return rc;
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
        else if (led.argsection < ARGS_FILES && led_init_opt(arg) ) ;
        else if (led.argsection == ARGS_FUNCT && led_init_func_arg(arg) ) ;
        else if (led.argsection < ARGS_FUNCT && led_init_func(arg) ) led.argsection = ARGS_FUNCT;
        else if (led.argsection == ARGS_SELECT && led_init_sel(arg) ) ;
        else led_assert(FALSE, LED_ERR_ARG, "Unknown argument: %s", arg);
    }

    led_verbose("Func: %d", led.func);
}

//-----------------------------------------------
// LED process functions
//-----------------------------------------------

int led_next_file() {
    led_verbose("Next file");

    if ( led.o_file_in ) {
        if ( led.curfile.file ) {
            fclose(led.curfile.file);
            led.curfile.file = NULL;
            led.curfile.name = NULL;
        }
        if ( led.file_count ) {
            led.curfile.name = led.file_names[0];
            led.file_names++;
            led.file_count--;
            led.curfile.file = fopen(led.curfile.name, "r");
            led_assert(led.curfile.file != NULL, LED_ERR_FILE, "File not found: %s", led.curfile.name);
        }
        else if (led.stdin_ispipe && !feof(stdin)) {
            led.curfile.name = fgets(led.buf_fname, LED_FNAME_MAX, stdin);
            led_assert(led.curfile.name != NULL, LED_ERR_FILE, "STDIN not readable: %s", led.curfile.name);
            led_str_trim(led.curfile.name);
            led.curfile.file = fopen(led.curfile.name, "r");
            led_assert(led.curfile.file != NULL, LED_ERR_FILE, "File not found: %s", led.curfile.name);
        }
    }
    else {
        if ( led.curfile.file ) {
            led.curfile.file = NULL;
            led.curfile.name = NULL;
        }
        else if (led.stdin_ispipe){
            led.curfile.file = stdin;
            led.curfile.name = "STDIN";
        }
    }
    led.curline.count = 0;
    led.curline.count_sel = 0;
    return led.curfile.file != NULL;
}

int led_read_line() {
    led_verbose("Read line");

    if (feof(led.curfile.file)) return 0;
    led.curline.str = fgets(led.buf_line, LED_LINE_MAX, led.curfile.file);
    if (led.curline.str == NULL) return 0;
    led.curline.len = strlen(led.curline.str);
    led.curline.count++;
    return led.curline.len;
}

void led_write_line() {
    led_verbose("Write line");
    fwrite(led.curline.str, sizeof(char), led.curline.len, stdout);
}

int led_select() {
    // no begin selector => no filter
    // no end selector => filter only matching line
    if (led.sel[0].type == SEL_TYPE_NONE )
        led.curline.selected = TRUE;
    else if (led.sel[0].type != SEL_TYPE_NONE && led.sel[1].type == SEL_TYPE_NONE && led.curline.selected )
        led.curline.selected = FALSE;

    if ( (led.sel[0].type == SEL_TYPE_COUNT && led.curline.count == led.sel[0].val)
         || (led.sel[0].type == SEL_TYPE_REGEX && led_regex_match(led.sel[0].regex, led.curline.str, led.curline.len)) ) {
        led.curline.selected = TRUE;
        led.curline.count_sel = 0;
    }
    else if ( (led.sel[1].type == SEL_TYPE_COUNT && led.curline.count_sel >= led.sel[1].val)
              || (led.sel[1].type == SEL_TYPE_REGEX && led_regex_match(led.sel[1].regex, led.curline.str, led.curline.len)) ) {
        led.curline.selected = FALSE;
    }

    if (led.curline.selected) led.curline.count_sel++;

    led_verbose("Select: beg %d -  end %d - sel %d - count %d", led.sel[0].type, led.sel[1].type, led.curline.selected, led.curline.count);
    return led.curline.selected;
}

void led_funct_substitute () {
    if (led.func_regex == NULL) {
        led.func_regex = led_regex_compile(led.func_arg[0].str);
        led_assert(led.func_arg[1].str != NULL, LED_ERR_ARG, "substitute: missing replace argument");
    }

    PCRE2_SIZE len = led.curline.len;
    int rc = pcre2_substitute(
                led.func_regex,
                led.curline.str,
                led.curline.len,
                0,
                PCRE2_SUBSTITUTE_EXTENDED,
                NULL,
                NULL,
                led.func_arg[1].str,
                led.func_arg[1].len,
                led.buf_line_trans,
                &len);
    led.curline.str = led.buf_line_trans;
    led.curline.len = len;
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

//-----------------------------------------------
// LED main
//-----------------------------------------------

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
