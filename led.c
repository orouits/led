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
    int rc = led_str_match(arg, "^[a-z_]+:$");
    if (rc) {
        led.fn_id = -1;
        led_debug("Funcion table max: %d", led_fn_table_size());
        for (int i = 0; i < led_fn_table_size(); i++) {
            led_fn_struct* fn_desc = led_fn_table_descriptor(i);
            if ( led_str_equal(arg, fn_desc->short_name) || led_str_equal(arg, fn_desc->long_name) ) {
                led.fn_id = i;
                led_debug("Function found: %d", led.fn_id);
                break;
            }
        }
        led_assert(led.fn_id >= 0, LED_ERR_ARG, "Unknown function: %s", arg);
        led_assert(led_fn_table_descriptor(led.fn_id)->impl, LED_ERR_ARG, "Function not yet implemented: %s", led_fn_table_descriptor(led.fn_id)->long_name);
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
    led_fn_struct* fn_desc = led_fn_table_descriptor(led.fn_id);
    led_debug("Configure function: %s (%d)", fn_desc->long_name, led.fn_id);

    const char* format = fn_desc->args_fmt;
    for (int i=0; i < LED_FARG_MAX && format[i]; i++) {
        if (format[i] == 'R') {
            // TODO: ?regexname search operator
            led_assert(led.fn_arg[i].str != NULL, LED_ERR_ARG, "function arg %d: missing regex\n%s", i+1, fn_desc->help_format);
            led.fn_arg[i].regex = led_regex_compile(led.fn_arg[i].str);
            led_debug("Arg regex found: %d", i);
        }
        else if (format[i] == 'r') {
            if (led.fn_arg[i].str) {
                led.fn_arg[i].regex = led_regex_compile(led.fn_arg[i].str);
                led_debug("Arg regex found: %d", i);
            }
        }
        else if (format[i] == 'N') {
            led_assert(led.fn_arg[i].str != NULL, LED_ERR_ARG, "function arg %d: missing number\n%s", i+1, fn_desc->help_format);
            led.fn_arg[i].val = atol(led.fn_arg[i].str);
            led_debug("Arg numerical found: %d", i);
        }
        else if (format[i] == 'n') {
            if (led.fn_arg[i].str) {
                led.fn_arg[i].val = atol(led.fn_arg[i].str);
                led_debug("Arg numerical found: %d", i);
            }
        }
        else if (format[i] == 'S') {
            led_assert(led.fn_arg[i].str != NULL, LED_ERR_ARG, "function arg %d: missing string\n%s", i+1, fn_desc->help_format);
            led_debug("Arg string found: %d", i);
        }
        else if (format[i] == 's') {
            if (led.fn_arg[i].str) {
                led_debug("Arg string found: %d", i);
            }
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

    // pre-configure the processor command
    led_init_config();
}

void led_help() {
    const char* DASHS = "----------------------------------------------------------------------------------------------------";
    fprintf(stderr,
"\
Led (line editor) aims to be a tool that can replace grep/sed and others text utilities often chained,\n\
for simple automatic word processing based on PCRE2 modern regular expressions.\n\
\n\
## Synopsis\n\
    Files content processing:    led [<selector>] [<processor>] [-options] -f [files] ...\n\
    Piped content processing:    cat <file> | led [<selector>] [<processor>] [-options] | led ...\n\
    Massive files processing:    ls -1 <dir> | led [<selector>] [<processor>] [-options] -F -I -f | led ...\n\
\n\
## Selector:\n\
    <regex>              => select all lines matching with <regex>\n\
    <n>                  => select line <n>\n\
    <regex> <regex_stop> => select group of lines starting matching <regex> (included) until matching <regex_stop> (excluded)\n\
    <regex> <count>      => select group of lines starting matching <regex> (included) until <count> lines are selected\n\
    <n>     <regex_stop> => select group of lines starting line <n> (included) until matching <regex_stop> (excluded)\n\
    <n>     <count>      => select group of lines starting line <n> (included) until <count> lines are selected\n\
\n\
## Processor:\n\
    <function>: [arg] ...\n\
\n\
## Global options\n\
    -z  end of line is 0\n\
    -v  verbose to STDERR\n\
    -r  report to STDERR\n\
    -q  quiet, do not ouptut anything (exit code only)\n\
    -e  exit code on value\n\
\n\
## Selector Options:\n\
    -n  invert selection\n\
    -b  group selection as blocks of contiguous lines before function process\n\
    -s  output only selected\n\
\n\
## File Options:\n\
    -f          read filenames from STDIN instead of content or from command line if followed file names (file section)\n\
    -F          write filenames to STDOUT instead of content.\n\
                    Combined with -f -I, this option allows advanced massive files transformations through pipes.\n\
    -I          modify files inplace\n\
    -W<path>    write content to a fixed file\n\
    -A<path>    append content to a fixed file\n\
    -E<ext>     write content to <current filename>.<ext>\n\
    -E          write content to <current filename>.<NNN>\n\
                    The ext number is computed regarding existing files.\n\
    -D<dir>     write files in <dir>.\n\
    -U          write unchanged filenames\n\
\n\
## Processor commands:\n\n\
"
    );
    fprintf(stderr, "|%.5s|%.20s|%.10s|%.50s|%.40s|\n", DASHS, DASHS, DASHS, DASHS, DASHS);
    fprintf(stderr, "| %-4s| %-19s| %-9s| %-49s| %-39s|\n", "Id", "Name", "Short", "Description", "Format");
    fprintf(stderr, "|%.5s|%.20s|%.10s|%.50s|%.40s|\n", DASHS, DASHS, DASHS, DASHS, DASHS);
    for (int i=0; i < led_fn_table_size(); i++) {
        led_fn_struct* fn_desc = led_fn_table_descriptor(i);
        if (!fn_desc->impl) fprintf(stderr, "\e[90m");
        fprintf(stderr, "| %-4d| %-19s| %-9s| %-49s| %-39s|\n",
            i,
            fn_desc->long_name,
            fn_desc->short_name,
            fn_desc->help_desc,
            fn_desc->help_format
        );
        if (!fn_desc->impl) fprintf(stderr, "\e[0m");
    }
    fprintf(stderr, "|%.5s|%.20s|%.10s|%.50s|%.40s|\n", DASHS, DASHS, DASHS, DASHS, DASHS);
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
    led_fn_struct* fn_desc = led_fn_table_descriptor(led.fn_id);
    if (fn_desc->impl)
        (fn_desc->impl)();
    else
        led_assert(FALSE, LED_ERR_ARG, "Function not implemented: %s", fn_desc->long_name);
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
