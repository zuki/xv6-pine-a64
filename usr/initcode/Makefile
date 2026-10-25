-include ../../config.mk

icode: initcode.S
	$(CC) -std=gnu11 -mcpu=cortex-a53 -mtune=cortex-a53 -nostdinc -static -c initcode.S -o initcode.o -I ../../inc
	$(LD) -N -e _start -Ttext=0 -static -o initcode.out initcode.o
	$(STRIP) initcode.out
	$(OBJCOPY) -S -O binary initcode.out initcode
	$(OBJDUMP) -S initcode.out > initcode.asm
	xxd -i initcode initcode.inc
