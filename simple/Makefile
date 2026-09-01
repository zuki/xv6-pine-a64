GCCPREFIX := /opt/homebrew/bin/aarch64-elf-

CC := $(GCCPREFIX)gcc
LD := $(GCCPREFIX)ld
AS := $(GCCPREFIX)as

OBJCOPY := $(GCCPREFIX)objcopy

RM := rm -f

CFLAGS := -Wall -nostdlib -g

OBJ := hello.o startup.o

.PHONY: all

all: hello.img

hello.img: $(OBJ) Makefile
	$(LD) $(OBJ) -T link.ld -o hello.elf
	$(OBJCOPY) -O binary hello.elf $@

%.o: %.s
	$(AS) -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	$(RM) hello.img hello.elf $(OBJ)