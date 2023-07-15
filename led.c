#include "led.h"

#define ARGS_SEC_SELECT 0
#define ARGS_SEC_FUNCT 1
#define ARGS_SEC_FILES 2

const char* LED_SEC_LABEL[] = {
    "selector",
    "function",
    "files",
};

#define SEL_TYPE_NONE 0
#define SEL_TYPE_REGEX 1
#define SEL_TYPE_COUNT 2
#define SEL_COUNT 2

// #define SEL_FN_LINE 0
// #define SEL_FN_BLOCK 1

#define LED_EXIT_STD 0
#define LED_EXIT_VAL 1

#define LED_FILE_OUT_NONE 0
#define LED_FILE_OUT_INPLACE 1
#define LED_FILE_OUT_WRITE 2
#define LED_FILE_OUT_APPEND 3
#define LED_FILE_OUT_NEWEXT 4

const void* LED_FN_MAP[] = {
    ":nn", ":none", &led_fn_none, NULL, "No processing",
    ":sub", ":substitute", &led_fn_substitute, &led_fncfg_substitute, "Substitute <regex> <replace>",
    ":exe", ":execute", NULL, NULL, "Execute <regex> <replace=command>",
    ":rm", ":remove", &led_fn_remove, NULL, "Remove the line",
    ":ins", ":insert", &led_fn_insert, NULL, "Insert <string> [N]",
    ":app", ":append", &led_fn_append, NULL, "Append <string> [N]",
    ":rns", ":rangesel", &led_fn_rangesel, NULL, "Range select <start> [count]",
    ":rnu", ":rangeunsel", NULL, NULL, "Range unselect <start> [count]",
    ":tr", ":translate", &led_fn_translate, NULL, "Translate <chars> <chars>",
    ":csl", ":caselower", &led_fn_caselower, NULL, "Case to lower",
    ":csu", ":caseupper", &led_fn_caseupper, NULL, "Case to upper",
    ":csf", ":casefirst", &led_fn_casefirst, NULL, "Case first upper",
    ":csc", ":casecamel", &led_fn_casecamel, NULL, "Case to camel style",
    ":qts", ":quotesimple", &led_fn_quotesimple, NULL, "Quote simple",
    ":qtd", ":quotedouble", &led_fn_quotedouble, NULL, "Quote double",
    ":qtb", ":quoteback", &led_fn_quoteback, NULL, "Quote back",
    ":qtr", ":quoteremove", NULL, NULL, "Quote remove",
    ":tm", ":trim", NULL, NULL, "Trim",
    ":tml", ":trimleft", NULL, NULL, "Trim left",
    ":tmr", ":trimright", NULL, NULL, "Trim right",
    ":sp", ":split", NULL, NULL, "Split",
    ":rv", ":revert", NULL, NULL, "Revert",
    ":fl", ":field", NULL, NULL, "Extract fields",
    ":jn", ":join", NULL, NULL, "Join lines",
    ":ecrb64", ":encryptbase64", NULL, NULL, "Encrypt base64",
    ":dcrb64", ":decryptbase64", NULL, NULL, "Decrypt base64",
    ":urc", ":urlencode", NULL, NULL, "Encode URL",
    ":urd", ":urldecode", NULL, NULL, "Decode URL",
    ":phc", ":pathcanonical", NULL, NULL, "Conert to canonical path",
    ":phd", ":pathdir", NULL, NULL, "Extract last dir of the path",
    ":phf", ":pathfile", NULL, NULL, "Extract file of the path",
    ":phr", ":pathrename", NULL, NULL, "Rename file of the path without specific chars",
    ":rnn", ":randomizenum", NULL, NULL, "Randomize numeric values",
    ":rna", ":randomizealpha", NULL, NULL, "Randomize alpha values",
    ":rnan", ":randomizealphaum", NULL, NULL, "Randomize alpha numeric values",
};

led_struct led;

//-----------------------------------------------
// LED init functions
//-----------------------------------------------

int led_init_opt(const char* arg) {
    int rc = led_str_match(arg, "-\\D");
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
                led.argsection = ARGS_SEC_FILES;
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
    int map_sz = sizeof(LED_FN_MAP)/sizeof(void*);
    for (int i = 0; i < map_sz; i+=5) {
        if ( led_str_equal(arg, (const char*)LED_FN_MAP[i]) || led_str_equal(arg, (const char*)LED_FN_MAP[i + 1]) ) {
            // match with define constant
            led.func.id = i/5;
            led.func.label = (const char*)LED_FN_MAP[i + 1];
            led.func.ptr = (void (*)(void))LED_FN_MAP[i + 2];
            led.func.cfgptr = (void (*)(void))LED_FN_MAP[i + 3];
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
                led_debug("Selector: %d, %d", i, led.sel[i].type);
                break;
            }
        }
    }
    return rc;
}

void led_init(int argc, char* argv[]) {
    led_debug("Init");

    memset(&led, 0, sizeof(led));
    
    led.func.id = 0;
    led.func.label = (const char*)LED_FN_MAP[1];
    led.func.ptr = (void (*)(void))LED_FN_MAP[2];
    led.func.cfgptr = (void (*)(void))LED_FN_MAP[3];

    led.stdin_ispipe = !isatty(fileno(stdin));
    led.stdout_ispipe = !isatty(fileno(stdout));

    for (int argi=1; argi < argc; argi++) {
        const char* arg = argv[argi];

        if (led.argsection == ARGS_SEC_FILES ) {
            led.file_names = argv + argi;
            led.file_count = argc - argi;
        }
        else if (led.argsection < ARGS_SEC_FILES && led_init_opt(arg) ) ;
        else if (led.argsection == ARGS_SEC_FUNCT && led_init_func_arg(arg) ) ;
        else if (led.argsection < ARGS_SEC_FUNCT && led_init_func(arg) ) led.argsection = ARGS_SEC_FUNCT;
        else if (led.argsection == ARGS_SEC_SELECT && led_init_sel(arg) ) ;
        else led_assert(FALSE, LED_ERR_ARG, "Unknown argument: %s (%s)", arg, LED_SEC_LABEL[led.argsection]);
    }

    // if a process function is not defined show only selected
    led.o_output_selected = led.o_output_selected || !led.func.id;

    led_debug("Function: %s (%d)", led.func.label, led.func.id);

    // pre-configure the processor command
    if (led.func.cfgptr) (*led.func.cfgptr)();
}

void led_help() {
    fprintf(stderr, "led <selector> [:<processor>] [-options] [files] ...\n\n");
    fprintf(stderr, "Processor commands:\n");
    fprintf(stderr, "| %-8s | %-20s | %-50s |\n", "Short", "Long name", "Description");
    int map_sz = sizeof(LED_FN_MAP)/sizeof(void*);
    for (int i = 0; i < map_sz; i+=5) {
        fprintf(stderr, "| %-8s | %-20s | %-50s |\n",
            (const char*)LED_FN_MAP[i],
            (const char*)LED_FN_MAP[i+1],
            (const char*)LED_FN_MAP[i+4]
        );
    }
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
    if (led.func.ptr) 
        (*led.func.ptr)();
    else 
        led_assert(FALSE, LED_ERR_ARG, "Function not implemented: %s", led.func.label);
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
