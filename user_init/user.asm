
user.out:     file format elf64-littleaarch64


Disassembly of section .text:

0000000000000000 <.text>:
   0:	a9bb7bfd 	stp	x29, x30, [sp, #-80]!
   4:	910003fd 	mov	x29, sp
   8:	d28c45e0 	mov	x0, #0x622f                	// #25135
   c:	f2adcd20 	movk	x0, #0x6e69, lsl #16
  10:	f2ce65e0 	movk	x0, #0x732f, lsl #32
  14:	f2e00d00 	movk	x0, #0x68, lsl #48
  18:	f90023e0 	str	x0, [sp, #64]
  1c:	d28c45e0 	mov	x0, #0x622f                	// #25135
  20:	f2adcd20 	movk	x0, #0x6e69, lsl #16
  24:	f2ce65e0 	movk	x0, #0x732f, lsl #32
  28:	f2e00d00 	movk	x0, #0x68, lsl #48
  2c:	f9001fe0 	str	x0, [sp, #56]
  30:	9100e3e0 	add	x0, sp, #0x38
  34:	f90017e0 	str	x0, [sp, #40]
  38:	f9001bff 	str	xzr, [sp, #48]
  3c:	90000000 	adrp	x0, 0x0
  40:	9104e001 	add	x1, x0, #0x138
  44:	910063e0 	add	x0, sp, #0x18
  48:	f9400022 	ldr	x2, [x1]
  4c:	f8405021 	ldur	x1, [x1, #5]
  50:	f9000002 	str	x2, [x0]
  54:	f8005001 	stur	x1, [x0, #5]
  58:	910063e0 	add	x0, sp, #0x18
  5c:	52800043 	mov	w3, #0x2                   	// #2
  60:	52800002 	mov	w2, #0x0                   	// #0
  64:	aa0003e1 	mov	x1, x0
  68:	12800c60 	mov	w0, #0xffffff9c            	// #-100
  6c:	94000026 	bl	0x104
  70:	7100001f 	cmp	w0, #0x0
  74:	540001aa 	b.ge	0xa8  // b.tcont
  78:	910063e0 	add	x0, sp, #0x18
  7c:	d2802003 	mov	x3, #0x100                 	// #256
  80:	52840002 	mov	w2, #0x2000                	// #8192
  84:	aa0003e1 	mov	x1, x0
  88:	12800c60 	mov	w0, #0xffffff9c            	// #-100
  8c:	94000021 	bl	0x110
  90:	910063e0 	add	x0, sp, #0x18
  94:	52800043 	mov	w3, #0x2                   	// #2
  98:	52800002 	mov	w2, #0x0                   	// #0
  9c:	aa0003e1 	mov	x1, x0
  a0:	12800c60 	mov	w0, #0xffffff9c            	// #-100
  a4:	94000018 	bl	0x104
  a8:	52800000 	mov	w0, #0x0                   	// #0
  ac:	9400001c 	bl	0x11c
  b0:	52800000 	mov	w0, #0x0                   	// #0
  b4:	9400001a 	bl	0x11c
  b8:	9100a3e1 	add	x1, sp, #0x28
  bc:	910103e0 	add	x0, sp, #0x40
  c0:	d2800002 	mov	x2, #0x0                   	// #0
  c4:	9400000d 	bl	0xf8
  c8:	b9004fe0 	str	w0, [sp, #76]
  cc:	b9404fe0 	ldr	w0, [sp, #76]
  d0:	7100001f 	cmp	w0, #0x0
  d4:	5400006a 	b.ge	0xe0  // b.tcont
  d8:	b9404fe0 	ldr	w0, [sp, #76]
  dc:	94000013 	bl	0x128
  e0:	d503201f 	nop
  e4:	a8c57bfd 	ldp	x29, x30, [sp], #80
  e8:	d65f03c0 	ret
  ec:	d2800808 	mov	x8, #0x40                  	// #64
  f0:	d4000001 	svc	#0x0
  f4:	d65f03c0 	ret
  f8:	d2801ba8 	mov	x8, #0xdd                  	// #221
  fc:	d4000001 	svc	#0x0
 100:	d65f03c0 	ret
 104:	d2800708 	mov	x8, #0x38                  	// #56
 108:	d4000001 	svc	#0x0
 10c:	d65f03c0 	ret
 110:	d2800428 	mov	x8, #0x21                  	// #33
 114:	d4000001 	svc	#0x0
 118:	d65f03c0 	ret
 11c:	d28002e8 	mov	x8, #0x17                  	// #23
 120:	d4000001 	svc	#0x0
 124:	d65f03c0 	ret
 128:	d2800ba8 	mov	x8, #0x5d                  	// #93
 12c:	d4000001 	svc	#0x0
 130:	d65f03c0 	ret
