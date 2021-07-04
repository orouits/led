# LED
Line Editor (led) is a simple command line utility written in C to edit text files using the well known PCRE2 library written by Philip Hazel for modern REGEX synthax and more.
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

composition:

selector := address1 [address2] [mode]
address := regex|number
mode := line|block

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
led abc def block -f file.txt
```

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
