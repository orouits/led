# Ideas
## new processor format

```
echo abc | led fls//1
echo ABC | led cs/
echo abc def | led 'fls/(bc d)/1'
echo abc | led tr//ax/dz
find . | led 's//mv $0 __$0' 'cc/ __(.+)' 's/__//' -X        # => mv en masse
```

## Summary:

- can chain multiple commands on current line. Each command input take is the output of the previousline like a pipe.
- a command is defined se défined by a unique shell arg with the following syntax: `cmd/[regex][/arg][/arg]...`
- '/' separator an be replaced by ':' to zvoid escaping in values.
- short name of substitute/ is s/ to be as near as possible as sed substitute form.
- every commnd runs in a context defined by a regex (1st param). If not defined '.*' pattern is used. The back reference pcre2 of globzl match is $0
- instead of output lines on STDOUT or a file (-F -E -D...) led can execute the transformed line as a command with -X option. by extention, `led -X` is equivalent to `xargs -L 1`
- for the selector each parameter is a shell arg as currently.
- fname_xxx/ commands will be redefined as case_xxx/ commands. But some of them will not only change the case.
- a register command will also be added to allow to recall some values registered before in `s/` subsequent commands 

```
led 'rg/abc(.+)/B' # capture in register B
led 'rg/abc(.+)' # capture in register A (default register)
led 'rg/' # all the line in register A (default regex, default register)
```

This can simplify massive move usecase:
```
find . | led rg/ cc/ 's//mv $A $0' -X        # => mv en masse
```
An alternative for command regex was to dedicate a command for it that modify the scope of the next command:
```
echo abc def | led 'p/(bc d)' fls/1
find . | led 's/mv $0 __$0' 'p/ __(.+)' cc/ p/__ 's//' -X
```
But it is more fuzzy.

