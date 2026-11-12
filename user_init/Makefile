-include ../config.mk

icode: user.c user_sys.S
	$(CC) -std=gnu11 -mcpu=cortex-a53 -mtune=cortex-a53 -nostdinc -static -c user.c -o user.o -I ../inc
	$(CC) -std=gnu11 -mcpu=cortex-a53 -mtune=cortex-a53 -nostdinc -static -c user_sys.S -o user_sys.o -I ../inc
	$(LD) -N -Ttext=0x400000 -static -o user.out user.o user_sys.o
	$(STRIP) user.out
	$(OBJCOPY) -S -O binary user.out user
	$(OBJDUMP) -S user.out > user.asm
	xxd -i user initcode.inc
