
user.out:     file format elf64-littleaarch64


Disassembly of section .text:

0000000000400000 <.text>:
  400000:	a9bb7bfd 	stp	x29, x30, [sp, #-80]!
  400004:	910003fd 	mov	x29, sp
  400008:	d28c45e0 	mov	x0, #0x622f                	// #25135
  40000c:	f2adcd20 	movk	x0, #0x6e69, lsl #16
  400010:	f2ce65e0 	movk	x0, #0x732f, lsl #32
  400014:	f2e00d00 	movk	x0, #0x68, lsl #48
  400018:	f90023e0 	str	x0, [sp, #64]
  40001c:	d28c45e0 	mov	x0, #0x622f                	// #25135
  400020:	f2adcd20 	movk	x0, #0x6e69, lsl #16
  400024:	f2ce65e0 	movk	x0, #0x732f, lsl #32
  400028:	f2e00d00 	movk	x0, #0x68, lsl #48
  40002c:	f9001fe0 	str	x0, [sp, #56]
  400030:	9100e3e0 	add	x0, sp, #0x38
  400034:	f90017e0 	str	x0, [sp, #40]
  400038:	f9001bff 	str	xzr, [sp, #48]
  40003c:	90000000 	adrp	x0, 0x400000
  400040:	9104e001 	add	x1, x0, #0x138
  400044:	910063e0 	add	x0, sp, #0x18
  400048:	f9400022 	ldr	x2, [x1]
  40004c:	f8405021 	ldur	x1, [x1, #5]
  400050:	f9000002 	str	x2, [x0]
  400054:	f8005001 	stur	x1, [x0, #5]
  400058:	910063e0 	add	x0, sp, #0x18
  40005c:	52800043 	mov	w3, #0x2                   	// #2
  400060:	52800002 	mov	w2, #0x0                   	// #0
  400064:	aa0003e1 	mov	x1, x0
  400068:	12800c60 	mov	w0, #0xffffff9c            	// #-100
  40006c:	94000026 	bl	0x400104
  400070:	7100001f 	cmp	w0, #0x0
  400074:	540001aa 	b.ge	0x4000a8  // b.tcont
  400078:	910063e0 	add	x0, sp, #0x18
  40007c:	d2802003 	mov	x3, #0x100                 	// #256
  400080:	52840002 	mov	w2, #0x2000                	// #8192
  400084:	aa0003e1 	mov	x1, x0
  400088:	12800c60 	mov	w0, #0xffffff9c            	// #-100
  40008c:	94000021 	bl	0x400110
  400090:	910063e0 	add	x0, sp, #0x18
  400094:	52800043 	mov	w3, #0x2                   	// #2
  400098:	52800002 	mov	w2, #0x0                   	// #0
  40009c:	aa0003e1 	mov	x1, x0
  4000a0:	12800c60 	mov	w0, #0xffffff9c            	// #-100
  4000a4:	94000018 	bl	0x400104
  4000a8:	52800000 	mov	w0, #0x0                   	// #0
  4000ac:	9400001c 	bl	0x40011c
  4000b0:	52800000 	mov	w0, #0x0                   	// #0
  4000b4:	9400001a 	bl	0x40011c
  4000b8:	9100a3e1 	add	x1, sp, #0x28
  4000bc:	910103e0 	add	x0, sp, #0x40
  4000c0:	d2800002 	mov	x2, #0x0                   	// #0
  4000c4:	9400000d 	bl	0x4000f8
  4000c8:	b9004fe0 	str	w0, [sp, #76]
  4000cc:	b9404fe0 	ldr	w0, [sp, #76]
  4000d0:	7100001f 	cmp	w0, #0x0
  4000d4:	5400006a 	b.ge	0x4000e0  // b.tcont
  4000d8:	b9404fe0 	ldr	w0, [sp, #76]
  4000dc:	94000013 	bl	0x400128
  4000e0:	d503201f 	nop
  4000e4:	a8c57bfd 	ldp	x29, x30, [sp], #80
  4000e8:	d65f03c0 	ret
  4000ec:	d2800808 	mov	x8, #0x40                  	// #64
  4000f0:	d4000001 	svc	#0x0
  4000f4:	d65f03c0 	ret
  4000f8:	d2801ba8 	mov	x8, #0xdd                  	// #221
  4000fc:	d4000001 	svc	#0x0
  400100:	d65f03c0 	ret
  400104:	d2800708 	mov	x8, #0x38                  	// #56
  400108:	d4000001 	svc	#0x0
  40010c:	d65f03c0 	ret
  400110:	d2800428 	mov	x8, #0x21                  	// #33
  400114:	d4000001 	svc	#0x0
  400118:	d65f03c0 	ret
  40011c:	d28002e8 	mov	x8, #0x17                  	// #23
  400120:	d4000001 	svc	#0x0
  400124:	d65f03c0 	ret
  400128:	d2800ba8 	mov	x8, #0x5d                  	// #93
  40012c:	d4000001 	svc	#0x0
  400130:	d65f03c0 	ret
