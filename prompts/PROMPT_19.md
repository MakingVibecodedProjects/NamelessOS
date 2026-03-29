[← 18](PROMPT_18.md) | [index](README.md) | **19** | [20 →](PROMPT_20.md)

---

# PROMPT_19 — Phase 6 Step 3: Userspace libc

**Session date:** 2026-03-29
**Status when starting:** Phase 6 Step 2 complete (ELF64 loader live)
**Status when done:** Phase 6 Step 3 complete — `userspace/libc/` builds `libc.a` + `crt0.o`; zero warnings

## What was built

- `userspace/libc/include/stdint.h` — u8–u64, i8–i64, uintptr_t, size_t, ssize_t, min/max constants
- `userspace/libc/include/stddef.h` — NULL, offsetof, size_t, ptrdiff_t
- `userspace/libc/include/sys/types.h` — pid_t, uid_t, gid_t, off_t, mode_t
- `userspace/libc/include/string.h` — memset/memcpy/memmove/memcmp/strlen/strcmp/strncmp/strcpy/strncpy/strcat/strchr
- `userspace/libc/include/unistd.h` — STDIN/STDOUT/STDERR_FILENO, read/write/close/getpid/fork/_exit
- `userspace/libc/include/stdlib.h` — malloc/calloc/realloc/free/exit/atoi/atol
- `userspace/libc/include/stdio.h` — printf/vprintf/puts/putchar/sprintf/snprintf/vsprintf/vsnprintf
- `userspace/libc/crt0.asm` — `_start`: rdi=argc, rsi=argv already set by kernel; calls `main(argc, argv, NULL)`; passes return value to `exit`
- `userspace/libc/syscall.asm` — raw `syscall` instruction wrappers for read/write/open/close/brk/getpid/fork/execve/_exit/waitpid; `r10 ← rcx` for 4-arg calls
- `userspace/libc/string.c` — all string.h functions implemented
- `userspace/libc/malloc.c` — `brk`-based bump allocator; `malloc/calloc/realloc` (free is no-op); `exit` → `_exit`; `atoi/atol`
- `userspace/libc/printf.c` — `vsnprintf` supports `%c %s %d %i %u %x %X %p %%`; `printf/vprintf/puts/putchar` write to fd 1 via `write` syscall
- `userspace/libc/Makefile` — cross-compiles with `x86_64-linux-gnu-gcc -ffreestanding -nostdinc`; adds GCC built-in include path (for `stdarg.h`); builds `libc.a` and `crt0.o` into `build/userspace/libc/`
- Top-level `Makefile` — added `userspace` target (`make -C userspace/libc`) and wired it into `clean`

## Key decisions

- **`-I $(GCC_INC)` for `stdarg.h`** — `stdarg.h` is a GCC compiler built-in; with `-nostdinc` it's stripped; the path from `gcc -print-file-name=include` brings it back without any hosted libc headers
- **`syscall.asm` uses `r10 ← rcx` for 4-arg calls** — the SYSCALL instruction clobbers rcx; Linux ABI passes arg4 in r10 instead; C callers put it in rcx per SysV ABI so the wrapper must move it
- **`brk_syscall` exported separately from `malloc`** — `malloc.c` calls `brk_syscall` (the raw syscall); `brk` as a POSIX function is not exposed so there's no confusion with the future `sys_brk` stub
- **Bump allocator for Phase 6** — `free` is a no-op; sufficient for `init` and `shell` until a proper heap is needed in Phase 8
- **`mcmodel=small`** — userspace programs link at low addresses (< 2 GB); `-mcmodel=kernel` is only for the kernel's high-half address space

## Verified build output

```
AR: ../../build/userspace/libc/libc.a
AS: ../../build/userspace/libc/crt0.o
```
(zero warnings, zero errors; kernel ISO build unaffected)

## Next session

[PROMPT_20 →](PROMPT_20.md) — Phase 6 Step 4: `userspace/programs/init` (PID 1) + `userspace/programs/shell` (cd/ls/cat/exec).

---

[← 18](PROMPT_18.md) | [index](README.md) | **19** | [20 →](PROMPT_20.md)
