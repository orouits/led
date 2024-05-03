# LED
Line EDitor (LED) is a simple command line utility to edit text files using the well known PCRE2 library (Philip Hazel) for modern REGEX synthax and more.
It aims to cover in one tool common text search/replace/process functions that can often need multiple tools combined like sed, grep, tr, awk, perl ...

# Command line synthax

## Overview

The **led** command line arguments is composed of section in the folling order:

`led [SELECTOR] [PROCESSOR] [-opts...] [-f] [FILES...]`

`cat [FILES...] | led [SELECTOR] [PROCESSOR] [-opts...]`

`ls [DIR] | led [SELECTOR] [PROCESSOR] [-opts...] -f`

The options can be anywhere before -f option
The section recognition (selector, processor) depends on the arguments format and content.

Led text processing workflow is simple:

input text lines >> select lines >> process lines >> output text lines

There is only one selector and one processor function per led call. **led**. do not implement a complex transformation language as **sed**. To achieve multiple transformation, just use pipes power with multiple led calls.

//TODO fully support UTF8

### The selector

the selector allows to apply the line processor on selected lines of the input text.
the selector is optional, it must be declared before a recognized processor function, the default lines selector is set as none (all lines selected)

When led is used only with the selector, it is equivalent to grep with a similar addressing feature as sed using PCRE2 as regex engine.

Example:

`cat <file> | grep Test`

is equivalent to:

`cat <file> | led Test`

#### syntax:

- selector := [address_from] [address_to]
- address  := regex|number

addressing examples:

```
# select the fisrt line only
cat file.txt | led 1

# select block of lines from line 10 and 3 next ones (including line 10)
cat file.txt | led 10 3

# select all lines containing abc (regex)
cat file.txt | led abc

# for each line containing `abc` select the block of lines until it contains `def` (not included)
cat file.txt | led abc def

# for each line containing `abc` select the block of 5 next ones
cat file.txt | led abc 5

# select block of lines from line 4 to a line containing `def` (not included)
cat file.txt | led 4 def

# select block of lines from 1 line after lines containing `abc` to 1 line after line containing `def` (not included)
cat file.txt | led abc +1 def +1

# for each line containing `abc`; select the line that is 2 lines after
cat file.txt | led abc +2

# block of contiguous selected lines will be processed 1 per 1 (default)
cat file.txt | led abc 10 <processor>

# block of contiguous selected lines will be packed and processed all together (multi-line processing)
cat file.txt | led abc 10 <processor> -p

```

### The processor section

The processor section is composed of 1 or N functions. Each function is a separate shell argument.

- `function/[regex][/arg1][/arg2]... function/[regex][/arg1][/arg2]...`

if the processor is not defined, led just output lines selected by the selector

The `regex` is used to identify a zone where the function is applied in the line. It is always the first argument but can be empty.
- if regex is empty
    - `function/`
    - `function//arg1`
    - the default regex is used (.*)
    - it allows to apply the operation on the whole line
- if defined
    - `function/Abc.+/`
    - `function/Abc.+/arg1`
    - it allows to apply the operation on the mathing zone or first capture zone (if defined) only.
- if defined with capture block (...)
    - it allows to apply the operation on the first capture zone only for better context matching

Each function has a short name and a long name
- the argement separator can be replaced by `:` instead of `/` to solve character escaping.

Argments follows the regex.

#### s|substitute function

The `s|substitute` function allows to substitute string from a regex.

PCRE2 library substitution feature is used (see https://www.pcre.org/current/doc/html/pcre2_substitute.html).
`PCRE2_SUBSTITUTE_EXTENDED` option is used in order to have more substitution flexibility (see https://www.pcre.org/current/doc/html/pcre2api.html#SEC36).

`s|substitute/[regex]/replace[/opt]`

- regex: the search regex string
- replace: the replace string
- opts: character sequence of PCRE2 options (ge...)

#### rm|remove function

Remove line

`rm|remove/`

#### ins|insert function

Insert replaced line(s)

`ins|insert/[regex]/<replace>[/N]`

- replace: the replaced string to insert before line
- N: number of line inserted, default 1

#### app|append function

Append replaced line(s)

`app|append/[regex]/<replace>[/N]`

- replace: the replaced string to append after line
- N: number of line appended, default 1

#### rn|range functions

Extract a range of characters in the line

`rn|range/[regex]/N[/C]`

`rnn|rangenot/[regex]/N[/C]`

- N: from column, relative to the end of line if N is negative
- C: character count, 1 by default


#### tr|translate function

Translate characters string of a matching regex.

`tr|translate/[regex]/<src_chars>/<dst_chars>`

- schars: a sequence of source characters to be replaced by dest characters
- dchars: a sequence of dest characters


#### case functions

Convert to various case

`csl|case_lower/[regex]`

`csu|case_upper/[regex]`

`csf|case_first/[regex]`

`csc|case_camel/[regex]`

#### quote functions

Quote and unquote if needed.

`qt|quote_simple/[regex]`

`qtd|quote_double/[regex]`

`qtb|quote_back/[regex]`

`qtr|quote_remove/[regex]`

#### trim functions

`tm|trim/[regex]`

`tml|trim_left/[regex]`

`tmr|trim_right/[regex]`

#### split functions

 split line with given separators

`sp|split/[regex]/<sep_chars>`

split line with comma separators

`spc|split_csv/[regex]`

split line with space separators

`sps|split_space/[regex]`

split line with space and comma separators

`spm|split_mixed/[regex]`

#### revert function

revert the char order of the line

`rv|revert/[regex]`

#### field functions

 Extract fields of a line.

`fl|field/[regex]/<N>/<sep_chars>`

`flc|field_csv/[regex]/<N>`

`fls|field_space/[regex]/<N>`

`flm|field_mixed/[regex]/<N>`

- N: extract the Nth field, by default the first one.
- sep: separator chars

#### join function

 Join lines.
 This function needs selector `pack` mode to transmit all lines in the same buffer.

`j|join`

#### base64 encoding functions

 encode/decode lines.
 This function can work with selector `block` mode to encrypt a block of lines or a whole file.

`b64e|base64_encode/[regex]`

`b64d|base64_decode/[regex]`

#### urlencode function

 URL encode line or part of line.

`urle|url_encode/[regex]`

#### shel escape / unescape functions

 Escape chars for shell executions.

`she|shell_escape/[regex]`

`shu|shell_unescape/[regex]`

#### path functions

Modify path in a line.

`rp|realpath/[regex]`

`dn|dirname/[regex]`

`bn|basename/[regex]`

#### file name functions

Modify file name in a line, path prefix is not modified.

`fnl|fname_lower/[regex]`

`fnu|fname_upper/[regex]`

`fnc|fname_camel/[regex]`

`fns|fname_snake/[regex]`

#### randomize functions

Generate randomized characters

`rzn|randomize_num/[regex]`

`rza|randomize_alpha/[regex]`

`rzan|randomize_alnum/[regex]`

`rzh|randomize_hexa/[regex]`

`rzm|randomize_mixed/[regex]`

#### Generate chars function

Generate randomized characters

`gen|generate/[regex]/<char>[/N]`

- char: the character to repeat
- N: char count, default is 1

#### Register function

Registering line or part of line into one or more temporary registers (0 to 9)
A register can be used into a substitue replace string with using `$R[N]` notation.

`r|register/[regex][/N]`

- without N: each capture of the regex is copied into subsequent register IDs
- N: the regex capture is copied int the given register ID

`r/` => all the line into R0

`r//1` => all the line into R1

`r/(\w+),(\w+)/` => all the matching zone into R0, first capture into R1, second captureinto R2

#### Register recall function

Copy (recall) a register value (or part of the value) to line

`rr|register_recall/[regex][/N]`

- `[regex]`: regex to copy only a part of register value
- `N`: the register ID to be copied, 0 if not defined.

`rr/` => R0 copied to the current line

`rr//1` => R1 copied to the current line

`rr/=(\w+)/1` => first catched group of R1 copied to the current line

## Invocation

3 way to invoque led:
* pipe mode
* direct mode with files
* advanced pipe mode with files

### Pipe mode

`cat <file> | led ...`

this is the default mode when the `[files]` section is not given.

### Direct mode with files

`led ... -f <file> <file> ...`

the **-f** option must be the last option, every subsequent argument is considered as an intput file name.

### Advanced pipe mode with files

`find . -spec <filespec> | led ... -f`

the file section is limitted to the **-f** option without any files behind. It tells led to read file names from STDIN.

see options -F, -W, -A, -E, -D to ouput filenames and build multiple changes on multiple files

## Options

### Selector options

- `-n` invert selection
- `-b` selected lines as blocks.
- `-s` output only selected

### File options

- `-f` read filenames to STDIN instead of content, or from command line if followd by arguments as file names (file section)
- `-F` write filenames to STDOUT instead of content. This option is made to be used with advanced pipe mode with files to build a pipeline of multiple led transformation chained on multiple files.
- `-W<path>` write content to a fixed file
- `-A<path>` append content to a fixed file
- `-E<ext>` write content to filename.ext
- `-D<dir>` write files in dir.

### Global options

- `-v` verbose to STDERR
- `-r` report to STDERR
- `-q` quiet, do not ouptut anything (exit code only)
- `-e` exit code on value

## Exit code

Standard:
- 0 = match/change
- 1 = no match
- 2 = internal error

On value (see -e):
- 0 = output not empty
- 1 = no match/change
- 2 = internal error

# Examples

## "grep" like

`cat file.txt | led <regex>`

`led <regex> -f file.txt`

## "sed" like for simple substitute

`led s/<regex>/<replace> -f file.txt`

`cat file.txt | led s/<regex>/<replace> > file-changed.txt`

change in-place:

`led s/<regex>/<replace> -F -f file.txt`

## massive multi change inplace:

`ls *.txt | led s/<regex>/<replace> s/<regex>/<replace>  ... -F -f`

## massive execution (rename files in camel case)

`find /path/to/dir -type f | led she/ r/ shd/ fnc/ 's//$R $0'`