-include ../config.mk

MUSL = /Users/zuki/musl
LIBC_A = $(MUSL)/lib/libc.a
LIBC_SPEC = $(MUSL)/lib/musl-gcc.specs
LIBC_LIBS = $(wildcard $(MUSL)/lib/*)
LIBC_INCS = $(wildcard $(MUSL)/include/**/*)
LIBC_DEPS = $(LIBC_LIBS) $(LIBC_INCS)

OBJ = ../obj/usr
#LIBC_SPEC_OUT = $(OBJ)/musl-gcc.specs

USR_CC := $(MUSL)/bin/musl-gcc

# -z max-page-size: https://stackoverflow.com/questions/33005638/how-to-change-alignment-of-code-segment-in-elf
CFLAGS = -std=gnu99 -O3 -MMD -MP -static -z max-page-size=4096 \
  -fno-omit-frame-pointer -Iinc

BIN := $(OBJ)/bin
SRC := src

USER_DIRS := $(shell find $(SRC) -maxdepth 1 -mindepth 1 -type d)
USER_BINS := $(USER_DIRS:$(SRC)/%=$(BIN)/%)

all:
	$(MAKE) $(USER_BINS)

$(OBJ)/%.c.o: %.c $(LIBC_DEPS)
	mkdir -p $(dir $@)
	$(USR_CC) $(CFLAGS) -c -o $@ $<
	
# Same as `bin/NAME: $(OBJ)/NAME/*.c.o $(LIBC_A)`
# NOTE: Static library should be at last.
.SECONDEXPANSION:
$(BIN)/%: $$(addsuffix .o, $$(addprefix $(OBJ)/, $$(shell find $(SRC)/% -name "*.c")))
	mkdir -p $(dir $@)
	$(USR_CC) $(CFLAGS) -o $@ $^
	$(OBJDUMP) -S -d $@ > $(OBJ)/src/$(notdir $@)/$(notdir $@).asm
	$(OBJDUMP) -x $@ > $(OBJ)/src/$(notdir $@)/$(notdir $@).hdr
	$(STRIP) --strip-debug $@

clean:
	rm -rf $(OBJ) $(BIN)

.PHONY: all clean
