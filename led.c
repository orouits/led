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
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

bool led_init_opt(led_str_t* arg) {
    bool rc = led_str_match_pat(arg, "^-[a-zA-Z]+");
    if (rc) {
        led_debug("led_init_opt: arg option=%s", led_str_str(arg));
        led_str_foreach_uchar(arg) {
            const char* optstr = NULL;
            led_debug("led_init_opt: option=%lc sitcked=%s", foreach.uc, optstr);
            switch (foreach.uc) {
                case '-':
                    break;
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
                    led_assert(!led.opt.file_out, LED_ERR_ARG, "Bad option -%c, output file mode already set", foreach.uc);
                    led_assert(!led.opt.exec, LED_ERR_ARG, "Bad option -%c, exec mode already set", foreach.uc);
                    led.opt.file_out = LED_OUTPUT_FILE_INPLACE;
                    break;
                case 'W':
                    led_assert(!led.opt.file_out, LED_ERR_ARG, "Bad option -%c, output file mode already set", foreach.uc);
                    led_assert(!led.opt.exec, LED_ERR_ARG, "Bad option -%c, exec mode already set", foreach.uc);
                    led.opt.file_out = LED_OUTPUT_FILE_WRITE;
                    optstr = led_str_str_at(arg, foreach.i_next);
                    led_str_init_str(&led.opt.file_out_path, optstr);
                    led_debug("led_init_opt: path=%s", led_str_str(&led.opt.file_out_path));
                    break;
                case 'A':
                    led_assert(!led.opt.file_out, LED_ERR_ARG, "Bad option -%c, output file mode already set", foreach.uc);
                    led_assert(!led.opt.exec, LED_ERR_ARG, "Bad option -%c, exec mode already set", foreach.uc);
                    led.opt.file_out = LED_OUTPUT_FILE_APPEND;
                    optstr = led_str_str_at(arg, foreach.i_next);
                    led_str_init_str(&led.opt.file_out_path, optstr);
                    led_debug("led_init_opt: path=%s", led_str_str(&led.opt.file_out_path));
                    break;
                case 'E':
                    led_assert(!led.opt.file_out, LED_ERR_ARG, "Bad option -%c, output file mode already set", foreach.uc);
                    led_assert(!led.opt.exec, LED_ERR_ARG, "Bad option -%c, exec mode already set", foreach.uc);
                    led.opt.file_out = LED_OUTPUT_FILE_NEWEXT;
                    optstr = led_str_str_at(arg, foreach.i_next);
                    led.opt.file_out_extn = atoi(optstr);
                    if (led.opt.file_out_extn <= 0)
                        led_str_init_str(&led.opt.file_out_ext, optstr);
                    led_debug("led_init_opt: ext=%s", led_str_str(&led.opt.file_out_ext));
                    break;
                case 'D':
                    led_assert(!led.opt.file_out, LED_ERR_ARG, "Bad option -%c, output file mode already set", foreach.uc);
                    led_assert(!led.opt.exec, LED_ERR_ARG, "Bad option -%c, exec mode already set", foreach.uc);
                    led.opt.file_out = LED_OUTPUT_FILE_DIR;
                    optstr = led_str_str_at(arg, foreach.i_next);
                    led_str_init_str(&led.opt.file_out_dir, optstr);
                    led_debug("led_init_opt: dir=%s", led_str_str(&led.opt.file_out_dir));
                    break;
                case 'U':
                    led.opt.file_out_unchanged = true;
                    break;
                case 'X':
                    led_assert(!led.opt.file_out, LED_ERR_ARG, "Bad option -%c, output file mode already set", foreach.uc);
                    led.opt.exec = true;
                    break;
                default:
                    led_assert(false, LED_ERR_ARG, "Unknown option: -%c", foreach.uc);
            }
            if (optstr) break; // the option required a value on following characters, we have to stop to scan for agregated options.
        }
    }
    return rc;
}

bool led_init_func(led_str_t* arg) {
    bool is_func = false;
    char fsep ='\0';
    if ( (is_func = led_str_match(arg, LED_REGEX_FUNC)) ) fsep = '/';
    else if ( (is_func = led_str_match(arg, LED_REGEX_FUNC2)) ) fsep = ':';

    if (is_func) {
        // check if additional func can be defined
        led_assert(led.func_count < LED_FUNC_MAX, LED_ERR_ARG, "Maximum functions reached %d", LED_FUNC_MAX );

        // get the function name
        led_str_t fname;
        led_str_cut_next(arg, fsep, &fname);

        // find the function id
        size_t ifunc = led.func_count++;
        led_fn_t* pfunc = &led.func_list[ifunc];
        pfunc->id = led_fn_table_size();
        led_debug("led_init_func: table size=%d", led_fn_table_size());
        led_foreach_pval_len(led_fn_table_descriptor(0), led_fn_table_size())
            if (led_str_equal_str(&fname, foreach.pval->short_name) || led_str_equal_str(&fname, foreach.pval->long_name)) {
                led_debug("led_init_func: function found=%d", foreach.i);
                pfunc->id = foreach.i;
                break;
            }
        // check if func is usable
        led_assert(pfunc->id < led_fn_table_size(), LED_ERR_ARG, "Unknown function: %s", led_str_str(&fname));
        led_assert(led_fn_table_descriptor(pfunc->id)->impl != NULL, LED_ERR_ARG, "Function not yet implemented in: %s", led_fn_table_descriptor(pfunc->id)->long_name);

        // compile zone regex if given
        led_str_t regx;
        led_str_cut_next(arg, fsep, &regx);
        if (!led_str_isempty(&regx)) {
            led_debug("led_init_func: regex found=%s", led_str_str(&regx));
            pfunc->regex = led_str_regex_compile(&regx, led.opt.pack_selected ? PCRE2_MULTILINE: 0);
        }
        else {
            led_debug("led_init_func: regex NOT found, no zone selection");
            pfunc->regex = NULL;
        }

        // store func arguments
        while (!led_str_isempty(arg)) {
            // check if additional func arg can be defined
            led_assert(pfunc->arg_count < LED_FARG_MAX, LED_ERR_ARG, "Maximum function argments reached %d", LED_FARG_MAX );
            led_str_t* farg = &(pfunc->arg[pfunc->arg_count++].lstr);
            led_str_cut_next(arg, fsep, farg);
            led_debug("led_init_func: function argument found=%s", led_str_str(farg));
        }
    }
    return is_func;
}

bool led_init_sel(led_str_t* arg) {
    led_debug("led_init_sel: %s", led_str_str(arg));
    bool rc = true;
    if (led_str_match_pat(arg, "^\\+[0-9]+$") && led.sel.type_start == SEL_TYPE_REGEX) {
        led.sel.val_start = strtol(arg->str, NULL, 10);
        led_debug("led_init_sel: selector start: shift after regex=%d", led.sel.val_start);
    }
    else if (!led.sel.type_start) {
        if (led_str_match(arg, LED_REGEX_INTEGER)) {
            led.sel.type_start = SEL_TYPE_COUNT;
            led.sel.val_start = strtol(arg->str, NULL, 10);
            led_debug("led_init_sel: selector start: type number=%d", led.sel.val_start);
        }
        else {
            led.sel.type_start = SEL_TYPE_REGEX;
            led.sel.regex_start = led_str_regex_compile(arg,0);
            led_debug("led_init_sel: selector start: type regex=%s", led_str_str(arg));
        }
    }
    else if (!led.sel.type_stop) {
        if (led_str_match(arg, LED_REGEX_INTEGER)) {
            led.sel.type_stop = SEL_TYPE_COUNT;
            led.sel.val_stop = strtol(arg->str, NULL, 10);
            led_debug("led_init_sel: selector stop: type number=%d", led.sel.val_stop);
        }
        else {
            led.sel.type_stop = SEL_TYPE_REGEX;
            led.sel.regex_stop = led_str_regex_compile(arg,0);
            led_debug("led_init_sel: selector stop: type regex=%s", led_str_str(arg));
        }
    }
    else rc = false;

    return rc;
}

void led_init_config() {
    led_foreach_pval_len(led.func_list, led.func_count) {
        led_fn_t* pfunc = foreach.pval;

        led_fn_desc_t* pfn_desc = led_fn_table_descriptor(pfunc->id);
        led_debug("led_init_config: configure function=%s id=%d", pfn_desc->long_name, pfunc->id);

        led_foreach_char(pfn_desc->args_fmt) {
            led_assert(foreach.i < LED_FARG_MAX, LED_ERR_ARG, "function arg %i exceed max defined %i\n%s", foreach.i, LED_FARG_MAX, pfn_desc->help_format);
            if (foreach.c == 'N') {
                led_assert(led_str_isinit(&pfunc->arg[foreach.i].lstr), LED_ERR_ARG, "function arg %i: missing number\n%s", foreach.i+1, pfn_desc->help_format);
                pfunc->arg[foreach.i].val = atol(led_str_str(&pfunc->arg[foreach.i].lstr));
                led_debug("led_init_config: function arg=%i numeric found=%li", foreach.i+1, pfunc->arg[foreach.i].val);
                // additionally compute the positive unsigned value to help
                pfunc->arg[foreach.i].uval = pfunc->arg[foreach.i].val < 0 ? (size_t)(-pfunc->arg[foreach.i].val) : (size_t)pfunc->arg[foreach.i].val;
            }
            else if (foreach.c == 'n') {
                if (led_str_isinit(&pfunc->arg[foreach.i].lstr)) {
                    pfunc->arg[foreach.i].val = atol(led_str_str(&pfunc->arg[foreach.i].lstr));
                    led_debug("led_init_config: function arg=%i: numeric found=%li", foreach.i+1, pfunc->arg[foreach.i].val);
                    // additionally compute the positive unsigned value to help
                    pfunc->arg[foreach.i].uval = pfunc->arg[foreach.i].val < 0 ? (size_t)(-pfunc->arg[foreach.i].val) : (size_t)pfunc->arg[foreach.i].val;
                }
            }
            else if (foreach.c == 'P') {
                led_assert(led_str_isinit(&pfunc->arg[foreach.i].lstr), LED_ERR_ARG, "function arg %i: missing number\n%s", foreach.i+1, pfn_desc->help_format);
                pfunc->arg[foreach.i].val = atol(led_str_str(&pfunc->arg[foreach.i].lstr));
                led_assert(pfunc->arg[foreach.i].val >= 0, LED_ERR_ARG, "function arg %i: not a positive number\n%s", foreach.i+1, pfn_desc->help_format);
                pfunc->arg[foreach.i].uval = (size_t)pfunc->arg[foreach.i].val;
                led_debug("led_init_config: function arg=%i positive numeric found=%lu", foreach.i+1, pfunc->arg[foreach.i].uval);
            }
            else if (foreach.c == 'p') {
                if (led_str_isinit(&pfunc->arg[foreach.i].lstr)) {
                    pfunc->arg[foreach.i].val = atol(led_str_str(&pfunc->arg[foreach.i].lstr));
                    led_assert(pfunc->arg[foreach.i].val >= 0, LED_ERR_ARG, "function arg %i: not a positive number\n%s", foreach.i+1, pfn_desc->help_format);
                    pfunc->arg[foreach.i].uval = (size_t)pfunc->arg[foreach.i].val;
                    led_debug("led_init_config: function arg=%i positive numeric found=%lu", foreach.i+1, pfunc->arg[foreach.i].uval);
                }
            }
            else if (foreach.c == 'S') {
                led_assert(led_str_isinit(&pfunc->arg[foreach.i].lstr), LED_ERR_ARG, "function arg %i: missing string\n%s", foreach.i+1, pfn_desc->help_format);
                led_debug("led_init_config: function arg=%i string found=%s", foreach.i+1, led_str_str(&pfunc->arg[foreach.i].lstr));
            }
            else if (foreach.c == 's') {
                if (led_str_isinit(&pfunc->arg[foreach.i].lstr)) {
                    led_debug("led_init_config: function arg=%i string found=%s", foreach.i+1, led_str_str(&pfunc->arg[foreach.i].lstr));
                }
            }
            else {
                led_assert(true, LED_ERR_ARG, "function arg %i: bad internal format (%s)", foreach.i+1, pfn_desc->args_fmt);
            }
        }
    }
}

void led_init(size_t argc, char* argv[]) {
    static const char* LED_SEC_TABLE[] = {
        "selector",
        "function",
        "files",
    };

    setlocale(LC_ALL, "");
    led_debug("led_init:");

    led_regex_init();

    memset(&led, 0, sizeof(led));

    led.stdin_ispipe = !isatty(fileno(stdin));
    led.stdout_ispipe = !isatty(fileno(stdout));

    if (argc <= 1) led.opt.help = true;

    int arg_section = 0;
    led_foreach_val_len(argv, argc) {
        if (foreach.i == 0) continue; // bypass arg 0 as led command.
        led_str_decl_str(arg, foreach.val);

        if (arg_section == ARGS_SEC_FILES) {
            led.file_names = argv + foreach.i;
            led.file_count = argc - foreach.i;
            led_debug("led_init: arg is file names with count=%lu", led.file_count);
            break;
        }
        else if (arg_section < ARGS_SEC_FILES && led_init_opt(&arg)) {
            if (led.opt.file_in) arg_section = ARGS_SEC_FILES;
            led_debug("led_init: arg is opt=%s", led_str_str(&arg));
        }
        else if (arg_section <= ARGS_SEC_FUNCT && led_init_func(&arg)) {
            arg_section = ARGS_SEC_FUNCT;
            led_debug("led_init: arg is func=%s", led_str_str(&arg));
        }
        else if (arg_section == ARGS_SEC_SELECT && led_init_sel(&arg)) {
            led_debug("led_init: arg is part of selector=%s", led_str_str(&arg));
        }
        else {
            led_assert(false, LED_ERR_ARG, "Unknown or wrong argument: %s (%s section)", led_str_str(&arg), LED_SEC_TABLE[arg_section]);
        }
    }

    // if a process function is not defined show only selected
    led.opt.output_selected = led.opt.output_selected || led.func_count == 0;

    // init led_str_t file names with their buffers.
    led_str_init_buf(&led.file_in.name, led.file_in.buf_name);
    led_str_init_buf(&led.file_out.name, led.file_out.buf_name);

    led_line_reset(&led.line_read);
    led_line_reset(&led.line_prep);
    led_line_reset(&led.line_write);
    led_foreach_int(LED_REG_MAX)
        led_line_reset(&led.line_reg[foreach.i]);

        // pre-configure the processor command
    led_init_config();

    led_debug("led_init: config sel.count=%d", led.sel.count);
    led_debug("led_init: config func count=%d", led.func_count);

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
## Processor functions:\n\n\
"
   );
    fprintf(stderr, "|%.5s|%.20s|%.10s|%.50s|%.40s|\n", DASHS, DASHS, DASHS, DASHS, DASHS);
    fprintf(stderr, "| %-4s| %-19s| %-9s| %-49s| %-39s|\n", "Id", "Name", "Short", "Description", "Format");
    fprintf(stderr, "|%.5s|%.20s|%.10s|%.50s|%.40s|\n", DASHS, DASHS, DASHS, DASHS, DASHS);
    led_foreach_pval_len(led_fn_table_descriptor(0), led_fn_table_size()) {
        if (!foreach.pval->impl) fprintf(stderr, "\e[90m");
        fprintf(stderr, "| %-4lu| %-19s| %-9s| %-49s| %-39s|\n",
            foreach.i,
            foreach.pval->long_name,
            foreach.pval->short_name,
            foreach.pval->help_desc,
            foreach.pval->help_format
       );
        if (!foreach.pval->impl) fprintf(stderr, "\e[0m");
    }
    fprintf(stderr, "|%.5s|%.20s|%.10s|%.50s|%.40s|\n", DASHS, DASHS, DASHS, DASHS, DASHS);
}

//-----------------------------------------------
// LED process functions
//-----------------------------------------------

void led_file_open_in() {
    led_debug("led_file_open_in: ");
    if (led.file_count) {
        led_str_cpy_str(&led.file_in.name, led.file_names[0]);
        led.file_names++;
        led.file_count--;
        led_str_trim(&led.file_in.name);
        led_debug("led_file_open_in: open file from args= %s", led_str_str(&led.file_in.name));
        led.file_in.file = fopen(led_str_str(&led.file_in.name), "r");
        led_assert(led.file_in.file != NULL, LED_ERR_FILE, "File not found: %s", led_str_str(&led.file_in.name));
        led.report.file_in_count++;
    }
    else if (led.stdin_ispipe) {
        char buf_fname[LED_FNAME_MAX+1];
        char* fname = fgets(buf_fname, LED_FNAME_MAX, stdin);
        if (fname) {
            led_str_cpy_str(&led.file_in.name, fname);
            led_str_trim(&led.file_in.name);
            led_debug("led_file_open_in: open file from stdin=%s", led_str_str(&led.file_in.name));
            led.file_in.file = fopen(led_str_str(&led.file_in.name), "r");
            led_assert(led.file_in.file != NULL, LED_ERR_FILE, "File not found: %s", led_str_str(&led.file_in.name));
            led.report.file_in_count++;
        }
    }
}

void led_file_close_in() {
    fclose(led.file_in.file);
    led.file_in.file = NULL;
    led_str_empty(&led.file_in.name);
}

void led_file_stdin() {
    if (led.file_in.file) {
        led_assert(led.file_in.file == stdin, LED_ERR_FILE, "File is not STDIN internal error: %s", led_str_str(&led.file_in.name));
        led.file_in.file = NULL;
        led_str_empty(&led.file_in.name);
    } else if (led.stdin_ispipe) {
        led.file_in.file = stdin;
        led_str_cpy_str(&led.file_in.name, "STDIN");
    }
}

void led_file_open_out() {
    const char* mode = "";
    if (led.opt.file_out == LED_OUTPUT_FILE_INPLACE) {
        led_str_cpy(&led.file_out.name, &led.file_in.name);
        led_str_app_str(&led.file_out.name, ".part");
        mode = "w+";
    }
    else if (led.opt.file_out == LED_OUTPUT_FILE_WRITE) {
        led_str_cpy(&led.file_out.name, &led.opt.file_out_path);
        mode = "w+";
    }
    else if (led.opt.file_out == LED_OUTPUT_FILE_APPEND) {
        led_str_cpy(&led.file_out.name, &led.opt.file_out_path);
        mode = "a";
    }
    else if (led.opt.file_out == LED_OUTPUT_FILE_NEWEXT) {
        led_str_cpy(&led.file_out.name, &led.file_in.name);
        led_str_app(&led.file_out.name, &led.opt.file_out_ext);
        mode = "w+";
    }
    else if (led.opt.file_out == LED_OUTPUT_FILE_DIR) {
        led_str_decl(tmp, LED_FNAME_MAX+1);

        led_str_cpy(&led.file_out.name, &led.opt.file_out_dir);
        led_str_app_str(&led.file_out.name, "/");
        led_str_cpy(&tmp, &led.file_in.name);
        led_str_app(&led.file_out.name, led_str_basename(&tmp));
        mode = "w+";
    }
    led.file_out.file = fopen(led_str_str(&led.file_out.name), mode);
    led_assert(led.file_out.file != NULL, LED_ERR_FILE, "File open error: %s", led_str_str(&led.file_out.name));
    led.report.file_out_count++;
}

void led_file_close_out() {
    led_str_decl(tmp, LED_FNAME_MAX+1);

    fclose(led.file_out.file);
    led.file_out.file = NULL;
    if (led.opt.file_out == LED_OUTPUT_FILE_INPLACE) {
        led_str_cpy(&tmp, &led.file_out.name);
        led_str_trunk_end(&tmp, 5);
        led_debug("led_file_close_out: rename=%s to=%s", led_str_str(&led.file_out.name), led_str_str(&tmp));
        int syserr = remove(led_str_str(&tmp));
        led_assert(!syserr, LED_ERR_FILE, "File remove error: %d => %s", syserr, led_str_str(&tmp));
        rename(led_str_str(&led.file_out.name), led_str_str(&tmp));
        led_assert(!syserr, LED_ERR_FILE, "File rename error: %d => %s", syserr, led_str_str(&led.file_out.name));
        led_str_cpy(&led.file_out.name, &tmp);
    }
}

void led_file_print_out() {
    fwrite(led_str_str(&led.file_out.name), sizeof *led_str_str(&led.file_out.name), led_str_len(&led.file_out.name), stdout);
    fwrite("\n", sizeof *led_str_str(&led.file_out.name), 1, stdout);
    fflush(stdout);
    led_str_empty(&led.file_out.name);
}

void led_file_stdout() {
    led.file_out.file = stdout;
    led_str_cpy_str(&led.file_out.name, "STDOUT");
}

bool led_file_next() {
    led_debug("led_file_next: ---------------------------------------------------");

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

    led_debug("led_file_next: input from=%s", led_str_str(&led.file_in.name));
    led_debug("led_file_next: output to=%s", led_str_str(&led.file_out.name));

    led.sel.total_count = 0;
    led.sel.count = 0;
    led.sel.selected = false;
    led.sel.inboundary = false;
    return led.file_in.file != NULL;
}

bool led_process_read() {
    led_debug("led_process_read: ");
    if (!led_line_isinit(&led.line_read)) {
        led_str_init(&led.line_read.lstr, fgets(led.line_read.buf, sizeof led.line_read.buf, led.file_in.file), sizeof led.line_read.buf);
        if (led_line_isinit(&led.line_read)) {
            led_str_trunk_uchar(&led.line_read.lstr, '\n');
            led.line_read.zone_start = 0;
            led.line_read.zone_stop = led.line_read.lstr.len;
            led.line_read.selected = false;
            led.sel.total_count++;
            led.report.line_read_count++;
            led_debug("led_process_read: read line num=%d len=%d", led.sel.total_count, led.line_read.lstr.len);
        }
        else
            led_debug("led_process_read: read line num=%d is NULL", led.sel.total_count);
    }
    return led_line_isinit(&led.line_read);
}

void led_process_write() {
    led_debug("led_process_write: ");
    if (led_line_isinit(&led.line_write)) {
        led_debug("led_process_write: write line num=%d len=%d", led.sel.total_count, led_str_len(&led.line_write.lstr));
        led_str_app_uchar(&led.line_write.lstr, '\n');
        led_debug("led_process_write: write line to file=%s", led_str_str(&led.file_out.name));
        fwrite(led_str_str(&led.line_write.lstr), sizeof *led_str_str(&led.line_write.lstr), led_str_len(&led.line_write.lstr), led.file_out.file);
        fflush(led.file_out.file);
        led.report.line_write_count++;
    }
    led_line_reset(&led.line_write);
}

void led_process_exec() {
    led_debug("led_process_exec: ");
    if (led_line_isinit(&led.line_write) && !led_str_isblank(&led.line_write.lstr)) {
        led_debug("led_process_exec: exec line num=%d len=%d command=%s", led.sel.total_count, led_str_len(&led.line_write.lstr), led_str_str(&led.line_write.lstr));
        FILE *fp = popen(led_str_str(&led.line_write.lstr), "r");
        led_assert(fp != NULL, LED_ERR_ARG, "Command error");
        led_str_decl(output, 4096);
        while (led_str_isinit(led_str_init(&output, fgets(led_str_str(&output), led_str_size(&output), fp), led_str_size(&output)))) {
            fwrite(led_str_str(&output), sizeof *led_str_str(&output), led_str_len(&output), led.file_out.file);
            fflush(led.file_out.file);
        }
        pclose(fp);
    }
    led_line_reset(&led.line_write);
}

bool led_process_selector() {
    led_debug("led_process_selector: led.sel.type_start=%d %s", led.sel.type_start, led_str_str(&led.line_read.lstr));

    bool ready = false;
    // stop selection on stop boundary
    if (!led_line_isinit(&led.line_read)
        || (led.sel.type_stop == SEL_TYPE_NONE && led.sel.type_start != SEL_TYPE_NONE && led.sel.shift == 0)
        || (led.sel.type_stop == SEL_TYPE_COUNT && led.sel.count >= led.sel.val_stop)
        || (led.sel.type_stop == SEL_TYPE_REGEX && led_str_match(&led.line_read.lstr, led.sel.regex_stop))
        ) {
        led.sel.inboundary = false;
        led.sel.count = 0;
        led_debug("led_process_selector: stop selection");
    }
    if (led.sel.shift > 0) led.sel.shift--;

    // start selection on start boundary
    if (led_line_isinit(&led.line_read) && (
        led.sel.type_start == SEL_TYPE_NONE
        || (led.sel.type_start == SEL_TYPE_COUNT && led.sel.total_count == led.sel.val_start)
        || (led.sel.type_start == SEL_TYPE_REGEX && led_str_match(&led.line_read.lstr, led.sel.regex_start))
        )) {
        led.sel.inboundary = true;
        led.sel.shift = led.sel.val_start;
        led.sel.count = 0;
        led_debug("led_process_selector: start selection");
    }

    led.sel.selected = led.sel.inboundary && led.sel.shift == 0;
    led.line_read.selected = led.sel.selected == !led.opt.invert_selected;

    led_debug("led_process_selector: select inboundary=%d, shift=%d selected=%d line selected=%d", led.sel.inboundary, led.sel.shift, led.sel.selected, led.line_read.selected);

    if (led.sel.selected) led.sel.count++;

    if (led.opt.pack_selected) {
        if (led_line_isselected(&led.line_read)) {
            led_debug("led_process_selector: pack: append to ready");
            if (!(led.opt.filter_blank && led_str_isblank(&led.line_read.lstr))) {
                if (led_line_isinit(&led.line_prep)) {
                    if (led_str_iscontent(&led.line_prep.lstr))
                        led_str_app_uchar(&led.line_prep.lstr, '\n');
                    led_str_app(&led.line_prep.lstr, &led.line_read.lstr);
                }
                else
                    led_line_cpy(&led.line_prep, &led.line_read);
            }
            led_line_select(&led.line_prep, true);
            led_line_reset(&led.line_read);
        }
        else if (led_line_isselected(&led.line_prep)) {
            led_debug("led_process_selector: pack: ready to process");
            ready = true;
        }
        else {
            led_debug("led_process_selector: pack: no selection");
            if (!(led.opt.filter_blank && led_str_isblank(&led.line_read.lstr)))
                led_line_cpy(&led.line_prep, &led.line_read);
            led_line_reset(&led.line_read);
            ready = true;
        }
    }
    else {
        if (!(led.opt.filter_blank && led_str_isblank(&led.line_read.lstr)))
            led_line_cpy(&led.line_prep, &led.line_read);
        led_line_reset(&led.line_read);
        ready = true;
    }

    led_debug("led_process_selector: line ready to process=%d", ready);
    return ready;
}

void led_process_functions() {
    led_debug("led_process_functions: Process line prep isinit=%d len=%d", led_line_isinit(&led.line_prep), led_str_len(&led.line_prep.lstr));
    if (led_line_isinit(&led.line_prep)) {
        led_debug("led_process_functions: prep line is init");
        if (led_line_isselected(&led.line_prep)) {
            led_debug("led_process_functions: prep line is selected");
            if (led.func_count > 0) {
                led_foreach_pval_len(led.func_list, led.func_count) {
                    led_fn_t* pfunc = foreach.pval;
                    led_fn_desc_t* pfn_desc = led_fn_table_descriptor(pfunc->id);
                    led.report.line_match_count++;
                    led_debug("led_process_functions: call=%s", pfn_desc->long_name);
                    (pfn_desc->impl)(pfunc);
                    led_line_cpy(&led.line_prep, &led.line_write);
                    led_debug("led_process_functions: result=\n%s", led_str_str(&led.line_write.lstr));
                }
            }
            else {
                led_debug("led_process_functions: no function, copy len=%d", led_str_len(&led.line_prep.lstr));
                led_line_cpy(&led.line_write, &led.line_prep);
            }
        }
        else if (!led.opt.output_selected) {
            led_debug("led_process_functions: copy unselected to dest");
            led_line_cpy(&led.line_write, &led.line_prep);
        }
    }
    led_debug("led_process_functions: result len=%d line=\n%s", led_str_len(&led.line_write.lstr), led_str_str(&led.line_write.lstr));
    led_line_reset(&led.line_prep);
}

void led_report() {
    fprintf(stderr, "\n-- LED report --\n");
    fprintf(stderr, "line_read_count:\t%ld\n", led.report.line_read_count);
    fprintf(stderr, "line_match_count:\t%ld\n", led.report.line_match_count);
    fprintf(stderr, "line_write_count:\t%ld\n", led.report.line_write_count);
    fprintf(stderr, "file_input_count:\t%ld\n", led.report.file_in_count);
    fprintf(stderr, "file_output_count:\t%ld\n", led.report.file_out_count);
    fprintf(stderr, "file_match_count:\t%ld\n", led.report.file_match_count);
}

//-----------------------------------------------
// LED main
//-----------------------------------------------

int main(int argc, char* argv[]) {
#ifdef _WIN32
    // Suppress unused parameter warning for argc argv on Windows
    (void)argc;
    (void)argv;
    
    // On Windows, get wide-character command line arguments
    int wargc;
    wchar_t** wargv = CommandLineToArgvW(GetCommandLineW(), &wargc);
    if (wargv == NULL) {
        return 1; // Error handling
    }

    // Allocate new argv for UTF-8 strings
    char** new_argv = malloc((wargc + 1) * sizeof(char*));
    if (new_argv == NULL) {
        LocalFree(wargv);
        return 1;
    }

    // Convert each wide-character argument to UTF-8
    for (int i = 0; i < wargc; i++) {
        int len = WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, NULL, 0, NULL, NULL);
        if (len == 0) {
            for (int j = 0; j < i; j++) free(new_argv[j]);
            free(new_argv);
            LocalFree(wargv);
            return 1;
        }
        new_argv[i] = malloc(len);
        if (new_argv[i] == NULL) {
            for (int j = 0; j < i; j++) free(new_argv[j]);
            free(new_argv);
            LocalFree(wargv);
            return 1;
        }
        WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, new_argv[i], len, NULL, NULL);
    }
    new_argv[wargc] = NULL;

    // Use the new UTF-8 arguments
    led_init(wargc, new_argv);
#else
    // On Linux, use the original arguments
    led_init(argc, argv);
#endif

    // Original program logic
    if (led.opt.help)
        led_help();
    else
        while (led_file_next()) {
            bool isline = false;
            do {
                isline = led_process_read();
                if (led_process_selector()) {
                    led_process_functions();
                    if (led.opt.exec)
                        led_process_exec();
                    else
                        led_process_write();
                }
            } while (isline);
        }
    if (led.opt.report)
        led_report();
    led_free();

#ifdef _WIN32
    // Clean up allocated memory on Windows
    for (int i = 0; i < wargc; i++) {
        free(new_argv[i]);
    }
    free(new_argv);
    LocalFree(wargv);
#endif

    return 0;
}
