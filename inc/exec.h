#ifndef INC_EXEC_H
#define INC_EXEC_H

#include <types.h>
#include <memlayout.h>
#include <elf.h>
//#include <linux/capability.h>

#define EM_AARCH64          183

#define ELF_MIN_ALIGN       4096
#define ELF_PAGESTART(_v)   ((_v) & ~(uint64_t)(ELF_MIN_ALIGN-1))
#define ELF_PAGEOFFSET(_v)  ((_v) & (uint64_t)(ELF_MIN_ALIGN-1))
#define ELF_PAGEALIGN(_v)   (((_v) + ELF_MIN_ALIGN - 1) & ~(uint64_t)(ELF_MIN_ALIGN - 1))
// ET_DYNプログラムが実行された場合にロードされる場所である。
// 典型的な使い方は、新しいバージョンのローダーをテストするために
// "./ld.so someprog"を呼び出すことである。 ldが"exec"するプログラムの
// 邪魔にならないようにし、brkのための十分なスペースがあることを
// 確認する必要がある。
#define ELF_EXEC_PAGESIZE   4096
#define BINPRM_BUF_SIZE     128

#define BAD_ADDR(x)         ((uint64_t)(x) > USER_TOP)
// AUXVの個数
#define ELF_CHECK_ARCH(x)   ((x)->machine == EM_AARCH64)

#define ELF_CHECK_E_IDNET(elf) \
        (   (elf)->e_ident[EI_MAG0] == ELFMAG0 \
         && (elf)->e_ident[EI_MAG1] == ELFMAG1 \
         && (elf)->e_ident[EI_MAG2] == ELFMAG2 \
         && (elf)->e_ident[EI_MAG3] == ELFMAG3 )

// 以下はaarch64版ubuntuの値を使用: arch/arm64/include/uapi/asm/hwcap.h
// 0x887 = FP, ASIMD, EVTSTRM, PMULL, SHA1, SHA2, ATOMICS, FPHP
#define ELF_HWCAP           0x887
#define ELF_HWCAP2          0
#define ELF_PLATFORM        "aarch64"

#ifndef elf_addr_t
#define elf_addr_t          uint64_t
#define elf_caddr_t         char *
#endif

int exec(char *path, char *argv[], char *envp[]);

#endif
