A small collection of loadable bash builtins I've written, mainly to see what
is possible to do with loadable builtins. Some of these may be useful.

## Preparation

Currently bash 4.4 is not yet released, and one of the new things in bash 4.4
will be an easier way to build loadable builtins. So until bash 4.4 is released
and systems have caught up with this new version, you'll have to build it
yourself, or at least configure it and install the headers

### Grab the bash devel sources

If this is the first time, clone the repository

```
git clone git://git.sv.gnu.org/bash.git
```

Then enter the cloned repo, and switch to the devel branch:

```
cd bash &&
git checkout devel &&
git pull
```

Then configure it and run make on the `install-headers` target
```
./configure && make install-headers
```

The above should install `"$prefix/lib/pkgconfig/bash.pc"` in addition to some
header files.


## Build bash-builtins

To build, just run

```
make
```

## Install

*To be continued...*
