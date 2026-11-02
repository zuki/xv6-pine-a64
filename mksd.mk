V6_IMG := obj/v6.img

USR_FILES := $(shell find obj/usr/bin -type f)

.DELETE_ON_ERROR: $(FS_IMG)

all: $(V6_IMG)

mkfs/mkfs: mkfs/mkfs.c
	cc -o mkfs/mkfs -Imkfs/ mkfs/mkfs.c

$(V6_IMG): mkfs/mkfs test.txt $(USR_FILES)
	echo $^
	./mkfs/mkfs $@ test.txt $(USR_FILES)

clean:
	rm -f mkfs/mkfs obj/v6.img
