# Architecture

## Memory layout

```
0xFFFFFFFF80000000  ← kernel text/data (higher half)
0xFFFFFFFF80000000 + ELF size
        ...
0x0000000000000000  ← user space (future)
```

The kernel is linked at `0xFFFFFFFF80000000` (`-mcmodel=kernel`). GRUB loads the
ELF at low physical addresses; the linker script sets up the page table so the
kernel immediately runs at the higher-half virtual address.

## Module system

Every subsystem lives in `src/MODULE/` and exports exactly three files:

```
MODULE_internal.h   # constants, private structs — never included outside MODULE/
MODULE.h            # public API — the only file other modules may include
MODULE.c            # implementation
```

Modules register themselves in `src/core/module_registry.c`. Init order is
explicit and matters — each module is init'd in the order it appears in the
`modules[]` table.

```c
kernel_module_t mod_foo = {
    .name     = "foo",
    .init     = foo_init,   // returns 0 on success
    .dump     = foo_dump,   // prints debug state via klog
    .shutdown = NULL,
};
```

## Dependency order

```
lib → serial/vga → gdt/idt/pic → pmm → vmm → heap
    → timer/keyboard/pci
    → ata → vfs → tmpfs/devfs
    → process → scheduler → syscall
    → elf → userspace
    → net (e1000 → tcp/ip stack)
```

## Logging

```c
klog(LOG_INFO,  "[module] message %d", value);
klog(LOG_DEBUG, "[module] detail");
klog(LOG_WARN,  "[module] something odd");
klog(LOG_ERROR, "[module] non-fatal error");
```

All log lines go to COM1 (serial). Format: `[LEVEL] [module] message`.

## PANIC

```c
PANIC("unexpected state: %d", val);
```

Prints file, line, all registers, and a stack trace, then halts all CPUs with
`cli; hlt`. Never returns.

## Coding rules

- Public functions prefixed by module name: `pmm_alloc`, `vfs_open`, `tcp_connect`
- Magic numbers in `MODULE_internal.h` as `#define NAME value`
- Structs use `typedef struct { ... } name_t;`
- 0 = success, negative = error (errno-style)
- Always NULL-check before deref
- No hosted libc headers — not even `<stdint.h>` (we have `src/lib/types.h`)
- No `printf` in modules — use `klog`
- Zero warnings policy: the build must emit no warnings at `-Wall -Wextra`
