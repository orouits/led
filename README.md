# LED
Line Editor (led) is a simple command line utility written in C to edit text files using the well known PCRE2 library by Philip Hazel for modern REGEX synthax and more.
It aims to cover in one tool common text search/replace functions that can sometimes need multiple tools like sed, grep, tr, awk, perl ... 

# Command line synthax

## Overview

led command line arguements is composed of section in the folling order:

`led [selector] :[processor] [-options] [files]`

the section recognition si done by keywork or options.

Led text processor workflow is basic, it optionally select parts of input text (or all if not given), optionally procces selected text and print to output (with options).

There is only one selector and one processor function per led call. *led* do not embeed a complex transformation language as *sed* to achieve multiple transformation.
The choice is to manage complex multiple tranformation through step by step calls with pipe invocation with an advanced way that allows pipes and massive file change together.

### The selector

the selector allows to apply the line processor on selected lines of the input text.
the selector is optional, it must be declared before a recognized processor function, the default lines selector is set as none (all lines selected)  

When led is used only with the selector, it is equivalent to grep with a similar addressing feature as sed using PCRE2 as regex engine. 

#### syntax:

- selector := [address_from] [address_to]  
- address := regex|number

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

# select block of lines from 1 line after lines containing "abc" to 1 line after line containing "def" (not included)
cat file.txt | led abc +1 def +1

# select lines 2 lines after lines containing "abc"
cat file.txt | led abc +2

# block of lines selected will be given to the processor line by line (default)
cat file.txt | led abc def line

# block of lines selected will be given to the processor once in a multi-line buffer (for multi-line processing)
cat file.txt | led abc def block

```


### The processor  

- processor := cmd arg arg ...

#### sb|substitute command

The `:sub|:substitute` command allows to substitute string from a regex.

PCRE2 library substitution feature is used (see https://www.pcre.org/current/doc/html/pcre2_substitute.html).
`PCRE2_SUBSTITUTE_EXTENDED` option is used in order to have more substitution flexibility (see https://www.pcre.org/current/doc/html/pcre2api.html#SEC36).

`:sub|:substitute <regex> <replace>`

- regex: the search regex string
- replace: th replace string

#### ex|execute command

The `:exe|:execute` command allows to substitute string from a regex and execute it.

PCRE2 library substitution feature is used (see https://www.pcre.org/current/doc/html/pcre2_substitute.html).
`PCRE2_SUBSTITUTE_EXTENDED` option is used in order to have more substitution flexibility (see https://www.pcre.org/current/doc/html/pcre2api.html#SEC36).

`:exe|:execute <regex> <command>`

- regex = the search regex string
- command: the replace string to be executed as a command with arguments 

#### rm|remove command

Remove line 

`:rm|:remove`

#### in|insert command

Insert line(s) 

`:ins|:insert string [N]`

#### ap|append command

Append line(s) 

`:app|:append string [N]`

#### rn|range command

Extract a range of characters in the line 

`:rn|:range N [C]`
`:rnn|:rangenot N [C]`

- N: from column, relative to the end of line if N is negative
- C: character count, 1 by default


#### tr|translate command

Translate characters string of a matching regex.

`:tr|:translate <schars> <dchars>`

- schars: a sequence of source characters to be replaced by dest characters 
- dchars: a sequence of dest characters
- regex: modification of the matching zone in line, if a capture is present, only the first capture is modified

#### case commands

`csl|caselower [<regex>]`
`csu|caseupper [<regex>]`
`csf|casefirst [<regex>]`
`csc|casecamel [<regex>]`

- regex: modification of the matching zone in line, if a capture is present, only the first capture is modified

#### quote commands

`qts|quotesimple [<regex>]`
`qtd|quotedouble [<regex>]`
`qtb|quoteback [<regex>]`
`qtr|quoteremove [<regex>]`

- regex: modification of the matching zone in line, if a capture is present, only the first capture is modified


#### trim commands

Trim a line.

`tm|trim       [<regex>]`
`tml|trimleft  [<regex>]`
`tmr|trimright [<regex>]`

- regex: modification of the matching zone in line, if a capture is present, only the first capture is modified

#### split commands

`sp|split [<regex>]`

- regex: matching separator string, blank + tab by default

#### revert commands

`rv|revert [<regex>]`

- regex: modification of the matching zone in line, if a capture is present, only the first capture is modified

#### field commands

 Extract fields of a line.

`fl|field [N] [N] ... [<regex>]`

- N: extract the Nth field, by default the first one. 
- regex: matching delimiter string, by default blanks and tabs

#### join command

 Join lines.
 This function needs selector `block` mode to transmit all lines in the same buffer.

`jn|join [N]`

- N: every N line. 

#### crypt commands

 encrypt or decrypt lines.
 This function can work with selector `block` mode to encrypt a block of lines or a whole file.

`ecrb64|encriptbase64`
`ecrmd5|encriptmd5`
`ecrsha1|encriptsha1`
`ecrsha256|encriptsha256`
`ecraes256|encriptaes256`

`dcrb64|decriptbase64`
`dcrmd5|decriptmd5`
`dcrsha1|decriptsha1`
`dcrsha256|decriptsha256`
`dcraes256|decriptaes256`

#### uc|urlencode command

 URL encode|decode lines.

`urc|urlencode [<regex>]`
`urd|urldecode [<regex>]`

- regex: modification of the matching zone in line, if a capture is present, only the first capture is modified

#### ph|path command

 Modify line as path.

`phc|pathcanonical [<regex>]`   set canonical path (default)
`phd|pathdir [<regex>]`         extract directory
`phf|pathfile [<regex>]`        extract filename
`phr|pathrename [<regex>]`      rename with magic filename (experimental), make filenames camel case without non-alnum except `.`  

- regex: modification of the matching zone in line, if a capture is present, only the first capture is modified

#### rn|randomize

 Generate randomized characters

`rnn|randomnum [<regex>]`
`rna|randomalpha [<regex>]`
`rnan|randomalphanum [<regex>]`

- regex: modification of the matching zone in line, if a capture is present, only the first capture is modified

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

the **-f** option tells led to enter into the files section which is the last one, every subsequent argument is considered as an intput file name.

### Advanced pipe mode with files

`find . -spec <filespec> | led -f`

the file section is limitted to the **-f** option without any files behind.
It tells led to read file names from STDIN.

## Options

### Selector options

_ `-n` invert selection 
_ `-b` selected lines as blocks. 
_ `-s` output only selected

### File options

- `-f` read filenames to STDIN instead of content, or from command line if followd by arguments as file names (file section)  
- `-F` write filenames to STDOUT instead of content. This option is made to be used with advanced pipe mode with files to build a pipeline of multiple led transformation chained on multiple files.
- `-I` write content to filename inplace
- `-W<path>` write content to a fixed file
- `-A<path>` append content to a fixed file
- `-E<ext>` write content to filename.ext
- `-E<3>` write content to filename.NNN
- `-D<dir>` write files in dir. 
- `-U` write unchanged filenames

### Global options

- `-z` end of line is 0
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

`led sb <regex> <replace> -f file.txt`

`cat file.txt | led sb <regex> <replace> > file-changed.txt`

change inplace:

`led sb <regex> <replace> -FIf file.txt`


## massive multi change inplace:

` ls *.txt | led sb <regex> <replace> -FIUf | led sb <regex> <replace> -FIUf | ...`
