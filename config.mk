CONFIG_FAT := 1

ARCH := aarch64
CROSS := aarch64-none-elf-
CC := $(CROSS)gcc
LD := $(CROSS)ld
READELF := $(CROSS)readelf
OBJDUMP := $(CROSS)objdump
OBJCOPY := $(CROSS)objcopy
STRIP := $(CROSS)strip

