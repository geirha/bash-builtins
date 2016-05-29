# csv

A CSV parser and printer written as a loadable builtin and intended to be used similar to the `read` builtin.

## Usage

```
$ help csv
csv: csv [-ap] [-d delim] [-f list] [-F sep] [-q quote] [-u fd] name ...
    Read CSV rows

    Reads a CSV row from standard input, or from file descriptor FD
    if the -u option is supplied.  The line is split into fields on
    commas, or sep if the -F option is supplied, and the first field
    is assigned to the first NAME, the second field to the second
    NAME, and so on.  If there are more fields in the row than
    supplied NAMEs, the remaining fields are read and discarded.

    Options:
      -a       assign the fields read to sequential indices of the array
               named by the first NAME, which may be associative.
               If a second NAME to an indexed array is provided, its values
               will be used as keys if the first NAME is an associative
               array. If the second NAME is empty, a row will be read into
               it first
      -d delim read until the first character of DELIM is read,
               rather than newline or carriage return and newline.
      -f list  read only the listed fields. LIST is a comma separated list
               of numbers and ranges. e.g. -f-2,5,7-8 will pick fields
               0, 1, 2, 5, 7, and 8.
      -F sep   split fields on SEP instead of comma
      -p       print a csv row instead of reading one. Each NAME are printed
               separated by SEP, and quoted if necessary. If the -a option
               is supplied, the first NAME is treated as an array holding the
               values for the row. A second NAME may be provided to specify
               the order of the printed fields.
      -q quote use QUOTE as quote character, rather than `"'.
      -u fd    read from file descriptor FD instead of the standard input.

    Exit Status:
    The return code is zero, unless an error occured, or end-of-file was
    encountered with no bytes read.
```

## Examples

### Read a csv row

```bash
csv one two three <<< foo,bar,baz
# one=foo two=bar three=baz
```

handles fields containing newlines, commas and quotes too

```bash
csv one two <<< $'"comma: , quote: "" newline: \n second line",last'
printf '<%s>\n' "$one" "$two"
## Output:
#<comma: , quote: " newline:
# second line>
#<last>
```

and using different separator

```bash
csv -F: name passwd uid gid gecos home shell < /etc/passwd
# name=root passwd=x uid=0 gid=0 gecos=root home=/root shell=/bin/bash
```

reading into an array

```bash
csv -F: -a row < /etc/passwd
# row=( [0]=root [1]=x [2]=0 [3]=0 [4]=root [5]=/root [6]=/bin/bash )
```

and associative array

```bash
declare -A row
header=( name passwd uid gid gecos home shell )
csv -F: -a row header < /etc/passwd
# row=( [name]=root [passwd]=x [uid]=0 [gid]=0 [gecos]=root [home]=/root [shell]=/bin/bash )
```

### Read the first entry of books.csv

```bash
csv author title publish_year isbn < books.csv
printf '«%s» was written by %s and published in %d\n' "$title" "$author" "$publish_year"
## Output:
#«A Game of Thrones» was written by Martin, George R.R. and published in 1996
```

--

### Read all the entries of books.csv

```bash
while csv author title publish_year isbn; do
  printf '«%s» was written by %s and published in %d\n' "$title" "$author" "$publish_year"
done < books.csv
## Output:
#«A Game of Thrones» was written by Martin, George R.R. and published in 1996
#«Consider Phlebas» was written by Banks, Iain M. and published in 1987
#«Gödel, Escher, Bach: An Eternal Golden Braid» was written by Hofstadter, Douglas and published in 1979
```

--

### Read all the entries of books.csv and write them back to books2.csv sorted by publishing year and including a header line.


First reading each column into separate arrays:

```bash
title=() author=() year=() isbn=()
i=0
while csv 'author[i]' 'title[i]' 'year[i]' 'isbn[i]'; do
    ((i++))
done <books.csv
# author=( [0]="Martin, George R.R." [1]="Banks, Iain M." [2]="Hofstadter, Douglas" )
# title=( [0]="A Game of Thrones" [1]="Consider Phlebas" [2]="Gödel, Escher, Bach: An Eternal Golden Braid" )
# year=( [0]=1996 [1]=1987 [2]=1979 )
# isbn=( [0]=0-553-10354-7 [1]=0-333-45430-8 [2]=978-0-465-02656-2 )
```

Find the right index order for sorting by year using the loadable `asort` builtin:

```bash
asort -ni sorted year
# year=( [0]=1996 [1]=1987 [2]=1979 )
# sorted=( [0]=2 [1]=1 [2]=0 )
```

Write the new file, including header.

```bash
{
    # Printing the header
    csv -p Title Author "Publishing year" ISBN

    # Printing all the rows
    for i in "${sorted[@]}"; do
        csv -p "${title[i]}" "${author[i]}" "${year[i]}" "${isbn[i]}"
    done
} >books2.csv
```
