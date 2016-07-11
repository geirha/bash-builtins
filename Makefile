prefix = /usr/local
makefile_inc = $(prefix)/lib/bash/Makefile.inc

all: Makefile.inc
	$(MAKE) -f Makefile.inc

Makefile.inc: Makefile.stub $(makefile_inc)
	{ sed -e '/^all:/,$$d' $(makefile_inc);\
	cat Makefile.stub;\
	} > Makefile.inc

clean:
	$(MAKE) -f Makefile.inc clean
	rm -f Makefile.inc

.PHONY: all clean
