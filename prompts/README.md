# NamelessOS — Session Prompts

Every file here is an auto-generated build log written by Claude Code at the
end of each session. They form a chain — each one records what was built and
links forward to the next.

---

## The Chain

| # | Module | Phase | Status |
|---|--------|-------|--------|
| [PROMPT_1](PROMPT_1.md) | Kernel scaffold — VGA, serial, module registry | Phase 1 | done |
| [PROMPT_2](PROMPT_2.md) | GDT | Phase 1 | done |
| [PROMPT_3](PROMPT_3.md) | IDT + exception handlers | Phase 1 | done |
| [PROMPT_4](PROMPT_4.md) | PIC + IRQ routing | Phase 1 | done |
| [PROMPT_5](PROMPT_5.md) | PMM — physical memory manager | Phase 2 | done |
| [PROMPT_6](PROMPT_6.md) | Heap — slab allocator | Phase 2 | done |
| [PROMPT_7](PROMPT_7.md) | Timer — PIT 1000 Hz | Phase 3 | done |
| [PROMPT_8](PROMPT_8.md) | Keyboard — PS/2 | Phase 3 | done |
| [PROMPT_9](PROMPT_9.md) | PCI — bus enumeration | Phase 3 | done |
| [PROMPT_10](PROMPT_10.md) | ATA — PIO 28-bit LBA | Phase 4 | done |
| [PROMPT_11](PROMPT_11.md) | VFS — abstract layer | Phase 4 | done |
| [PROMPT_12](PROMPT_12.md) | tmpfs — in-memory filesystem | Phase 4 | done |
| [PROMPT_13](PROMPT_13.md) | devfs — /dev/null, /dev/zero | Phase 4 | done |
| [PROMPT_14](PROMPT_14.md) | Process — TCB, kthread_create, idle pid=0 | Phase 5 | done |
| [PROMPT_15](PROMPT_15.md) | Scheduler — round-robin, context_switch | Phase 5 | done |
| [PROMPT_16](PROMPT_16.md) | Syscall — SYSCALL/SYSRET, dispatch table, 12 syscalls | Phase 5 | done |
| [PROMPT_17](PROMPT_17.md) | Per-process page tables, COW fork, CR3 switch | Phase 6 | done |
| [PROMPT_18](PROMPT_18.md) | ELF64 loader — PT_LOAD mapper, entry point | Phase 6 | done |
| [PROMPT_19](PROMPT_19.md) | Userspace libc — crt0, syscall wrappers, malloc, printf | Phase 6 | done |
| [PROMPT_20](PROMPT_20.md) | init (PID 1) + shell (cd/ls/cat/exec) | Phase 6 | done |
| [PROMPT_21](PROMPT_21.md) | e1000 NIC driver — PCI detect, DMA TX/RX rings, polled mode | Phase 7 | done |
| [PROMPT_22](PROMPT_22.md) | Ethernet — frame parser/builder, ethertype dispatch | Phase 7 | done |

> Files are written automatically by Claude Code. Do not edit manually.

**Start reading:** [PROMPT_1 →](PROMPT_1.md)
