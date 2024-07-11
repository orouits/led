/***************************************************************************
 Copyright (C) 2024 - Olivier ROUITS <olivier.rouits@free.fr>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 USA
 ***************************************************************************/

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
        led_u8s_empty(&led.file_in.name);
    }
    if (led.opt.file_out && led.file_out.file) {
        fclose(led.file_out.file);
        led.file_out.file = NULL;
        led_u8s_empty(&led.file_out.name);
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

void led_assert(bool cond, int code, const char* message, ...) {
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

bool led_init_opt(led_u8s_t* arg) {
    bool rc = led_u8s_match_pat(arg, "^-[a-zA-Z]+");
    if (rc) {
        led_debug("arg option: %s", led_u8s_str(arg));
        size_t opti = 1;
        while(opti < arg->len) {
            u8c_t opt = led_u8s_char_next(arg, &opti);
            char* optstr = led_u8s_str_at(arg, opti);
            led_debug("option: %c sitcked string: %s", opt, optstr);
            switch (opt) {
            case 'h':
                led.opt.help = true;
                break;
            case 'v':
                led.opt.verbose = true;
                break;
            case 'q':
                led.opt.quiet = true;
                break;
            case 'r':
                led.opt.report = true;
                break;
            case 'x':
                led.opt.exit_mode = LED_EXIT_VAL;
                break;
            case 'n':
                led.opt.invert_selected = true;
                break;
            case 'm':
                led.opt.output_match = true;
                break;
            case 'p':
                led.opt.pack_selected = true;
                break;
            case 's':
                led.opt.output_selected = true;
                break;
            case 'e':
                led.opt.filter_blank = true;
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
                led_u8s_init_str(&led.opt.file_out_path, optstr);
                led_debug("Option path: %s", led_u8s_str(&led.opt.file_out_path));
                opti = arg->len;
                break;
            case 'A':
                led_assert(!led.opt.file_out, LED_ERR_ARG, "Bad option -%c, output file mode already set", opt);
                led_assert(!led.opt.exec, LED_ERR_ARG, "Bad option -%c, exec mode already set", opt);
                led.opt.file_out = LED_OUTPUT_FILE_APPEND;
                led_u8s_init(&led.opt.file_out_path, optstr, 0);
                led_debug("Option path: %s", led_u8s_str(&led.opt.file_out_path));
                opti = arg->len;
                break;
            case 'E':
                led_assert(!led.opt.file_out, LED_ERR_ARG, "Bad option -%c, output file mode already set", opt);
                led_assert(!led.opt.exec, LED_ERR_ARG, "Bad option -%c, exec mode already set", opt);
                led.opt.file_out = LED_OUTPUT_FILE_NEWEXT;
                led.opt.file_out_extn = atoi(optstr);
                if (led.opt.file_out_extn <= 0)
                    led_u8s_init(&led.opt.file_out_ext, optstr, 0);
                led_debug("Option ext: %s", led_u8s_str(&led.opt.file_out_ext));
                opti =arg->len;
                break;
            case 'D':
                led_assert(!led.opt.file_out, LED_ERR_ARG, "Bad option -%c, output file mode already set", opt);
                led_assert(!led.opt.exec, LED_ERR_ARG, "Bad option -%c, exec mode already set", opt);
                led_u8s_init(&led.opt.file_out_dir, optstr, 0);
                led.opt.file_out = LED_OUTPUT_FILE_DIR;
                led_debug("Option dir: %s", led_u8s_str(&led.opt.file_out_dir));
                opti = arg->len;
                break;
            case 'U':
                led.opt.file_out_unchanged = true;
                break;
            case 'X':
                led_assert(!led.opt.file_out, LED_ERR_ARG, "Bad option -%c, output file mode already set", opt);
                led.opt.exec = true;
                break;
            default:
                led_assert(false, LED_ERR_ARG, "Unknown option: -%c", opt);
            }
        }
    }
    return rc;
}

bool led_init_func(led_u8s_t* arg) {
    bool is_func = false;
    char fsep ='\0';
    if ( (is_func = led_u8s_match(arg, LED_REGEX_FUNC)) ) fsep = '/';
    else if ( (is_func = led_u8s_match(arg, LED_REGEX_FUNC2)) ) fsep = ':';

    if (is_func) {
        // check if additional func can be defined
        led_assert(led.func_count < LED_FUNC_MAX, LED_ERR_ARG, "Maximum functions reached %d", LED_FUNC_MAX );

        // search for function
        led_u8s_t fname;
        led_u8s_cut_next(arg, fsep, &fname);
        size_t ifunc = led.func_count++;
        led_fn_t* pfunc = &led.func_list[ifunc];
        led_debug("Funcion table max: %d", led_fn_table_size());
        for (pfunc->id = 0; pfunc->id < led_fn_table_size(); pfunc->id++) {
            led_fn_desc_t* pfn_desc = led_fn_table_descriptor(pfunc->id);
            if (led_u8s_equal_str(&fname, pfn_desc->short_name) || led_u8s_equal_str(&fname, pfn_desc->long_name)) {
                led_debug("Function found: %d", pfunc->id);
                break;
            }
        }

        // check if func is usable
        led_assert(pfunc->id < led_fn_table_size(), LED_ERR_ARG, "Unknown function: %s", led_u8s_str(&fname));
        led_assert(led_fn_table_descriptor(pfunc->id)->impl != NULL, LED_ERR_ARG, "Function not yet implemented in: %s", led_fn_table_descriptor(pfunc->id)->long_name);

        // compile zone regex if given
        led_u8s_t regx;
        led_u8s_cut_next(arg, fsep, &regx);
        if (!led_u8s_isempty(&regx)) {
            led_debug("Regex found: %s", led_u8s_str(&regx));
            pfunc->regex = led_u8s_regex_compile(&regx);
        }
        else {
            led_debug("Regex NOT found, fixed to the whole line");
            pfunc->regex = LED_REGEX_ALL_LINE;
        }

        // store func arguments
        while(!led_u8s_isempty(arg)) {
            // check if additional func arg can be defined
            led_assert(pfunc->arg_count < LED_FARG_MAX, LED_ERR_ARG, "Maximum function argments reached %d", LED_FARG_MAX );
            led_u8s_t* farg = &(pfunc->arg[pfunc->arg_count++].lstr);
            led_u8s_cut_next(arg, fsep, farg);
            led_debug("Function argument found: %s", led_u8s_str(farg));
        }
    }
    return is_func;
}

bool led_init_sel(led_u8s_t* arg) {
    bool rc = true;
    if (led_u8s_match_pat(arg, "^\\+[0-9]+$") && led.sel.type_start == SEL_TYPE_REGEX) {
        led.sel.val_start = strtol(arg->str, NULL, 10);
        led_debug("Selector start: shift after regex (%d)", led.sel.val_start);
    }
    else if (!led.sel.type_start) {
        if (led_u8s_match(arg, LED_REGEX_INTEGER)) {
            led.sel.type_start = SEL_TYPE_COUNT;
            led.sel.val_start = strtol(arg->str, NULL, 10);
            led_debug("Selector start: type number (%d)", led.sel.val_start);
        }
        else {
            led.sel.type_start = SEL_TYPE_REGEX;
            led.sel.regex_start = led_u8s_regex_compile(arg);
            led_debug("Selector start: type regex (%s)", led_u8s_str(arg));
        }
    }
    else if (!led.sel.type_stop) {
        if (led_u8s_match(arg, LED_REGEX_INTEGER)) {
            led.sel.type_stop = SEL_TYPE_COUNT;
            led.sel.val_stop = strtol(arg->str, NULL, 10);
            led_debug("Selector stop: type number (%d)", led.sel.val_stop);
        }
        else {
            led.sel.type_stop = SEL_TYPE_REGEX;
            led.sel.regex_stop = led_u8s_regex_compile(arg);
            led_debug("Selector stop: type regex (%s)", led_u8s_str(arg));
        }
    }
    else rc = false;

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
                led_assert(led_u8s_isinit(&pfunc->arg[i].lstr), LED_ERR_ARG, "function arg %i: missing regex\n%s", i+1, pfn_desc->help_format);
                pfunc->arg[i].regex = led_u8s_regex_compile(&pfunc->arg[i].lstr);
                led_debug("function arg %i: regex found", i+1);
            }
            else if (format[i] == 'r') {
                if (led_u8s_isinit(&pfunc->arg[i].lstr)) {
                    pfunc->arg[i].regex = led_u8s_regex_compile(&pfunc->arg[i].lstr);
                    led_debug("function arg %i: regex found", i+1);
                }
            }
            else if (format[i] == 'N') {
                led_assert(led_u8s_isinit(&pfunc->arg[i].lstr), LED_ERR_ARG, "function arg %i: missing number\n%s", i+1, pfn_desc->help_format);
                pfunc->arg[i].val = atol(led_u8s_str(&pfunc->arg[i].lstr));
                led_debug("function arg %i: numeric found: %li", i+1, pfunc->arg[i].val);
                // additionally compute the positive unsigned value to help
                pfunc->arg[i].uval = pfunc->arg[i].val < 0 ? (size_t)(-pfunc->arg[i].val) : (size_t)pfunc->arg[i].val;
            }
            else if (format[i] == 'n') {
                if (led_u8s_isinit(&pfunc->arg[i].lstr)) {
                    pfunc->arg[i].val = atol(led_u8s_str(&pfunc->arg[i].lstr));
                    led_debug("function arg %i: numeric found: %li", i+1, pfunc->arg[i].val);
                    // additionally compute the positive unsigned value to help
                    pfunc->arg[i].uval = pfunc->arg[i].val < 0 ? (size_t)(-pfunc->arg[i].val) : (size_t)pfunc->arg[i].val;
                }
            }
            else if (format[i] == 'P') {
                led_assert(led_u8s_isinit(&pfunc->arg[i].lstr), LED_ERR_ARG, "function arg %i: missing number\n%s", i+1, pfn_desc->help_format);
                pfunc->arg[i].val = atol(led_u8s_str(&pfunc->arg[i].lstr));
                led_assert(pfunc->arg[i].val >= 0, LED_ERR_ARG, "function arg %i: not a positive number\n%s", i+1, pfn_desc->help_format);
                pfunc->arg[i].uval = (size_t)pfunc->arg[i].val;
                led_debug("function arg %i: positive numeric found: %lu", i+1, pfunc->arg[i].uval);
            }
            else if (format[i] == 'p') {
                if (led_u8s_isinit(&pfunc->arg[i].lstr)) {
                    pfunc->arg[i].val = atol(led_u8s_str(&pfunc->arg[i].lstr));
                    led_assert(pfunc->arg[i].val >= 0, LED_ERR_ARG, "function arg %i: not a positive number\n%s", i+1, pfn_desc->help_format);
                    pfunc->arg[i].uval = (size_t)pfunc->arg[i].val;
                    led_debug("function arg %i: positive numeric found: %lu", i+1, pfunc->arg[i].uval);
                }
            }
            else if (format[i] == 'S') {
                led_assert(led_u8s_isinit(&pfunc->arg[i].lstr), LED_ERR_ARG, "function arg %i: missing string\n%s", i+1, pfn_desc->help_format);
                led_debug("function arg %i: string found: %s", i+1, led_u8s_str(&pfunc->arg[i].lstr));
            }
            else if (format[i] == 's') {
                if (led_u8s_isinit(&pfunc->arg[i].lstr)) {
                    led_debug("function arg %i: string found: %s", i+1, led_u8s_str(&pfunc->arg[i].lstr));
                }
            }
            else {
                led_assert(true, LED_ERR_ARG, "function arg %i: bad internal format (%s)", i+1, format);
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

    if (argc <= 1) led.opt.help = true;

    int arg_section = 0;
    for (int argi=1; argi < argc; argi++) {
        led_u8s_decl_str(arg, argv[argi]);

        if (arg_section == ARGS_SEC_FILES) {
            led.file_names = argv + argi;
            led.file_count = argc - argi;
            led_debug("Arg is file: %s", led_u8s_str(&arg));
        }
        else if (arg_section < ARGS_SEC_FILES && led_init_opt(&arg)) {
            if (led.opt.file_in) arg_section = ARGS_SEC_FILES;
            led_debug("Arg is opt: %s", led_u8s_str(&arg));
        }
        else if (arg_section <= ARGS_SEC_FUNCT && led_init_func(&arg)) {
            arg_section = ARGS_SEC_FUNCT;
            led_debug("Arg is func: %s", led_u8s_str(&arg));
        }
        else if (arg_section == ARGS_SEC_SELECT && led_init_sel(&arg)) {
            led_debug("Arg is part of selector: %s", led_u8s_str(&arg));
        }
        else {
            led_assert(false, LED_ERR_ARG, "Unknown or wrong argument: %s (%s section)", led_u8s_str(&arg), LED_SEC_TABLE[arg_section]);
        }
    }

    // if a process function is not defined show only selected
    led.opt.output_selected = led.opt.output_selected || led.func_count == 0;

    // init led_u8s_t file names with their buffers.
    led_u8s_init_buf(&led.file_in.name, led.file_in.buf_name);
    led_u8s_init_buf(&led.file_out.name, led.file_out.buf_name);

    led_line_reset(&led.line_read);
    led_line_reset(&led.line_prep);
    led_line_reset(&led.line_write);
    for (size_t i=0; i<LED_REG_MAX; i++)
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
        led_u8s_cpy_chars(&led.file_in.name, led.file_names[0]);
        led.file_names++;
        led.file_count--;
        led_u8s_trim(&led.file_in.name);
        led_debug("open file from args: %s", led_u8s_str(&led.file_in.name));
        led.file_in.file = fopen(led_u8s_str(&led.file_in.name), "r");
        led_assert(led.file_in.file != NULL, LED_ERR_FILE, "File not found: %s", led_u8s_str(&led.file_in.name));
        led.report.file_in_count++;
    }
    else if (led.stdin_ispipe) {
        char buf_fname[LED_FNAME_MAX+1];
        char* fname = fgets(buf_fname, LED_FNAME_MAX, stdin);
        if (fname) {
            led_u8s_cpy_chars(&led.file_in.name, fname);
            led_u8s_trim(&led.file_in.name);
            led_debug("open file from stdin: [%s]", led_u8s_str(&led.file_in.name));
            led.file_in.file = fopen(led_u8s_str(&led.file_in.name), "r");
            led_assert(led.file_in.file != NULL, LED_ERR_FILE, "File not found: %s", led_u8s_str(&led.file_in.name));
            led.report.file_in_count++;
        }
    }
}

void led_file_close_in() {
    fclose(led.file_in.file);
    led.file_in.file = NULL;
    led_u8s_empty(&led.file_in.name);
}

void led_file_stdin() {
    if (led.file_in.file) {
        led_assert(led.file_in.file == stdin, LED_ERR_FILE, "File is not STDIN internal error: %s", led_u8s_str(&led.file_in.name));
        led.file_in.file = NULL;
        led_u8s_empty(&led.file_in.name);
    } else if (led.stdin_ispipe) {
        led.file_in.file = stdin;
        led_u8s_cpy_chars(&led.file_in.name, "STDIN");
    }
}

void led_file_open_out() {
    const char* mode = "";
    if (led.opt.file_out == LED_OUTPUT_FILE_INPLACE) {
        led_u8s_cpy(&led.file_out.name, &led.file_in.name);
        led_u8s_app_str(&led.file_out.name, ".part");
        mode = "w+";
    }
    else if (led.opt.file_out == LED_OUTPUT_FILE_WRITE) {
        led_u8s_cpy(&led.file_out.name, &led.opt.file_out_path);
        mode = "w+";
    }
    else if (led.opt.file_out == LED_OUTPUT_FILE_APPEND) {
        led_u8s_cpy(&led.file_out.name, &led.opt.file_out_path);
        mode = "a";
    }
    else if (led.opt.file_out == LED_OUTPUT_FILE_NEWEXT) {
        led_u8s_cpy(&led.file_out.name, &led.file_in.name);
        led_u8s_app(&led.file_out.name, &led.opt.file_out_ext);
        mode = "w+";
    }
    else if (led.opt.file_out == LED_OUTPUT_FILE_DIR) {
        led_u8s_decl(tmp, LED_FNAME_MAX+1);

        led_u8s_cpy(&led.file_out.name, &led.opt.file_out_dir);
        led_u8s_app_str(&led.file_out.name, "/");
        led_u8s_cpy(&tmp, &led.file_in.name);
        led_u8s_app(&led.file_out.name, led_u8s_basename(&tmp));
        mode = "w+";
    }
    led.file_out.file = fopen(led_u8s_str(&led.file_out.name), mode);
    led_assert(led.file_out.file != NULL, LED_ERR_FILE, "File open error: %s", led_u8s_str(&led.file_out.name));
    led.report.file_out_count++;
}

void led_file_close_out() {
    led_u8s_decl(tmp, LED_FNAME_MAX+1);

    fclose(led.file_out.file);
    led.file_out.file = NULL;
    if (led.opt.file_out == LED_OUTPUT_FILE_INPLACE) {
        led_u8s_cpy(&tmp, &led.file_out.name);
        led_u8s_trunk_end(&tmp, 5);
        led_debug("Rename: %s ==> %s", led_u8s_str(&led.file_out.name), led_u8s_str(&tmp));
        int syserr = remove(led_u8s_str(&tmp));
        led_assert(!syserr, LED_ERR_FILE, "File remove error: %d => %s", syserr, led_u8s_str(&tmp));
        rename(led_u8s_str(&led.file_out.name), led_u8s_str(&tmp));
        led_assert(!syserr, LED_ERR_FILE, "File rename error: %d => %s", syserr, led_u8s_str(&led.file_out.name));
        led_u8s_cpy(&led.file_out.name, &tmp);
    }
}

void led_file_print_out() {
    fwrite(led_u8s_str(&led.file_out.name), sizeof *led_u8s_str(&led.file_out.name), led_u8s_len(&led.file_out.name), stdout);
    fwrite("\n", sizeof *led_u8s_str(&led.file_out.name), 1, stdout);
    fflush(stdout);
    led_u8s_empty(&led.file_out.name);
}

void led_file_stdout() {
    led.file_out.file = stdout;
    led_u8s_cpy_chars(&led.file_out.name, "STDOUT");
}

bool led_file_next() {
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

    led_debug("Input from: %s", led_u8s_str(&led.file_in.name));
    led_debug("Output to: %s", led_u8s_str(&led.file_out.name));

    led.sel.total_count = 0;
    led.sel.count = 0;
    led.sel.selected = false;
    led.sel.inboundary = false;
    return led.file_in.file != NULL;
}

bool led_process_read() {
    led_debug("led_process_read");
    if (!led_line_isinit(&led.line_read)) {
        led_u8s_init(&led.line_read.lstr, fgets(led.line_read.buf, sizeof led.line_read.buf, led.file_in.file), sizeof led.line_read.buf);
        if (led_line_isinit(&led.line_read)) {
            led_u8s_trunk_char(&led.line_read.lstr, '\n');
            led.line_read.zone_start = 0;
            led.line_read.zone_stop = led.line_read.lstr.len;
            led.line_read.selected = false;
            led.sel.total_count++;
            led_debug("Read line: (%d) len=%d", led.sel.total_count, led.line_read.lstr.len);
        }
        else
            led_debug("Read line is NULL: (%d)", led.sel.total_count);
    }
    return led_line_isinit(&led.line_read);
}

void led_process_write() {
    led_debug("led_process_write");
    if (led_line_isinit(&led.line_write)) {
        led_debug("Write line: (%d) len=%d", led.sel.total_count, led_u8s_len(&led.line_write.lstr));
        led_u8s_app_char(&led.line_write.lstr, '\n');
        led_debug("Write line to %s", led_u8s_str(&led.file_out.name));
        fwrite(led_u8s_str(&led.line_write.lstr), sizeof *led_u8s_str(&led.line_write.lstr), led_u8s_len(&led.line_write.lstr), led.file_out.file);
        fflush(led.file_out.file);
    }
    led_line_reset(&led.line_write);
}

void led_process_exec() {
    led_debug("led_process_exec");
    if (led_line_isinit(&led.line_write) && !led_u8s_isblank(&led.line_write.lstr)) {
        led_debug("Exec line: (%d) len=%d", led.sel.total_count, led_u8s_len(&led.line_write.lstr));
        led_debug("Exec command %s", led_u8s_str(&led.line_write.lstr));

        FILE *fp = popen(led_u8s_str(&led.line_write.lstr), "r");
        led_assert(fp != NULL, LED_ERR_ARG, "Command error");
        led_u8s_decl(output, 4096);
        while (led_u8s_isinit(led_u8s_init(&output, fgets(led_u8s_str(&output), led_u8s_size(&output), fp), led_u8s_size(&output)))) {
            fwrite(led_u8s_str(&output), sizeof *led_u8s_str(&output), led_u8s_len(&output), led.file_out.file);
            fflush(led.file_out.file);
        }
        pclose(fp);
    }
    led_line_reset(&led.line_write);
}

bool led_process_selector() {
    led_debug("led_process_selector");

    bool ready = false;
    // stop selection on stop boundary
    if (!led_line_isinit(&led.line_read)
        || (led.sel.type_stop == SEL_TYPE_NONE && led.sel.type_start != SEL_TYPE_NONE && led.sel.shift == 0)
        || (led.sel.type_stop == SEL_TYPE_COUNT && led.sel.count >= led.sel.val_stop)
        || (led.sel.type_stop == SEL_TYPE_REGEX && led_u8s_match(&led.line_read.lstr, led.sel.regex_stop))
        ) {
        led.sel.inboundary = false;
        led.sel.count = 0;
    }
    if (led.sel.shift > 0) led.sel.shift--;

    // start selection on start boundary
    if (led_line_isinit(&led.line_read) && (
        led.sel.type_start == SEL_TYPE_NONE
        || (led.sel.type_start == SEL_TYPE_COUNT && led.sel.total_count == led.sel.val_start)
        || (led.sel.type_start == SEL_TYPE_REGEX && led_u8s_match(&led.line_read.lstr, led.sel.regex_start))
        )) {
        led.sel.inboundary = true;
        led.sel.shift = led.sel.val_start;
        led.sel.count = 0;
    }

    led.sel.selected = led.sel.inboundary && led.sel.shift == 0;
    led.line_read.selected = led.sel.selected == !led.opt.invert_selected;

    led_debug("Select: inboundary=%d, shift=%d selected=%d line selected=%d", led.sel.inboundary, led.sel.shift, led.sel.selected, led.line_read.selected);

    if (led.sel.selected) led.sel.count++;

    if (led.opt.pack_selected) {
        if (led_line_isselected(&led.line_read)) {
            led_debug("pack: append to ready");
            if (!(led.opt.filter_blank && led_u8s_isblank(&led.line_read.lstr))) {
                if (led_u8s_iscontent(&led.line_prep.lstr))
                    led_u8s_app_char(&led.line_prep.lstr, '\n');
                led_u8s_app(&led.line_prep.lstr, &led.line_read.lstr);
            }
            led_line_select(&led.line_prep, true);
            led_line_reset(&led.line_read);
        }
        else if (led_line_isselected(&led.line_prep)) {
            led_debug("pack: ready to process");
            ready = true;
        }
        else {
            led_debug("pack: no selection");
            if (!(led.opt.filter_blank && led_u8s_isblank(&led.line_read.lstr)))
                led_line_cpy(&led.line_prep, &led.line_read);
            led_line_reset(&led.line_read);
            ready = true;
        }
    }
    else {
        if (!(led.opt.filter_blank && led_u8s_isblank(&led.line_read.lstr)))
            led_line_cpy(&led.line_prep, &led.line_read);
        led_line_reset(&led.line_read);
        ready = true;
    }

    led_debug("Line ready to process: %d", ready);
    return ready;
}

void led_process_functions() {
    led_debug("led_process_functions");
    led_debug("Process line prep (isinit: %d len: %d)", led_line_isinit(&led.line_prep), led_u8s_len(&led.line_prep.lstr));
    if (led_line_isinit(&led.line_prep)) {
        led_debug("prep line is init");
        if (led_line_isselected(&led.line_prep)) {
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
                led_debug("No function copy (len: %d)", led_u8s_len(&led.line_prep.lstr));
                led_line_cpy(&led.line_write, &led.line_prep);
            }
        }
        else if (!led.opt.output_selected) {
            led_debug("Copy unselected to dest");
            led_line_cpy(&led.line_write, &led.line_prep);
        }
    }
    led_debug("Process result line write (len=%d)", led_u8s_len(&led.line_write.lstr));
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
