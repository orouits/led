#include "led.h"

#define ARGS_SEC_SELECT 0
#define ARGS_SEC_FUNCT 1
#define ARGS_SEC_FILES 2

const char* LED_SEC_TABLE[] = {
    "selector",
    "function",
    "files",
};

#define SEL_TYPE_NONE 0
#define SEL_TYPE_REGEX 1
#define SEL_TYPE_COUNT 2
#define SEL_COUNT 2

#define LED_EXIT_STD 0
#define LED_EXIT_VAL 1

#define LED_FILE_OUT_NONE 0
#define LED_FILE_OUT_INPLACE 1
#define LED_FILE_OUT_WRITE 2
#define LED_FILE_OUT_APPEND 3
#define LED_FILE_OUT_NEWEXT 4

typedef void (*led_fn_impl)();

typedef struct {
    const char* short_name;
    const char* long_name;
    led_fn_impl impl;
    const char* args_fmt;
    const char* help_desc;
    const char* help_format;
} led_fn_struct;

led_fn_struct LED_FN_TABLE[] = {
    { "nn:", "none:", &led_fn_impl_none, "", "No processing", "none:" },
    { "sub:", "substitute:", &led_fn_impl_substitute, "RS", "Substitute", "substitute: <regex> <replace>" },
    { "exe:", "execute:", NULL, "RS", "Execute", "execute: <regex> <replace=command>" },
    { "rm:", "remove:", &led_fn_impl_remove, "s", "Remove line", "remove: [<regex>]" },
    { "rmb:", "remove_blank:", NULL, "", "Remove blank/empty lines", "remove_blank:" },
    { "ins:", "insert:", &led_fn_impl_insert, "Sn", "Insert line", "insert: <string> [N]" },
    { "app:", "append:", &led_fn_impl_append, "Sn", "Append line", "append: <string> [N]" },
    { "rns:", "range_sel:", &led_fn_impl_rangesel, "Nn", "Range select", "range_sel: <start> [count]" },
    { "rnu:", "range_unsel:", NULL, "Nn", "Range unselect", "range_unsel: <start> [count]" },
    { "tr:", "translate:", &led_fn_impl_translate, "SS", "Translate", "translate: <chars> <chars>" },
    { "csl:", "case_lower:", &led_fn_impl_caselower, "r", "Case to lower", "case_lower: [<regex>]" },
    { "csu:", "case_upper:", &led_fn_impl_caseupper, "r", "Case to upper", "case_upper: [<regex>]" },
    { "csf:", "case_first:", &led_fn_impl_casefirst, "r", "Case first upper", "case_first: [<regex>]" },
    { "csc:", "case_camel:", &led_fn_impl_casecamel, "r", "Case to camel style", "case_camel: [<regex>]" },
    { "qts:", "quote_simple:", &led_fn_impl_quotesimple, "r", "Quote simple", "quote_simple: [<regex>]" },
    { "qtd:", "quote_double:", &led_fn_impl_quotedouble, "r", "Quote double", "quote_double: [<regex>]" },
    { "qtb:", "quote_back:", &led_fn_impl_quoteback, "r", "Quote back", "quote_back: [<regex>]" },
    { "qtr:", "quote_remove:", NULL, "r", "Quote remove", "quote_remove: [<regex>]" },
    { "tm:", "trim:", NULL, "r", "Trim", "trim: [<regex>]" },
    { "tml:", "trim_left:", NULL, "r", "Trim left", "trim_left: [<regex>]" },
    { "tmr:", "trim_right:", NULL, "r", "Trim right", "trim_right: [<regex>]" },
    { "sp:", "split:", NULL, "S", "Split", "split: <string>" },
    { "rv:", "revert:", NULL, "r", "Revert", "revert: [<regex>]" },
    { "fl:", "field:", NULL, "SN", "Extract fields", "field: <sep> <N>" },
    { "jn:", "join:", NULL, "", "Join lines", "join:" },
    { "ecrb64:", "encrypt_base64:", NULL, "s", "Encrypt base64", "encrypt_base64: [<regex>]" },
    { "dcrb64:", "decrypt_base64:", NULL, "s", "Decrypt base64", "decrypt_base64: [<regex>]" },
    { "urc:", "url_encode:", NULL, "s", "Encode URL", "url_encode: [<regex>]" },
    { "urd:", "url_decode:", NULL, "s", "Decode URL", "url_decode: [<regex>]" },
    { "phc:", "path_canonical:", NULL, "s", "Conert to canonical path", "path_canonical: [<regex>]" },
    { "phd:", "path_dir:", NULL, "s", "Extract last dir of the path", "path_dir: [<regex>]" },
    { "phf:", "path_file:", NULL, "s", "Extract file of the path", "path_file: [<regex>]" },
    { "phr:", "path_rename:", NULL, "s", "Rename file of the path without specific chars", "path_rename: [<regex>]" },
    { "rnn:", "randomize_num:", NULL, "s", "Randomize numeric values", "randomize_num: [<regex>]" },
    { "rna:", "randomize_alpha:", NULL, "s", "Randomize alpha values", "randomize_alpha: [<regex>]" },
    { "rnan:", "randomize_alphaum:", NULL, "s", "Randomize alpha numeric values", "randomize_alphaum: [<regex>]" },
};

#define LED_FN_TABLE_MAX sizeof(LED_FN_TABLE)/sizeof(led_fn_struct)

led_struct led;

//-----------------------------------------------
// LED init functions
//-----------------------------------------------

int led_init_opt(const char* arg) {
    int rc = led_str_match(arg, "^-[a-zA-Z]+$");
    if ( rc ) {
        int opti = 1;
        int optl = strlen(arg);
        while (opti < optl) {
            led_debug("options: %c", arg[opti]);
            switch (arg[opti]) {
            case 'h':
                led.o_help = TRUE;
                break;
            case 'z':
                led.o_zero = TRUE;
                break;
            case 'v':
                led.o_verbose = TRUE;
                break;
            case 'q':
                led.o_quiet = TRUE;
                break;
            case 'r':
                led.o_report = TRUE;
                break;
            case 'x':
                led.o_exit_mode = LED_EXIT_VAL;
                break;
            case 'n':
                led.o_sel_invert = TRUE;
                break;
            case 'b':
                led.o_sel_block = TRUE;
                break;
            case 's':
                led.o_output_selected = TRUE;
                break;
            case 'e':
                led.o_filter_empty = TRUE;
                break;
            case 'f':
                led.o_file_in = TRUE;
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
                led_assert(FALSE, LED_ERR_ARG, "Unknown option: -%c", arg[opti]);
            }
            opti++;
        }
    }
    return rc;
}

int led_init_func(const char* arg) {
    int rc = led_str_match(arg, "^[a-z]+:$");
    if (rc) {
        led.fn_id = -1;
        led_debug("Funcion table max: %d", LED_FN_TABLE_MAX);
        for (int i = 0; i < LED_FN_TABLE_MAX; i++) {
            if ( led_str_equal(arg, LED_FN_TABLE[i].short_name) || led_str_equal(arg, LED_FN_TABLE[i].long_name) ) {
                led.fn_id = i;
                break;
            }
        }
        led_assert(led.fn_id >= 0, LED_ERR_ARG, "Unknown function: %s", arg);
        led_assert(LED_FN_TABLE[led.fn_id].impl, LED_ERR_ARG, "Function not yet implemented: %s", LED_FN_TABLE[led.fn_id].long_name);
    }
    return rc;
}

int led_init_func_arg(const char* arg) {
    int rc = !led.fn_arg[LED_FARG_MAX - 1].str;
    if (rc) {
        for (int i = 0; i < LED_FARG_MAX; i++) {
            if ( !led.fn_arg[i].str ) {
                char* str;
                led.fn_arg[i].str = arg;
                led.fn_arg[i].len = strlen(arg);
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
                led_debug("Selector: %d, %d", i, led.sel[i].type);
                break;
            }
        }
    }
    return rc;
}

void led_init_config() {
    const char* format = LED_FN_TABLE[led.fn_id].args_fmt;
    for (int i=0; format[i]; i++) {
        if (format[i] == 'R') {
            led_assert(led.fn_arg[i].str != NULL, LED_ERR_ARG, "function arg %d: missing regex\n%s", i+1, LED_FN_TABLE[led.fn_id].help_format);
            led.fn_arg[i].regex = led_regex_compile(led.fn_arg[i].str);
        }
        else if (format[i] == 'r') {
            if (led.fn_arg[i].str)
                led.fn_arg[i].regex = led_regex_compile(led.fn_arg[i].str);
        }
        else if (format[i] == 'N') {
            led_assert(led.fn_arg[i].str != NULL, LED_ERR_ARG, "function arg %d: missing number\n%s", i+1, LED_FN_TABLE[led.fn_id].help_format);
            led.fn_arg[i].val = atol(led.fn_arg[i].str);
        }
        else if (format[i] == 'n') {
            led.fn_arg[i].val = atol(led.fn_arg[i].str);
        }
        else if (format[i] == 'S') {
            led_assert(led.fn_arg[i].str != NULL, LED_ERR_ARG, "function arg %d: missing string\n%s", i+1, LED_FN_TABLE[led.fn_id].help_format);
        }
        else if (format[i] == 's') {
        }
        else {
            led_assert(TRUE, LED_ERR_ARG, "arg %d: bad internal format (%s)", i, format);
        }
    }
}


void led_init(int argc, char* argv[]) {
    led_debug("Init");
    int arg_section = 0;

    memset(&led, 0, sizeof(led));

    led.stdin_ispipe = !isatty(fileno(stdin));
    led.stdout_ispipe = !isatty(fileno(stdout));

    for (int argi=1; argi < argc; argi++) {
        const char* arg = argv[argi];

        if (arg_section == ARGS_SEC_FILES ) {
            led.file_names = argv + argi;
            led.file_count = argc - argi;
        }
        else if (arg_section < ARGS_SEC_FILES && led_init_opt(arg) ) {
            if (led.o_file_in) arg_section = ARGS_SEC_FILES;
        }
        else if (arg_section == ARGS_SEC_FUNCT && led_init_func_arg(arg) ) {

        }
        else if (arg_section < ARGS_SEC_FUNCT && led_init_func(arg) ) {
            arg_section = ARGS_SEC_FUNCT;
        }
        else if (arg_section == ARGS_SEC_SELECT && led_init_sel(arg) ) {

        }
        else {
            led_assert(FALSE, LED_ERR_ARG, "Unknown or wrong argument: %s (%s section)", arg, LED_SEC_TABLE[arg_section]);
        }
    }

    // if a process function is not defined show only selected
    led.o_output_selected = led.o_output_selected || !led.fn_id;

    led_debug("Function: %s (%d)", LED_FN_TABLE[led.fn_id].long_name, led.fn_id);

    // pre-configure the processor command
    led_init_config();
}

void led_help() {
    fprintf(stderr,
"\
led [<selector>] [<processor>] [-options] [files] ...\n\
\n\
Selector:\n\
    <regex>              => select all lines matching with <regex>\n\
    <n>                  => select line <n>\n\
    <regex> <regex_stop> => select group of lines starting matching <regex> (included) until matching <regex_stop> (excluded)\n\
    <regex> <count>      => select group of lines starting matching <regex> (included) until <count> lines are selected\n\
    <n>     <regex_stop> => select group of lines starting line <n> (included) until matching <regex_stop> (excluded)\n\
    <n>     <count>      => select group of lines starting line <n> (included) until <count> lines are selected\n\
\n\
Processor:\n\
    <function>: [arg] ...\n\
\n\
Global options\n\
    -z  end of line is 0\n\
    -v  verbose to STDERR\n\
    -r  report to STDERR\n\
    -q  quiet, do not ouptut anything (exit code only)\n\
    -e  exit code on value\n\
\n\
Selector Options:\n\
    -n  invert selection\n\
    -b  selected lines as blocks\n\
    -s  output only selected\n\
\n\
File Options:\n\
    -f          read filenames to STDIN instead of content, or from command line if followd by arguments as file names (file section)\n\
    -F          write filenames to STDOUT instead of content.\n\
                This option allows advanced massive files chained transformations with pipes.\n\
    -I          write content to filename inplace\n\
    -W<path>    write content to a fixed file\n\
    -A<path>    append content to a fixed file\n\
    -E<ext>     write content to filename.ext\n\
    -E<3>       write content to filename.NNN\n\
    -D<dir>     write files in dir.\n\
    -U          write unchanged filenames\n\
\n\
Processor commands:\n\
"
    );
    fprintf(stderr, "| %-20s | %-8s | %-50s | %-40s |\n", "Name", "Short", "Description", "Format");
    fprintf(stderr, "| %-20s | %-8s | %-50s | %-40s |\n", "-----", "-----", "-----", "-----");
    for (int i=0; i < LED_FN_TABLE_MAX; i++) {
        if (!LED_FN_TABLE[i].impl) fprintf(stderr, "\e[90m");
        fprintf(stderr, "| %-20s | %-8s | %-50s | %-40s |\n",
            LED_FN_TABLE[i].long_name,
            LED_FN_TABLE[i].short_name,
            LED_FN_TABLE[i].help_desc,
            LED_FN_TABLE[i].help_format
        );
        if (!LED_FN_TABLE[i].impl) fprintf(stderr, "\e[0m");
    }
    fprintf(stderr, "| %-20s | %-8s | %-50s | %-40s |\n", "-----", "-----", "-----", "-----");
}

//-----------------------------------------------
// LED process functions
//-----------------------------------------------

int led_next_file() {
    led_debug("Next file");

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
    led.curline.selected = FALSE;
    return led.curfile.file != NULL;
}

int led_read_line() {

    if (feof(led.curfile.file)) return 0;
    //memset(led.buf_line, 0, sizeof(led.buf_line));

    led.curline.str = fgets(led.buf_line, LED_LINE_MAX, led.curfile.file);
    if (led.curline.str == NULL) return FALSE;
    led.curline.len = strlen(led.curline.str);
    while (led.curline.len > 0 && led.curline.str[led.curline.len - 1] == '\n') {
        // no trailing \n for processing
        led.curline.len--;
        led.buf_line[led.curline.len] = '\0';
    }
    led.curline.count++;
    led_debug("New line: (%d) %s", led.curline.count, led.curline.str);
    return TRUE;
}

void led_write_line() {
    led_debug("Write line: (%d) %d", led.curline.count, led.curline.len);
    int nb = 0;

    // if no filter empty empty strings are output
    if (led.curline.str && (led.curline.len || !led.o_filter_empty) ) {
        led.curline.str[led.curline.len++] = '\n';
        led.curline.str[led.curline.len] = '\0';
        nb = fwrite(led.curline.str, sizeof(char), led.curline.len, stdout);
        fflush(stdout);
    }
}

int led_select() {
    // led.o_sel_invert option is not handled here but by main process.

    // stop selection on second boundary
    if ((led.sel[1].type == SEL_TYPE_NONE && led.sel[0].type != SEL_TYPE_NONE)
        || (led.sel[1].type == SEL_TYPE_COUNT && led.curline.count_sel >= led.sel[1].val)
        || (led.sel[1].type == SEL_TYPE_REGEX && led_regex_match(led.sel[1].regex, led.curline.str, led.curline.len)) ) {
        led.curline.selected = FALSE;
        led.curline.count_sel = 0;
    }

    // start selection on first boundary
    if (led.sel[0].type == SEL_TYPE_NONE
        || (led.sel[0].type == SEL_TYPE_COUNT && led.curline.count == led.sel[0].val)
        || (led.sel[0].type == SEL_TYPE_REGEX && led_regex_match(led.sel[0].regex, led.curline.str, led.curline.len)) ) {
        led.curline.selected = TRUE;
        led.curline.count_sel = 0;
    }

    if (led.curline.selected) led.curline.count_sel++;

    led_debug("Select: %d", led.curline.selected);
    return led.curline.selected;
}

void led_process() {
    led_debug("Process line");
    led_fn_impl impl = LED_FN_TABLE[led.fn_id].impl;
    if (impl)
        (*impl)();
    else
        led_assert(FALSE, LED_ERR_ARG, "Function not implemented: %s", LED_FN_TABLE[led.fn_id].long_name);
}

//-----------------------------------------------
// LED main
//-----------------------------------------------

int main(int argc, char* argv[]) {
    led_init(argc, argv);

    if (led.o_help)
        led_help();
    else
        while (led_next_file()) {
            while (led_read_line()) {
                led_select();
                if (led.curline.selected == !led.o_sel_invert)
                    led_process();
                if (led.curline.selected == !led.o_sel_invert || !led.o_output_selected)
                    led_write_line();
            }
        }

    led_free();
    return 0;
}
