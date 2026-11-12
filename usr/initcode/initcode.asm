
initcode.out:     file format elf64-littleaarch64


Disassembly of section .text:

0000000000400000 <.text>:
  400000:	d2801ba8 	mov	x8, #0xdd                  	// #221
  400004:	10000080 	adr	x0, 0x400014
  400008:	d2800001 	mov	x1, #0x0                   	// #0
  40000c:	d2800002 	mov	x2, #0x0                   	// #0
  400010:	d4000001 	svc	#0x0
  400014:	6e69622f 	rsubhn2	v15.8h, v17.4s, v9.4s
  400018:	696e692f 	ldpsw	x15, x26, [x9, #-144]
  40001c:	00000074 	udf	#116
