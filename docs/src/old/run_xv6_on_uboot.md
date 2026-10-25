```
U-Boot> fatload mmc 0:1 0x80000 xv6.img
282820 bytes read in 14 ms (19.3 MiB/s)
U-Boot> md.b 0x80000 20
00080000: 60 01 00 10 21 00 80 d2 42 00 80 d2 63 00 80 d2  `...!...B...c...
00080010: 09 1b 80 d2 20 79 21 f8 20 79 22 f8 20 79 23 f8  .... y!. y". y#.
U-Boot> go 0x80000
## Starting application at 0x00080000 ...
```

```
U-Boot> fatload mmc 0:1 0x80000 xv6.img
282820 bytes read in 13 ms (20.7 MiB/s)
U-Boot> showvar $kernel_addr_r         
## Error: "0x00080000" not defined
U-Boot> booti 0x80000 - -
Bad Linux ARM64 Image magic!    // arch/arm/lib/image.cで出力
U-Boot> help bootm
bootm - boot application image from memory

Usage:
bootm [addr [arg ...]]
    - boot application image stored in memory
        passing arguments 'arg ...'; when booting a Linux kernel,
        'arg' can be the address of an initrd image
        When booting a Linux kernel which requires a flat device-tree
        a third argument is required which is the address of the
        device-tree blob. To boot that kernel without an initrd image,
        use a '-' for the second argument. If you do not pass a third
        a bd_info struct will be passed instead

Sub-commands to do part of the bootm sequence.  The sub-commands must be
issued in the order below (it's ok to not issue all sub-commands):        
        start [addr [arg ...]]                                            
        loados  - load OS image                                           
        ramdisk - relocate initrd, set env initrd_start/initrd_end        
        fdt     - relocate flat device tree                               
        cmdline - OS specific command line processing/setup               
        bdt     - OS specific bd_info processing                          
        prep    - OS specific prep before relocation or go                
        go      - start OS                                                
U-Boot> bootm 0x80000                                                     
Wrong Image Format for bootm command                                      
ERROR: can't get kernel image!                                            
U-Boot> fatload mmc 0:1 0xFFFF000000080000 xv6.img                        
** Reading file would overwrite reserved memory **                        
Failed to load 'xv6.img'                                                  
U-Boot> md.b 0x80000 20                                                   
00080000: 60 01 00 10 21 00 80 d2 42 00 80 d2 63 00 80 d2  `...!...B...c...
00080010: 09 1b 80 d2 20 79 21 f8 20 79 22 f8 20 79 23 f8  .... y!. y". y#.
U-Boot> go 0x80000                                                        
## Starting application at 0x00080000 ...
S??O[?X??Q?kJ??R?T?XpJs??T?R?P????I?C?S?S?C?s@Y?N?MV?Rp?q?RIZ?/B?ܣRQ?qKX?ԫRQ?qK?
9L?zҊ??Vw?Y?ZHQ?Y?H???XQ?[?S?JZ
?H?i?SCk?????SRA?X?Ҳ؇???BJ?P???
```
