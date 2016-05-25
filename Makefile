CC = gcc
CFLAGS += -DHAVE_CONFIG_H -Wno-parentheses $(shell pkg-config --cflags bash)
LDFLAGS += -dynamiclib -dynamic -undefined dynamic_lookup

builtins = asort md5 csv

all: $(builtins)
	@printf '\nDone. To load the builtins, run:\n\n'
	@for x in $(builtins); do printf 'enable -f %s %s\n' "$$x" "$$x"; done

clean:
	rm -f $(builtins)

.PHONY: all clean
