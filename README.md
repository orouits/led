# LED
Line Editor (led) is a simple command line utility written in C to edit text files using the well known PCRE2 library by Philip Hazel for modern REGEX synthax and more.
It aims to cover in one tool common text search/replace functions may need sometimes multiple tools like sed, grep, tr, awk, perl ... 

# Command line synthax

## Overview

led command line arguements is composed of section in the folling order:

`led [selector] [processor] [-options] [files]`

the section recognition si done by keywork or options.

Led text processor workflow is basic, it optionally select parts of input text (or all if not given), optionally procces selected text and print to output (with options).

There is only one selector and one processor function per led call. *led* do not embeed a complex transformation language as *sed* to achieve multiple transformation.
The choice is to manage complex multiple tranformation through step by step calls with pipe invocation with an advanced way that allows pipes and massive file change together.

### The selector  

the selector allows to apply the line processor on selected lines of the input text.
the selector is optional, it must be declared before a recognized processor function, the default lines selector is set as none (all lines selected)  

When led is used only with the selector, it is equivalent to grep with a similar addressing feature as sed using PCRE2 as regex engine. 

#### syntax:

- selector := address1 [address2] [mode]
- address := regex|number
- mode := line|block

addressing examples:

```
# select the fisrt line only
cat file.txt | led 1

# select block of lines from line 10 and 3 next ones (including line 10)
cat file.txt | led 10 3

# select all lines containing abc (regex)
cat file.txt | led abc

# select all block of lines from a line containing "abc" to a line containing "def" (not included)
cat file.txt | led abc def

# select all block of lines from a line containing "abc" and 5 next ones
cat file.txt | led abc 5

# select block of lines from line 4 to a line containing "def" (not included)
cat file.txt | led 4 def

# block of lines selected will be given to the processor line by line (default)
cat file.txt | led abc def line

# block of lines selected will be given to the processor once in a multi-line buffer (for multi-line processing)
cat file.txt | led abc def block
```

### The processor  

- processor := cmd arg arg ...

#### sb|substitute command

The `sb` command allows to substitute string from a regex.

PCRE2 library substitution feature is used (see https://www.pcre.org/current/doc/html/pcre2_substitute.html).
`PCRE2_SUBSTITUTE_EXTENDED` option is used in order to have more substitution flexibility (see https://www.pcre.org/current/doc/html/pcre2api.html#SEC36).

`sb <regex> <replace> [<opts>]`

- regex: the search regex string
- replace: th replace string
- opts:
    - "g" fo global search

#### ex|execute command

The `ex` command allows to substitute string from a regex and execute it.

PCRE2 library substitution feature is used (see https://www.pcre.org/current/doc/html/pcre2_substitute.html).
`PCRE2_SUBSTITUTE_EXTENDED` option is used in order to have more substitution flexibility (see https://www.pcre.org/current/doc/html/pcre2api.html#SEC36).

`ex <regex> <command> [<opts>]`

- regex = the search regex string
- command: the replace string to be executed as a command with arguments 
- opts:
    - "g" for global search
    - "s" stop on error

#### rg|range command

Extract a range of characters in the line 

`rg N [C] [<opts>]`

- N: Columb from character N, if N is negative, from the end of the line
- C: Count character, 1 by default
- opts:
    - n: all but not this range

#### tr|translate command

Translate characters string of a matching regex.

`tr <schars> <dchars> [<regex>]`

- schars: a sequence of source characters to be replaced by dest characters 
- dchars: a sequence of dest characters
- regex: modification of the matching zone in line, if a capture is present, only the first capture is modified

#### cs|case command

Modify the case of a line.

`cs [<opts>] [<regex>]`

- opts: (only one) 
   - l: lowercase (default)
   - u: uppercase
   - f: only first is upper
   - c: camel case by detecting words and suppressing non alnum characters
- regex: modification of the matching zone in line, if a capture is present, only the first capture is modified

#### qt|quote command

Quote / uquote a line (idempotent).

`qt [<opts>] [<regex>]`

- opts: (only one) 
   - s: simple quote (default)
   - d: double quote
   - u: unquote (simple or double)
- regex: modification of the matching zone in line, if a capture is present, only the first capture is modified

#### tm|trim command

Trim a line.

`tm [<opts>] [<regex>]`

- opts: (only one)
   - r: right (default)
   - l: left
   - a: all
- regex: modification of the matching zone in line, if a capture is present, only the first capture is modified

#### sp|split command

Split a line.

`sp [<regex>]`

- regex: matching separator string, blank + tab by default

#### rv|revert command

Revert a line.

`rv [<regex>]`

- regex: modification of the matching zone in line, if a capture is present, only the first capture is modified

#### fl|field command

 Extract fields of a line.

`fl [N] [N] ... [<regex>]`

- N: extract the Nth field, by default the first one. 
- regex: matching delimiter string, by default blanks and tabs

#### jn|join command

 Join lines.
 This function needs selector `block` mode to transmit all lines in the same buffer.

`jn [N]`

- N: every N line. 

#### cr|crypt command

 encrypt or decrypt lines.

`cr <type> [<opts>]`

- type: 
    - b64
    - md5
    - sha1
    - sha256
    - aes256<key>
- opts
    - e: encrypt (default)
    - d: decrypt (error for hash algorithms)

#### ul|urlencode command

 URL encode|decode lines.

`ul [<opts>] [<regex>]`

- opts
    - e: encode (default)
    - d: decode
- regex: modification of the matching zone in line, if a capture is present, only the first capture is modified

#### ph|path command

 Modify line as path.

`ph [<opts>] [<regex>]`

- opts
    - c: set canonical path (default)
    - d: extract directory
    - f: extract filename
    - m: rename with magic filename (experimental), make filenames camel case without non-alnum except `.`  
- regex: modification of the matching zone in line, if a capture is present, only the first capture is modified
 
## Invocation

3 way to invoque led:
* pipe mode
* direct mode with files
* advanced pipe mode with files

### Pipe mode

`cat <file> | led ...`

this the default mode when the [files] section is not given.

### Direct mode with files

`led ... -f <file> <file> ...`

the **-f** option tells led to enter into the files section which is the last one, every subsequent argument is considered as an intput file name.

### Advanced pipe mode with files

`find . -spec <filespec> | led -f`

the file section is limitted to the **-f** option without any files behind.
It tells led to read file names from STDIN.

## Options

# Examples
