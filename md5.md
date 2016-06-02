# md5

Calculate md5sum of a string or stream.

## Usage

```
$ help md5
md5: md5 [string]
    Calculate MD5 sum.
    
    Calculates the MD5 sum of the string argument, or stdin if no
    argument is provided. The result is assigned to the REPLY 
    variable.
```

## Examples

### Calculate MD5 sum of a string

```bash
md5 ''
printf 'MD5 sum of the empty string is: %s\n' "$REPLY"
## Output:
#MD5 sum of «hello» is: d41d8cd98f00b204e9800998ecf8427e
```

```bash
md5 $'hello\n'
printf 'MD5 sum of hello<LF> is: %s\n' "$REPLY"
## Output:
#MD5 sum of hello<LF> is: b1946ac92492d2347c6235b4d2611184
```

### Calculate MD5 sum of a stream/file

```bash
md5 < /dev/null
printf 'MD5 sum of /dev/null is: %s\n' "$REPLY"
## Output:
#MD5 sum of /dev/null is: d41d8cd98f00b204e9800998ecf8427e
```

```bash
md5 <<< hello
printf 'MD5 sum of hello<LF> is: %s\n' "$REPLY"
## Output:
#MD5 sum of hello<LF> is: b1946ac92492d2347c6235b4d2611184
```

## Implementation notes

This was the first builtin I attempted. It was triggered by me solving the
[Advent of Code](http://adventofcode.com) challenges using multiple languages,
including bash. The one requiring calculating ~1M md5 sums took ages when each
one required forking and execing `md5sum(1)` or `md5(1)`. So I decided to try
writing a loadable builtin, and learn a bit about how an md5sum is calculated
in the process.

I wrote it by reading the specs of the RFC, then when it finally produced
correct results, I split it up into multiple functions, similar to how [BSD had
split them up](http://man.openbsd.org/OpenBSD-5.8/md5.3). The current source
can probably be optimized a bit, but the speed gain over forking and execing an
external executable is large enough.
