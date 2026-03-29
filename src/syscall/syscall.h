#ifndef SYSCALL_H
#define SYSCALL_H

#include "../lib/types.h"
#include "../lib/module.h"

/* Initialise SYSCALL/SYSRET machinery: program STAR/LSTAR/SFMASK MSRs,
   enable SCE in EFER.  Returns 0 on success. */
int syscall_init(void);

/* Register a handler for syscall number nr.
   May be called after syscall_init() by other modules. */
void syscall_register(u32 nr, u64 (*fn)(u64,u64,u64,u64,u64,u64));

/* Update the kernel stack top used by syscall_entry.asm on SYSCALL entry.
   Must be called alongside gdt_set_tss_rsp0() whenever the current
   process's kernel stack changes (init_launch, execve, context switch). */
void syscall_set_kernel_rsp(u64 rsp);

/* Module descriptor — registered in module_registry after mod_scheduler. */
extern kernel_module_t mod_syscall;

#endif /* SYSCALL_H */
