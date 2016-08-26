A small collection of loadable bash builtins I've written, mainly to see what
is possible to do with loadable builtins. Some of these may be useful.

* [asort](asort.md) - sort arrays in-place
* [csv](csv.md) - read and write CSV rows
* [md5](md5.md) - calculate md5 sum

## Preparation

Currently bash 4.4 is not yet released, and one of the new things in bash 4.4
will be an easier way to build loadable builtins. So until bash 4.4 is released
and systems have caught up with this new version, you'll have to build it
yourself, or at least configure it and install the headers

### Grab the bash devel sources

If this is the first time, clone the repository

```bash
git clone git://git.sv.gnu.org/bash.git
```

Then enter the cloned repo, and switch to the devel branch:

```bash
cd bash &&
git checkout devel &&
git pull
```

Then configure it and run make on the `install-headers` target
```bash
./configure &&
make &&
make install-headers &&
make -C examples/loadables install-dev
```

The above should install `"$prefix/lib/bash/Makefile.inc"` in addition to some
header files.


## Build bash-builtins

To build, just run

```bash
make
```

If you specified a --prefix on the configure, add this to make so it finds the installed files:

```bash
make prefix=/some/where
```

## Install

*To be continued...*
