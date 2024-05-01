#include "led.h"


const char* LED_SEC_TABLE[] = {
    "selector",
    "function",
    "files",
};


led_t led;

//-----------------------------------------------
// LED tech trace and error functions
//-----------------------------------------------

void led_free() {
    if (led.opt.file_in && led.file_in.file) {
        fclose(led.file_in.file);
        led.file_in.file = NULL;
        lstr_empty(&led.file_in.name);
    }
    if (led.opt.file_out && led.file_out.file) {
        fclose(led.file_out.file);
        led.file_out.file = NULL;
        lstr_empty(&led.file_out.name);
    }
    if (led.sel.regex_start != NULL) {
        pcre2_code_free(led.sel.regex_start);
        led.sel.regex_start = NULL;
    }
    if (led.sel.regex_stop != NULL) {
        pcre2_code_free(led.sel.regex_stop);
        led.sel.regex_stop = NULL;
    }
    for(size_t i = 0; i < led.func_count; i++) {
        led_fn_t* pfunc = &led.func_list[i];
        if (pfunc->regex != NULL) {
            if (pfunc->regex != LED_REGEX_ALL_LINE) // will be deleted in a dedicated function.
                pcre2_code_free(pfunc->regex);
            pfunc->regex = NULL;
        }
        for (size_t i = 0; i < pfunc->arg_count; i++) {
            if (pfunc->arg[i].regex != NULL) {
                pcre2_code_free(pfunc->arg[i].regex);
                pfunc->arg[i].regex = NULL;
            }
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

int led_init_opt(lstr* arg) {
    int rc = lstr_match_pat(arg, "^-[a-zA-Z]+");
    if (rc) {
        led_debug("arg option: %s", lstr_str(arg));
        for(size_t opti = 1; opti < arg->len; opti++) {
            char opt = lstr_char_at(arg, opti);
            char* optstr = lstr_str_at(arg, opti + 1);
            led_debug("option: %c sitcked string: %s", opt, optstr);
            switch (opt) {
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
                led.opt.file_in = LED_INPUT_FILE;
                break;
            case 'F':
                led_assert(!led.opt.file_out, LED_ERR_ARG, "Bad option -%c, output file mode already set", opt);
                led_assert(!led.opt.exec, LED_ERR_ARG, "Bad option -%c, exec mode already set", opt);
                led.opt.file_out = LED_OUTPUT_FILE_INPLACE;
                break;
            case 'W':
                led_assert(!led.opt.file_out, LED_ERR_ARG, "Bad option -%c, output file mode already set", opt);
                led_assert(!led.opt.exec, LED_ERR_ARG, "Bad option -%c, exec mode already set", opt);
                led.opt.file_out = LED_OUTPUT_FILE_WRITE;
                lstr_init_str(&led.opt.file_out_path, optstr);
                led_debug("Option path: %s", lstr_str(&led.opt.file_out_path));
                opti = arg->len;
                break;
            case 'A':
                led_assert(!led.opt.file_out, LED_ERR_ARG, "Bad option -%c, output file mode already set", opt);
                led_assert(!led.opt.exec, LED_ERR_ARG, "Bad option -%c, exec mode already set", opt);
                led.opt.file_out = LED_OUTPUT_FILE_APPEND;
                lstr_init(&led.opt.file_out_path, optstr, 0);
                led_debug("Option path: %s", lstr_str(&led.opt.file_out_path));
                opti = arg->len;
                break;
            case 'E':
                led_assert(!led.opt.file_out, LED_ERR_ARG, "Bad option -%c, output file mode already set", opt);
                led_assert(!led.opt.exec, LED_ERR_ARG, "Bad option -%c, exec mode already set", opt);
                led.opt.file_out = LED_OUTPUT_FILE_NEWEXT;
                led.opt.file_out_extn = atoi(optstr);
                if (led.opt.file_out_extn <= 0)
                    lstr_init(&led.opt.file_out_ext, optstr, 0);
                led_debug("Option ext: %s", lstr_str(&led.opt.file_out_ext));
                opti =arg->len;
                break;
            case 'D':
                led_assert(!led.opt.file_out, LED_ERR_ARG, "Bad option -%c, output file mode already set", opt);
                led_assert(!led.opt.exec, LED_ERR_ARG, "Bad option -%c, exec mode already set", opt);
                lstr_init(&led.opt.file_out_dir, optstr, 0);
                led.opt.file_out = LED_OUTPUT_FILE_DIR;
                led_debug("Option dir: %s", lstr_str(&led.opt.file_out_dir));
                opti = arg->len;
                break;
            case 'U':
                led.opt.file_out_unchanged = TRUE;
                break;
            case 'X':
                led_assert(!led.opt.file_out, LED_ERR_ARG, "Bad option -%c, output file mode already set", opt);
                led.opt.exec = TRUE;
                break;
            default:
                led_assert(FALSE, LED_ERR_ARG, "Unknown option: -%c", opt);
            }
        }
    }
    return rc;
}

int led_init_func(lstr* arg) {
    int is_func = FALSE;
    char fsep ='\0';
    if ( (is_func = lstr_match(arg, LED_REGEX_FUNC)) ) fsep = '/';
    else if ( (is_func = lstr_match(arg, LED_REGEX_FUNC2)) ) fsep = ':';

    if (is_func) {
        // check if additional func can be defined
        led_assert(led.func_count < LED_FUNC_MAX, LED_ERR_ARG, "Maximum functions reached %d", LED_FUNC_MAX );

        // search for function
        lstr fname;
        lstr_cut_next(arg, fsep, &fname);
        int ifunc = led.func_count++;
        led_fn_t* pfunc = &led.func_list[ifunc];
        led_debug("Funcion table max: %d", led_fn_table_size());
        for (pfunc->id = 0; pfunc->id < led_fn_table_size(); pfunc->id++) {
            led_fn_desc_t* pfn_desc = led_fn_table_descriptor(pfunc->id);
            if (lstr_equal_str(&fname, pfn_desc->short_name) || lstr_equal_str(&fname, pfn_desc->long_name)) {
                led_debug("Function found: %d", pfunc->id);
                break;
            }
        }

        // check if func is usable
        led_assert(pfunc->id < led_fn_table_size(), LED_ERR_ARG, "Unknown function: %s", lstr_str(&fname));
        led_assert(led_fn_table_descriptor(pfunc->id)->impl != NULL, LED_ERR_ARG, "Function not yet implemented in: %s", led_fn_table_descriptor(pfunc->id)->long_name);

        // compile zone regex if given
        lstr regx;
        lstr_cut_next(arg, fsep, &regx);
        if (!lstr_isempty(&regx)) {
            led_debug("Regex found: %s", lstr_str(&regx));
            pfunc->regex = lstr_regex_compile(&regx);
        }
        else {
            led_debug("Regex NOT found, fixed to the whole line");
            pfunc->regex = LED_REGEX_ALL_LINE;
        }

        // store func arguments
        while(!lstr_isempty(arg)) {
            // check if additional func arg can be defined
            led_assert(pfunc->arg_count < LED_FARG_MAX, LED_ERR_ARG, "Maximum function argments reached %d", LED_FARG_MAX );
            lstr* farg = &(pfunc->arg[pfunc->arg_count++].sval);
            lstr_cut_next(arg, fsep, farg);
            led_debug("Function argument found: %s", lstr_str(farg));
        }
    }
    return is_func;
}

int led_init_sel(lstr* arg) {
    int rc = TRUE;
    if (lstr_match_pat(arg, "^\\+[0-9]+$") && led.sel.type_start == SEL_TYPE_REGEX) {
        led.sel.val_start = strtol(arg->str, NULL, 10);
        led_debug("Selector start: shift after regex (%d)", led.sel.val_start);
    }
    else if (!led.sel.type_start) {
        if (lstr_match(arg, LED_REGEX_INTEGER)) {
            led.sel.type_start = SEL_TYPE_COUNT;
            led.sel.val_start = strtol(arg->str, NULL, 10);
            led_debug("Selector start: type number (%d)", led.sel.val_start);
        }
        else {
            led.sel.type_start = SEL_TYPE_REGEX;
            led.sel.regex_start = lstr_regex_compile(arg);
            led_debug("Selector start: type regex (%s)", lstr_str(arg));
        }
    }
    else if (!led.sel.type_stop) {
        if (lstr_match(arg, LED_REGEX_INTEGER)) {
            led.sel.type_stop = SEL_TYPE_COUNT;
            led.sel.val_stop = strtol(arg->str, NULL, 10);
            led_debug("Selector stop: type number (%d)", led.sel.val_stop);
        }
        else {
            led.sel.type_stop = SEL_TYPE_REGEX;
            led.sel.regex_stop = lstr_regex_compile(arg);
            led_debug("Selector stop: type regex (%s)", lstr_str(arg));
        }
    }
    else rc = FALSE;

    return rc;
}

void led_init_config() {
    for (size_t ifunc = 0; ifunc < led.func_count; ifunc++) {
        led_fn_t* pfunc = &led.func_list[ifunc];

        led_fn_desc_t* pfn_desc = led_fn_table_descriptor(pfunc->id);
        led_debug("Configure function: %s (%d)", pfn_desc->long_name, pfunc->id);

        const char* format = pfn_desc->args_fmt;
        for (size_t i=0; format[i] && i < LED_FARG_MAX; i++) {
            if (format[i] == 'R') {
                led_assert(lstr_isinit(&pfunc->arg[i].sval), LED_ERR_ARG, "function arg %i: missing regex\n%s", i+1, pfn_desc->help_format);
                pfunc->arg[i].regex = lstr_regex_compile(&pfunc->arg[i].sval);
                led_debug("function arg %i: regex found", i+1);
            }
            else if (format[i] == 'r') {
                if (lstr_isinit(&pfunc->arg[i].sval)) {
                    pfunc->arg[i].regex = lstr_regex_compile(&pfunc->arg[i].sval);
                    led_debug("function arg %i: regex found", i+1);
                }
            }
            else if (format[i] == 'N') {
                led_assert(lstr_isinit(&pfunc->arg[i].sval), LED_ERR_ARG, "function arg %i: missing number\n%s", i+1, pfn_desc->help_format);
                pfunc->arg[i].val = atol(lstr_str(&pfunc->arg[i].sval));
                led_debug("function arg %i: numeric found: %li", i+1, pfunc->arg[i].val);
                // additionally compute the positive unsigned value to help
                pfunc->arg[i].uval = pfunc->arg[i].val < 0 ? (size_t)(-pfunc->arg[i].val) : (size_t)pfunc->arg[i].val;
            }
            else if (format[i] == 'n') {
                if (lstr_isinit(&pfunc->arg[i].sval)) {
                    pfunc->arg[i].val = atol(lstr_str(&pfunc->arg[i].sval));
                    led_debug("function arg %i: numeric found: %li", i+1, pfunc->arg[i].val);
                    // additionally compute the positive unsigned value to help
                    pfunc->arg[i].uval = pfunc->arg[i].val < 0 ? (size_t)(-pfunc->arg[i].val) : (size_t)pfunc->arg[i].val;
                }
            }
            else if (format[i] == 'P') {
                led_assert(lstr_isinit(&pfunc->arg[i].sval), LED_ERR_ARG, "function arg %i: missing number\n%s", i+1, pfn_desc->help_format);
                pfunc->arg[i].val = atol(lstr_str(&pfunc->arg[i].sval));
                led_assert(pfunc->arg[i].val >= 0, LED_ERR_ARG, "function arg %i: not a positive number\n%s", i+1, pfn_desc->help_format);
                pfunc->arg[i].uval = (size_t)pfunc->arg[i].val;
                led_debug("function arg %i: positive numeric found: %lu", i+1, pfunc->arg[i].uval);
            }
            else if (format[i] == 'p') {
                if (lstr_isinit(&pfunc->arg[i].sval)) {
                    pfunc->arg[i].val = atol(lstr_str(&pfunc->arg[i].sval));
                    led_assert(pfunc->arg[i].val >= 0, LED_ERR_ARG, "function arg %i: not a positive number\n%s", i+1, pfn_desc->help_format);
                    pfunc->arg[i].uval = (size_t)pfunc->arg[i].val;
                    led_debug("function arg %i: positive numeric found: %lu", i+1, pfunc->arg[i].uval);
                }
            }
            else if (format[i] == 'S') {
                led_assert(lstr_isinit(&pfunc->arg[i].sval), LED_ERR_ARG, "function arg %i: missing string\n%s", i+1, pfn_desc->help_format);
                led_debug("function arg %i: string found: %s", i+1, lstr_str(&pfunc->arg[i].sval));
            }
            else if (format[i] == 's') {
                if (lstr_isinit(&pfunc->arg[i].sval)) {
                    led_debug("function arg %i: string found: %s", i+1, lstr_str(&pfunc->arg[i].sval));
                }
            }
            else {
                led_assert(TRUE, LED_ERR_ARG, "function arg %i: bad internal format (%s)", i+1, format);
            }
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
        lstr_decl_str(arg, argv[argi]);

        if (arg_section == ARGS_SEC_FILES) {
            led.file_names = argv + argi;
            led.file_count = argc - argi;
            led_debug("Arg is file: %s", lstr_str(&arg));
        }
        else if (arg_section < ARGS_SEC_FILES && led_init_opt(&arg)) {
            if (led.opt.file_in) arg_section = ARGS_SEC_FILES;
            led_debug("Arg is opt: %s", lstr_str(&arg));
        }
        else if (arg_section <= ARGS_SEC_FUNCT && led_init_func(&arg)) {
            arg_section = ARGS_SEC_FUNCT;
            led_debug("Arg is func: %s", lstr_str(&arg));
        }
        else if (arg_section == ARGS_SEC_SELECT && led_init_sel(&arg)) {
            led_debug("Arg is part of selector: %s", lstr_str(&arg));
        }
        else {
            led_assert(FALSE, LED_ERR_ARG, "Unknown or wrong argument: %s (%s section)", lstr_str(&arg), LED_SEC_TABLE[arg_section]);
        }
    }

    // if a process function is not defined show only selected
    led.opt.output_selected = led.opt.output_selected || led.func_count == 0;

    // init lstr file names with their buffers.
    lstr_init_buf(&led.file_in.name, led.file_in.buf_name);
    lstr_init_buf(&led.file_out.name, led.file_out.buf_name);

    led_line_reset(&led.line_read);
    led_line_reset(&led.line_prep);
    led_line_reset(&led.line_write);
    for (int i=0; i<LED_REG_MAX; i++)
        led_line_reset(&led.line_reg[i]);

    // pre-configure the processor command
    led_init_config();

    led_debug("Config sel count: %d", led.sel.count);
    led_debug("Config func count: %d", led.func_count);
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
    Piped content processing:    cat <file> | led [<selector>] [<processor>] [-opts] | led ...\n\
    Massive files processing:    ls -1 <dir> | led [<selector>] [<processor>] [-opts] -F -f | led ...\n\
\n\
## Selector:\n\
    <regex>              => select all lines matching with <regex>\n\
    <n>                  => select line <n>\n\
    <regex> <regex_stop> => select group of lines starting matching <regex> (included) until matching <regex_stop> (excluded)\n\
    <regex> <count>      => select group of lines starting matching <regex> (included) until <count> lines are selected\n\
    <n>     <regex_stop> => select group of lines starting line <n> (included) until matching <regex_stop> (excluded)\n\
    <n>     <count>      => select group of lines starting line <n> (included) until <count> lines are selected\n\
    +n      +n           => shift start/stop selector boundaries\n\
\n\
## Processor:\n\
    <function>/ (processor with no argument)\n\
    <function>/[regex]/[arg]/...\n\
    <function>//[arg]/... (interpret // as default regex '^.*$')\n\
\n\
## Global options\n\
    -v  verbose to STDERR\n\
    -r  report to STDERR\n\
    -q  quiet, do not ouptut anything (exit code only)\n\
    -e  exit code on value\n\
\n\
## Selector Options:\n\
    -n  invert selection\n\
    -p  pack contiguous selected line in one multi-line before function processing\n\
    -s  output only selected\n\
\n\
## File input options:\n\
    -f          read filenames from STDIN instead of content or from command line if followed file names (file section)\n\
\n\
## File output options:\n\
    -F          modify files inplace\n\
    -W<path>    write content to a fixed file\n\
    -A<path>    append content to a fixed file\n\
    -E<ext>     write content to <current filename>.<ext>\n\
    -D<dir>     write files in <dir>.\n\
    -X          execute lines.\n\
\n\
    All these options output the output filenames on STDOUT\n\
\n\
## Processor options\n\
    -m          output only processed maching zone when regex is used\n\
\n\
## Processor commands:\n\n\
"
   );
    fprintf(stderr, "|%.5s|%.20s|%.10s|%.50s|%.40s|\n", DASHS, DASHS, DASHS, DASHS, DASHS);
    fprintf(stderr, "| %-4s| %-19s| %-9s| %-49s| %-39s|\n", "Id", "Name", "Short", "Description", "Format");
    fprintf(stderr, "|%.5s|%.20s|%.10s|%.50s|%.40s|\n", DASHS, DASHS, DASHS, DASHS, DASHS);
    for (size_t i = 0; i < led_fn_table_size(); i++) {
        led_fn_desc_t* pfn_desc = led_fn_table_descriptor(i);
        if (!pfn_desc->impl) fprintf(stderr, "\e[90m");
        fprintf(stderr, "| %-4lu| %-19s| %-9s| %-49s| %-39s|\n",
            i,
            pfn_desc->long_name,
            pfn_desc->short_name,
            pfn_desc->help_desc,
            pfn_desc->help_format
       );
        if (!pfn_desc->impl) fprintf(stderr, "\e[0m");
    }
    fprintf(stderr, "|%.5s|%.20s|%.10s|%.50s|%.40s|\n", DASHS, DASHS, DASHS, DASHS, DASHS);
}

//-----------------------------------------------
// LED process functions
//-----------------------------------------------

void led_file_open_in() {
    led_debug("led_file_open_in");
    if (led.file_count) {
        lstr_cpy_chars(&led.file_in.name, led.file_names[0]);
        led.file_names++;
        led.file_count--;
        lstr_trim(&led.file_in.name);
        led_debug("open file from args: %s", lstr_str(&led.file_in.name));
        led.file_in.file = fopen(lstr_str(&led.file_in.name), "r");
        led_assert(led.file_in.file != NULL, LED_ERR_FILE, "File not found: %s", lstr_str(&led.file_in.name));
        led.report.file_in_count++;
    }
    else if (led.stdin_ispipe) {
        char buf_fname[LED_FNAME_MAX+1];
        char* fname = fgets(buf_fname, LED_FNAME_MAX, stdin);
        if (fname) {
            lstr_cpy_chars(&led.file_in.name, fname);
            lstr_trim(&led.file_in.name);
            led_debug("open file from stdin: [%s]", lstr_str(&led.file_in.name));
            led.file_in.file = fopen(lstr_str(&led.file_in.name), "r");
            led_assert(led.file_in.file != NULL, LED_ERR_FILE, "File not found: %s", lstr_str(&led.file_in.name));
            led.report.file_in_count++;
        }
    }
}

void led_file_close_in() {
    fclose(led.file_in.file);
    led.file_in.file = NULL;
    lstr_empty(&led.file_in.name);
}

void led_file_stdin() {
    if (led.file_in.file) {
        led_assert(led.file_in.file == stdin, LED_ERR_FILE, "File is not STDIN internal error: %s", lstr_str(&led.file_in.name));
        led.file_in.file = NULL;
        lstr_empty(&led.file_in.name);
    } else if (led.stdin_ispipe) {
        led.file_in.file = stdin;
        lstr_cpy_chars(&led.file_in.name, "STDIN");
    }
}

void led_file_open_out() {
    const char* mode = "";
    if (led.opt.file_out == LED_OUTPUT_FILE_INPLACE) {
        lstr_cpy(&led.file_out.name, &led.file_in.name);
        lstr_app_str(&led.file_out.name, ".part");
        mode = "w+";
    }
    else if (led.opt.file_out == LED_OUTPUT_FILE_WRITE) {
        lstr_cpy(&led.file_out.name, &led.opt.file_out_path);
        mode = "w+";
    }
    else if (led.opt.file_out == LED_OUTPUT_FILE_APPEND) {
        lstr_cpy(&led.file_out.name, &led.opt.file_out_path);
        mode = "a";
    }
    else if (led.opt.file_out == LED_OUTPUT_FILE_NEWEXT) {
        lstr_cpy(&led.file_out.name, &led.file_in.name);
        lstr_app(&led.file_out.name, &led.opt.file_out_ext);
        mode = "w+";
    }
    else if (led.opt.file_out == LED_OUTPUT_FILE_DIR) {
        lstr_decl(tmp, LED_FNAME_MAX+1);

        lstr_cpy(&led.file_out.name, &led.opt.file_out_dir);
        lstr_app_str(&led.file_out.name, "/");
        lstr_cpy(&tmp, &led.file_in.name);
        lstr_app(&led.file_out.name, lstr_basename(&tmp));
        mode = "w+";
    }
    led.file_out.file = fopen(lstr_str(&led.file_out.name), mode);
    led_assert(led.file_out.file != NULL, LED_ERR_FILE, "File open error: %s", lstr_str(&led.file_out.name));
    led.report.file_out_count++;
}

void led_file_close_out() {
    lstr_decl(tmp, LED_FNAME_MAX+1);

    fclose(led.file_out.file);
    led.file_out.file = NULL;
    if (led.opt.file_out == LED_OUTPUT_FILE_INPLACE) {
        lstr_cpy(&tmp, &led.file_out.name);
        lstr_trunk_end(&tmp, 5);
        led_debug("Rename: %s ==> %s", lstr_str(&led.file_out.name), lstr_str(&tmp));
        int syserr = remove(lstr_str(&tmp));
        led_assert(!syserr, LED_ERR_FILE, "File remove error: %d => %s", syserr, lstr_str(&tmp));
        rename(lstr_str(&led.file_out.name), lstr_str(&tmp));
        led_assert(!syserr, LED_ERR_FILE, "File rename error: %d => %s", syserr, lstr_str(&led.file_out.name));
        lstr_cpy(&led.file_out.name, &tmp);
    }
}

void led_file_print_out() {
    fwrite(lstr_str(&led.file_out.name), sizeof *lstr_str(&led.file_out.name), lstr_len(&led.file_out.name), stdout);
    fwrite("\n", sizeof *lstr_str(&led.file_out.name), 1, stdout);
    fflush(stdout);
    lstr_empty(&led.file_out.name);
}

void led_file_stdout() {
    led.file_out.file = stdout;
    lstr_cpy_chars(&led.file_out.name, "STDOUT");
}

int led_file_next() {
    led_debug("Next file ---------------------------------------------------");

    if (led.opt.file_out && led.file_out.file && ! (led.opt.file_out == LED_OUTPUT_FILE_WRITE || led.opt.file_out == LED_OUTPUT_FILE_APPEND)) {
        led_file_close_out();
        led_file_print_out();
    }

    if (led.opt.file_in && led.file_in.file)
        led_file_close_in();

    if (led.opt.file_in)
        led_file_open_in();
    else
        led_file_stdin();

    if (! led.file_out.file && led.file_in.file) {
        if (led.opt.file_out)
            led_file_open_out();
        else
            led_file_stdout();
    }

    if (! led.file_in.file && led.file_out.file) {
        led_file_close_out();
        led_file_print_out();
    }

    led_debug("Input from: %s", lstr_str(&led.file_in.name));
    led_debug("Output to: %s", lstr_str(&led.file_out.name));

    led.sel.total_count = 0;
    led.sel.count = 0;
    led.sel.selected = FALSE;
    led.sel.inboundary = FALSE;
    return led.file_in.file != NULL;
}

int led_process_read() {
    led_debug("led_process_read");
    if (!led_line_isinit(&led.line_read)) {
        lstr_init(&led.line_read.sval, fgets(led.line_read.buf, sizeof led.line_read.buf, led.file_in.file), sizeof led.line_read.buf);
        if (led_line_isinit(&led.line_read)) {
            lstr_unapp_char(&led.line_read.sval, '\n');
            led.line_read.zone_start = 0;
            led.line_read.zone_stop = led.line_read.sval.len;
            led.line_read.selected = FALSE;
            led.sel.total_count++;
            led_debug("Read line: (%d) len=%d", led.sel.total_count, led.line_read.sval.len);
        }
        else
            led_debug("Read line is NULL: (%d)", led.sel.total_count);
    }
    return led_line_isinit(&led.line_read);
}

void led_process_write() {
    led_debug("led_process_write");
    if (led_line_isinit(&led.line_write)) {
        led_debug("Write line: (%d) len=%d", led.sel.total_count, lstr_len(&led.line_write.sval));
        lstr_app_char(&led.line_write.sval, '\n');
        led_debug("Write line to %s", lstr_str(&led.file_out.name));
        fwrite(lstr_str(&led.line_write.sval), sizeof *lstr_str(&led.line_write.sval), lstr_len(&led.line_write.sval), led.file_out.file);
        fflush(led.file_out.file);
    }
    led_line_reset(&led.line_write);
}

void led_process_exec() {
    led_debug("led_process_exec");
    if (led_line_isinit(&led.line_write) && !lstr_isblank(&led.line_write.sval)) {
        led_debug("Exec line: (%d) len=%d", led.sel.total_count, lstr_len(&led.line_write.sval));
        led_debug("Exec command %s", lstr_str(&led.line_write.sval));

        FILE *fp = popen(lstr_str(&led.line_write.sval), "r");
        led_assert(fp != NULL, LED_ERR_ARG, "Command error");
        lstr_decl(output, 4096);
        while (lstr_isinit(lstr_init(&output, fgets(lstr_str(&output), lstr_size(&output), fp), lstr_size(&output)))) {
            fwrite(lstr_str(&output), sizeof *lstr_str(&output), lstr_len(&output), led.file_out.file);
            fflush(led.file_out.file);
        }
        pclose(fp);
    }
    led_line_reset(&led.line_write);
}

int led_process_selector() {
    led_debug("led_process_selector");

    int ready = FALSE;
    // stop selection on stop boundary
    if (!led_line_isinit(&led.line_read)
        || (led.sel.type_stop == SEL_TYPE_NONE && led.sel.type_start != SEL_TYPE_NONE && led.sel.shift == 0)
        || (led.sel.type_stop == SEL_TYPE_COUNT && led.sel.count >= led.sel.val_stop)
        || (led.sel.type_stop == SEL_TYPE_REGEX && lstr_match(&led.line_read.sval, led.sel.regex_stop))
        ) {
        led.sel.inboundary = FALSE;
        led.sel.count = 0;
    }
    if (led.sel.shift > 0) led.sel.shift--;

    // start selection on start boundary
    if (led_line_isinit(&led.line_read) && (
        led.sel.type_start == SEL_TYPE_NONE
        || (led.sel.type_start == SEL_TYPE_COUNT && led.sel.total_count == led.sel.val_start)
        || (led.sel.type_start == SEL_TYPE_REGEX && lstr_match(&led.line_read.sval, led.sel.regex_start))
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
            if (!(led.opt.filter_blank && lstr_isblank(&led.line_read.sval))) {
                if (lstr_iscontent(&led.line_prep.sval))
                    lstr_app_char(&led.line_prep.sval, '\n');
                lstr_app(&led.line_prep.sval, &led.line_read.sval);
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
            if (!(led.opt.filter_blank && lstr_isblank(&led.line_read.sval)))
                led_line_cpy(&led.line_prep, &led.line_read);
            led_line_reset(&led.line_read);
            ready = TRUE;
        }
    }
    else {
        if (!(led.opt.filter_blank && lstr_isblank(&led.line_read.sval)))
            led_line_cpy(&led.line_prep, &led.line_read);
        led_line_reset(&led.line_read);
        ready = TRUE;
    }

    led_debug("Line ready to process: %d", ready);
    return ready;
}

void led_process_functions() {
    led_debug("led_process_functions");
    led_debug("Process line prep (isinit: %d len: %d)", led_line_isinit(&led.line_prep), lstr_len(&led.line_prep.sval));
    if (led_line_isinit(&led.line_prep)) {
        led_debug("prep line is init");
        if (led_line_selected(&led.line_prep)) {
            led_debug("prep line is selected");
            if (led.func_count > 0) {
                for (size_t ifunc = 0; ifunc < led.func_count; ifunc++) {
                    led_fn_t* pfunc = &led.func_list[ifunc];
                    led_fn_desc_t* pfn_desc = led_fn_table_descriptor(pfunc->id);
                    led.report.line_match_count++;
                    led_debug("Process function %s", pfn_desc->long_name);
                    (pfn_desc->impl)(pfunc);
                    led_line_cpy(&led.line_prep, &led.line_write);
                }
            }
            else {
                led_debug("No function copy (len: %d)", lstr_len(&led.line_prep.sval));
                led_line_cpy(&led.line_write, &led.line_prep);
            }
        }
        else if (!led.opt.output_selected) {
            led_debug("Copy unselected to dest");
            led_line_cpy(&led.line_write, &led.line_prep);
        }
    }
    led_debug("Process result line write (len=%d)", lstr_len(&led.line_write.sval));
    led_line_reset(&led.line_prep);
}

void led_report() {
    fprintf(stderr, "\nLED report:\n");
    fprintf(stderr, "Line match count: %ld\n", led.report.line_match_count);
    fprintf(stderr, "\n");
    fprintf(stderr, "File input count: %ld\n", led.report.file_in_count);
    fprintf(stderr, "File output count: %ld\n", led.report.file_out_count);
    fprintf(stderr, "File match count: %ld\n", led.report.file_match_count);
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
                    led_process_functions();
                    if (led.opt.exec)
                        led_process_exec();
                    else
                        led_process_write();
                }
            } while(isline);
        }
    if (led.opt.report)
        led_report();
    led_free();
    return 0;
}
