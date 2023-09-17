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
// LED tech trace and error functions
//-----------------------------------------------

void led_free() {
    if ( led.curfile.file ) {
        fclose(led.curfile.file);
        led.curfile.file = NULL;
        led.curfile.name = NULL;
    }
    if ( led.sel.regex_start != NULL) {
        pcre2_code_free(led.sel.regex_start);
        led.sel.regex_start = NULL;
    }
    if ( led.sel.regex_stop != NULL) {
        pcre2_code_free(led.sel.regex_stop);
        led.sel.regex_stop = NULL;
    }
    if ( led.fn_regex != NULL) {
        pcre2_code_free(led.fn_regex);
        led.fn_regex = NULL;
    }
    for (int i=0; i<LED_FARG_MAX; i++ ) {
        if (led.fn_arg[i].regex != NULL) {
            pcre2_code_free(led.fn_arg[i].regex);
            led.fn_arg[i].regex = NULL;
        }
    }
    led_regex_free();

}

void led_assert(int cond, int code, const char* message, ...) {
    if (!cond) {
        if (message) {
            va_list args;
            va_start(args, message);
            vsnprintf((char*)led.buf_message, sizeof(led.buf_message), message, args);
            va_end(args);
            fprintf(stderr, "\e[31m[LED_ERROR] %s\e[0m\n", led.buf_message);
        }
        led_free();
        exit(code);
    }
}

void led_assert_pcre(int rc) {
    if (rc < 0) {
        pcre2_get_error_message(rc, led.buf_message, LED_MSG_MAX);
        fprintf(stderr, "\e[31m[LED_ERROR_PCRE] %s\e[0m\n", led.buf_message);
        led_free();
        exit(LED_ERR_PCRE);
    }
}

void led_debug(const char* message, ...) {
    if (led.opt.verbose) {
        va_list args;
        va_start(args, message);
        vsnprintf((char*)led.buf_message, LED_MSG_MAX, message, args);
        va_end(args);
        fprintf(stderr, "\e[34m[LED_DEBUG] %s\e[0m\n", led.buf_message);
    }
}

//-----------------------------------------------
// LED init functions
//-----------------------------------------------

int led_init_opt(const char* arg) {
    int rc = led_str_match("^-[a-zA-Z]+", arg);
    if ( rc ) {
        int argl = strlen(arg);
        for(int opti = 1; opti < argl; opti++) {
            led_debug("options: %c", arg[opti]);
            switch (arg[opti]) {
            case 'h':
                led.opt.help = TRUE;
                break;
            case 'v':
                led.opt.verbose = TRUE;
                break;
            case 'q':
                led.opt.quiet = TRUE;
                break;
            case 'r':
                led.opt.report = TRUE;
                break;
            case 'x':
                led.opt.exit_mode = LED_EXIT_VAL;
                break;
            case 'n':
                led.opt.invert_selected = TRUE;
                break;
            case 'm':
                led.opt.output_match = TRUE;
                break;
            case 'p':
                led.opt.pack_selected = TRUE;
                break;
            case 's':
                led.opt.output_selected = TRUE;
                break;
            case 'e':
                led.opt.filter_blank = TRUE;
                break;
            case 'f':
                led.opt.file_in = TRUE;
                break;
            case 'F':
                led.opt.file_out = TRUE;
                break;
            case 'I':
                led_assert(!led.opt.file_out_mode, LED_ERR_ARG, "Bad option -%c, output file mode already set", arg[opti]);
                led.opt.file_out_mode = LED_FILE_OUT_INPLACE;
                break;
            case 'W':
                led_assert(!led.opt.file_out_mode, LED_ERR_ARG, "Bad option -%c, output file mode already set", arg[opti]);
                led.opt.file_out_mode = LED_FILE_OUT_WRITE;
                led.opt.file_out_path = arg + opti + 1;
                opti = argl;
                break;
            case 'A':
                led_assert(!led.opt.file_out_mode, LED_ERR_ARG, "Bad option -%c, output file mode already set", arg[opti]);
                led.opt.file_out_mode = LED_FILE_OUT_APPEND;
                led.opt.file_out_path = arg + opti + 1;
                opti = argl;
                break;
            case 'E':
                led_assert(!led.opt.file_out_mode, LED_ERR_ARG, "Bad option -%c, output file mode already set", arg[opti]);
                led.opt.file_out_mode = LED_FILE_OUT_NEWEXT;
                led.opt.file_out_extn = atoi(arg + opti + 1);
                if ( led.opt.file_out_extn <= 0 )
                    led.opt.file_out_ext = arg + opti + 1;
                opti = argl;
                break;
            case 'D':
                led.opt.file_out_dir = arg + opti + 1;
                opti = argl;
                break;
            case 'U':
                led.opt.file_out_unchanged = TRUE;
                break;
            default:
                led_assert(FALSE, LED_ERR_ARG, "Unknown option: -%c", arg[opti]);
            }
        }
    }
    return rc;
}

int led_init_func(const char* arg) {
    int is_func = led_str_match("^[a-z0-9_]+:.*$", arg);
    if (is_func) {
        // search for function
        int isep =  led_char_pos_str(':', arg);
        led_debug("Funcion table max: %d", led_fn_table_size());
        for (led.fn_id = 0; led.fn_id < led_fn_table_size(); led.fn_id++) {
            led_fn_struct* fn_desc = led_fn_table_descriptor(led.fn_id);
            if ( led_str_equal_len(arg, fn_desc->short_name, isep) || led_str_equal_len(arg, fn_desc->long_name, isep) ) {
                led_debug("Function found: %d", led.fn_id);
                break;
            }
        }
        // check if func is usable
        led_assert(led.fn_id < led_fn_table_size(), LED_ERR_ARG, "Unknown function in: %s", arg);
        led_assert(led_fn_table_descriptor(led.fn_id)->impl != NULL, LED_ERR_ARG, "Function not yet implemented in: %s", led_fn_table_descriptor(led.fn_id)->long_name);

        // compile zone regex if given
        const char* rxstr = arg + isep + 1;
        if (rxstr[0] != '\0' ) {
            led_debug("Regex found: %s", rxstr);
            led.fn_regex = led_regex_compile(rxstr);
        }
        else {
            led_debug("Regex NOT found, fixed to ^.*$");
            led.fn_regex = led_regex_compile("^.*$");
        }
    }
    return is_func;
}

int led_init_func_arg(const char* arg) {
    int rc = !led.fn_arg[LED_FARG_MAX - 1].str;
    if (rc) {
        for (size_t i = 0; i < LED_FARG_MAX; i++) {
            if ( !led.fn_arg[i].str ) {
                led.fn_arg[i].str = arg;
                led.fn_arg[i].len = strlen(arg);
                break;
            }
        }
    }
    return rc;
}

int led_init_sel(const char* arg) {
    int rc = TRUE;
    if (led_str_match("^\\+[0-9]+$", arg) && led.sel.type_start == SEL_TYPE_REGEX) {
        led.sel.val_start = strtol(arg, NULL, 10);
        led_debug("Selector start: shift after regex (%d)", led.sel.val_start);
    }
    else if (!led.sel.type_start) {
        if (led_str_match("^[0-9]+$", arg)) {
            led.sel.type_start = SEL_TYPE_COUNT;
            led.sel.val_start = strtol(arg, NULL, 10);
            led_debug("Selector start: type number (%d)", led.sel.val_start);
        }
        else {
            led.sel.type_start = SEL_TYPE_REGEX;
            led.sel.regex_start = led_regex_compile(arg);
            led_debug("Selector start: type regex (%s)", arg);
        }
    }
    else if (!led.sel.type_stop) {
        if (led_str_match("^[0-9]+$", arg)) {
            led.sel.type_stop = SEL_TYPE_COUNT;
            led.sel.val_stop = strtol(arg, NULL, 10);
            led_debug("Selector stop: type number (%d)", led.sel.val_stop);
        }
        else {
            led.sel.type_stop = SEL_TYPE_REGEX;
            led.sel.regex_stop = led_regex_compile(arg);
            led_debug("Selector stop: type regex (%s)", arg);
        }
    }
    else rc = FALSE;

    return rc;
}

void led_init_config() {
    led_fn_struct* fn_desc = led_fn_table_descriptor(led.fn_id);
    led_debug("Configure function: %s (%d)", fn_desc->long_name, led.fn_id);

    const char* format = fn_desc->args_fmt;
    for (int i=0; i < LED_FARG_MAX && format[i]; i++) {
        if (format[i] == 'R') {
            led_assert(led.fn_arg[i].str != NULL, LED_ERR_ARG, "function arg %i: missing regex\n%s", i+1, fn_desc->help_format);
            led.fn_arg[i].regex = led_regex_compile(led.fn_arg[i].str);
            led_debug("function arg %i: regex found", i+1);
        }
        else if (format[i] == 'r') {
            if (led.fn_arg[i].str) {
                led.fn_arg[i].regex = led_regex_compile(led.fn_arg[i].str);
                led_debug("function arg %i: regex found", i+1);
            }
        }
        else if (format[i] == 'N') {
            led_assert(led.fn_arg[i].str != NULL, LED_ERR_ARG, "function arg %i: missing number\n%s", i+1, fn_desc->help_format);
            led.fn_arg[i].val = atol(led.fn_arg[i].str);
            led_debug("function arg %i: numeric found: %li", i+1, led.fn_arg[i].val);
            // additionally compute the positive unsigned value to help
            led.fn_arg[i].uval = led.fn_arg[i].val < 0 ? (size_t)(-led.fn_arg[i].val) : (size_t)led.fn_arg[i].val;
        }
        else if (format[i] == 'n') {
            if (led.fn_arg[i].str) {
                led.fn_arg[i].val = atol(led.fn_arg[i].str);
                led_debug("function arg %i: numeric found: %li", i+1, led.fn_arg[i].val);
                // additionally compute the positive unsigned value to help
                led.fn_arg[i].uval = led.fn_arg[i].val < 0 ? (size_t)(-led.fn_arg[i].val) : (size_t)led.fn_arg[i].val;
            }
        }
        else if (format[i] == 'P') {
            led_assert(led.fn_arg[i].str != NULL, LED_ERR_ARG, "function arg %i: missing number\n%s", i+1, fn_desc->help_format);
            led.fn_arg[i].val = atol(led.fn_arg[i].str);
            led_assert(led.fn_arg[i].val >= 0, LED_ERR_ARG, "function arg %i: not a positive number\n%s", i+1, fn_desc->help_format);
            led.fn_arg[i].uval = (size_t)led.fn_arg[i].val;
            led_debug("function arg %i: positive numeric found: %lu", i+1, led.fn_arg[i].uval);
        }
        else if (format[i] == 'p') {
            if (led.fn_arg[i].str) {
                led.fn_arg[i].val = atol(led.fn_arg[i].str);
                led_assert(led.fn_arg[i].val >= 0, LED_ERR_ARG, "function arg %i: not a positive numboptioptier\n%s", i+1, fn_desc->help_format);
                led.fn_arg[i].uval = (size_t)led.fn_arg[i].val;
                led_debug("function arg %i: positive numeric found: %lu", i+1, led.fn_arg[i].uval);
            }
        }
        else if (format[i] == 'S') {
            led_assert(led.fn_arg[i].str != NULL, LED_ERR_ARG, "function arg %i: missing string\n%s", i+1, fn_desc->help_format);
            led_debug("function arg %i: string found: %s", i+1, led.fn_arg[i].str);
        }
        else if (format[i] == 's') {
            if (led.fn_arg[i].str) {
                led_debug("function arg %i: string found: %s", i+1, led.fn_arg[i].str);
            }
        }
        else {
            led_assert(TRUE, LED_ERR_ARG, "function arg %i: bad internal format (%s)", i+1, format);
        }
    }
}

void led_init(int argc, char* argv[]) {
    led_debug("Init");

    led_regex_init();

    memset(&led, 0, sizeof(led));

    led.stdin_ispipe = !isatty(fileno(stdin));
    led.stdout_ispipe = !isatty(fileno(stdout));

    int arg_section = 0;
    for (int argi=1; argi < argc; argi++) {
        const char* arg = argv[argi];

        if (arg_section == ARGS_SEC_FILES ) {
            led.file_names = argv + argi;
            led.file_count = argc - argi;
        }
        else if (arg_section < ARGS_SEC_FILES && led_init_opt(arg) ) {
            if (led.opt.file_in) arg_section = ARGS_SEC_FILES;
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
    led.opt.output_selected = led.opt.output_selected || led.fn_id == 0;

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
    -v  verbose to STDERR\n\
    -r  report to STDERR\n\
    -q  quiet, do not ouptut anything (exit code only)\n\
    -e  exit code on value\n\
\n\
## Selector Options:\n\
    -n  invert selection\n\
    -p  pack contiguous selected line in one multi-line before function process\n\
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
## Processor options\n\
    -z <regex>  identify a processing matching zone into the line\n\
    -m          output only processed maching zone when regex is used\n\
\n\
## Processor commands:\n\n\
"
    );
    fprintf(stderr, "|%.5s|%.20s|%.10s|%.50s|%.40s|\n", DASHS, DASHS, DASHS, DASHS, DASHS);
    fprintf(stderr, "| %-4s| %-19s| %-9s| %-49s| %-39s|\n", "Id", "Name", "Short", "Description", "Format");
    fprintf(stderr, "|%.5s|%.20s|%.10s|%.50s|%.40s|\n", DASHS, DASHS, DASHS, DASHS, DASHS);
    for (size_t i = 0; i < led_fn_table_size(); i++) {
        led_fn_struct* fn_desc = led_fn_table_descriptor(i);
        if (!fn_desc->impl) fprintf(stderr, "\e[90m");
        fprintf(stderr, "| %-4lu| %-19s| %-9s| %-49s| %-39s|\n",
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

int led_file_next() {
    led_debug("Next file");

    if ( led.opt.file_in ) {
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
    led.sel.total_count = 0;
    led.sel.count = 0;
    led.sel.selected = FALSE;
    led.sel.inboundary = FALSE;
    return led.curfile.file != NULL;
}

int led_process_read() {
    if (!led_line_defined(&led.line_read)) {
        led.line_read.str = fgets(led.line_read.buf, sizeof led.line_read.buf, led.curfile.file);
        if (led.line_read.str != NULL) {
            led.line_read.len = strlen(led.line_read.str);
            if (led.line_read.len > 0 && led.line_read.str[led.line_read.len - 1] == '\n')
                led.line_read.str[--led.line_read.len] = '\0';
            led.line_read.zone_start = 0;
            led.line_read.zone_stop = led.line_read.len;
            led.line_read.selected = FALSE;
            led.sel.total_count++;
            led_debug("Read line: (%d) len=%d", led.sel.total_count, led.line_read.len);
        }
        else
            led_debug("Read line is NULL: (%d)", led.sel.total_count);
    }
    return led_line_defined(&led.line_read);
}

void led_process_write() {
    if (led_line_defined(&led.line_write)) {
        led_debug("Write line: (%d) len=%d", led.sel.total_count, led.line_write.len);
        led_line_append_char(&led.line_write, '\n');
        fwrite(led.line_write.str, sizeof *led.line_write.str, led.line_write.len, stdout);
        fflush(stdout);
        led_line_reset(&led.line_write);
    }
}

int led_process_selector() {
    int ready = FALSE;
    // stop selection on stop boundary
    if (!led_line_defined(&led.line_read)
        || (led.sel.type_stop == SEL_TYPE_NONE && led.sel.type_start != SEL_TYPE_NONE && led.sel.shift == 0)
        || (led.sel.type_stop == SEL_TYPE_COUNT && led.sel.count >= led.sel.val_stop)
        || (led.sel.type_stop == SEL_TYPE_REGEX && led_regex_match(led.sel.regex_stop, led.line_read.str, led.line_read.len)) ) {
        led.sel.inboundary = FALSE;
        led.sel.count = 0;
    }
    if (led.sel.shift > 0) led.sel.shift--;

    // start selection on start boundary
    if (led_line_defined(&led.line_read)
        && (
            led.sel.type_start == SEL_TYPE_NONE
            || (led.sel.type_start == SEL_TYPE_COUNT && led.sel.total_count == led.sel.val_start)
            || (led.sel.type_start == SEL_TYPE_REGEX && led_regex_match(led.sel.regex_start, led.line_read.str, led.line_read.len))
        )) {

        led.sel.inboundary = TRUE;
        led.sel.shift = led.sel.val_start;
        led.sel.count = 0;
    }

    led.sel.selected = led.sel.inboundary && led.sel.shift == 0;
    led.line_read.selected = led.sel.selected == !led.opt.invert_selected;
    led_debug("Select: inboundary=%d, shift=%d selected=%d line selected=%d", led.sel.inboundary, led.sel.shift, led.sel.selected, led.line_read.selected);

    if (led.sel.selected) led.sel.count++;

    if (led.opt.pack_selected) {
        if (led_line_selected(&led.line_read)) {
            led_debug("pack: append to ready");
            if (!(led.opt.filter_blank && led_line_isblank(&led.line_read))) {
                if ( led.line_prep.len > 0 )
                    led_line_append_char(&led.line_prep, '\n');
                led_line_append(&led.line_prep, &led.line_read);
            }
            led_line_select(&led.line_prep, TRUE);
            led_line_reset(&led.line_read);
        }
        else if (led_line_selected(&led.line_prep)) {
            led_debug("pack: ready to process");
            ready = TRUE;
        }
        else {
            led_debug("pack: no selection");
            if (!(led.opt.filter_blank && led_line_isblank(&led.line_read)))
                led_line_copy(&led.line_prep, &led.line_read);
            led_line_reset(&led.line_read);
            ready = TRUE;
        }
    }
    else {
        if (!(led.opt.filter_blank && led_line_isblank(&led.line_read)))
            led_line_copy(&led.line_prep, &led.line_read);
        led_line_reset(&led.line_read);
        ready = TRUE;
    }

    led_debug("Line ready to process: %d", ready);
    return ready;
}

void led_process_function() {
    led_debug("Process line ready (len=%d)", led.line_prep.len);
    led_fn_struct* fn_desc = led_fn_table_descriptor(led.fn_id);
    if (led_line_defined(&led.line_prep) && led_line_selected(&led.line_prep)) {
        led_debug("Process function %s", fn_desc->long_name);
        (fn_desc->impl)();
    }
    else if (!led.opt.output_selected) {
        led_debug("Copy unselected to dest");
        led_line_copy(&led.line_write, &led.line_prep);
    }
    led_line_reset(&led.line_prep);
    led_debug("Result line dest (len=%d)", led.line_write.len);
}

//-----------------------------------------------
// LED main
//-----------------------------------------------

int main(int argc, char* argv[]) {
    led_init(argc, argv);

    if (led.opt.help)
        led_help();
    else
        while (led_file_next()) {
            int isline = FALSE;
            do {
                isline = led_process_read();
                if (led_process_selector()) {
                    led_process_function();
                    led_process_write();
                }
            } while(isline);
        }
    led_free();
    return 0;
}
