
initcode.out:     file format elf64-littleaarch64


Disassembly of section .text:

0000000000000000 <.text>:
   0:	d2801ba8 	mov	x8, #0xdd                  	// #221
   4:	10000080 	adr	x0, 0x14
   8:	d2800601 	mov	x1, #0x30                  	// #48
   c:	d2800802 	mov	x2, #0x40                  	// #64
  10:	d4000001 	svc	#0x0
  14:	6e69622f 	rsubhn2	v15.8h, v17.4s, v9.4s
  18:	696e692f 	ldpsw	x15, x26, [x9, #-144]
  1c:	50000074 	adr	x20, 0x2a
  20:	3d485441 	ldr	b1, [x2, #533]
  24:	6e69622f 	rsubhn2	v15.8h, v17.4s, v9.4s
  28:	00000000 	udf	#0
  2c:	d503201f 	nop
  30:	00000014 	udf	#20
	...
  40:	0000001f 	udf	#31
	...
