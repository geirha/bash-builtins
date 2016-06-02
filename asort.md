# asort

A loadable builtin that sorts arrays in place

## Usage

```
$ help asort
asort: asort [-nr] array ...  or  asort [-nr] -i dest source
    Sort arrays in-place.
    
    Options:
      -n  compare according to string numerical value
      -r  reverse the result of comparisons
    
    If -i is supplied, SOURCE is not sorted in-place, but the indices (or keys
    if associative) of SOURCE, after sorting it by its values, are placed as
    values in the indexed array DEST
    
    Exit status:
    Return value is zero unless an error happened (like invalid variable name
    or readonly array).
```

## Examples

### Sort lexicographically

```bash
array=( banana apple pear lemon 2 10 )
asort array
# array=( [0]=10 [1]=2 [2]=apple [3]=banana [4]=lemon [5]=pear )
```

and in reverse:

```bash
asort -r array
# array=( [0]=pear [1]=lemon [2]=banana [3]=apple [4]=2 [5]=10 )
```

### Sort numerically

```bash
array=( 3.14 2 10 -1 .5 )
asort -n array
# array=( [0]=-1 [1]=.5 [2]=2 [3]=3.14 [4]=10 )
```

### Sort and store the indices/keys of original array

```bash
declare -A assoc=( [A]=10 [B]=25 [C]=2 [D]=0 [E]=6 )
asort -ni sorted assoc
# sorted=( [0]=D [1]=C [2]=E [3]=A [4]=B )

for key in "${sorted[@]}"; do
    printf '%s: %2d\n' "$key" "${assoc[$key]}"
done
## Output:
#D:  0
#C:  2
#E:  6
#A: 10
#B: 25
```
