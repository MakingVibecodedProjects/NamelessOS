[← 32](PROMPT_32.md) | [index](README.md) | **33** |

---

# PROMPT_33 — Phase 8 Steps 4+5: dynlink + httpd

**Session date:** 2026-03-29
**Status when starting:** Phase 8 Step 3 complete (init launch, shell, scheduler stable)
**Status when done:** Phase 8 complete — dynlink mmap/munmap syscalls, HTTP/1.0 server on port 80, roadmap finished

## What was built

### `src/dynlink/` — new module (Step 4)
- `dynlink_internal.h` — `PROT_*`, `MAP_*` flags; `MMAP_BASE=0x10000000`, `MMAP_LIMIT=0x50000000`; `MAP_FAILED`
- `dynlink.h` — `mod_dynlink` extern
- `dynlink.c`:
  - `sys_mmap(addr, length, prot, flags, fd, offset)` — MAP_ANONYMOUS only; rounds up to page; allocates PMM frames, zeros via `PHYS_TO_VIRT`, maps with `vmm_map_user_page(p->pml4_phys, ...)` into current process; bump pointer at `mmap_next`
  - `sys_munmap(addr, length)` — stub returns 0 (bump allocator semantics)
  - Registers syscalls 9 (SYS_MMAP) and 11 (SYS_MUNMAP) via `syscall_register`
  - `dynlink_init` logs: `[dynlink] mmap/munmap ready (anon window 0x10000000-0x50000000)`
- `src/core/module_registry.c` — added `&mod_dynlink` between `&mod_smp` and `&mod_init_launch`

### `userspace/programs/httpd/httpd.c` — new program (Step 5)
- `socket(AF_INET, SOCK_STREAM, 0)` → fd ≥ 64
- `bind` on port 80, `listen(BACKLOG=4)`
- Accept loop: `accept()` → if -EAGAIN do `read(STDIN_FILENO, &dummy, 0)` to yield, else `handle_client(cfd)`
- `handle_client`: reads until `\r\n\r\n` or `\n\n` in receive buffer; sends HTTP/1.0 response with `Content-Type: text/html`, `Content-Length`, `Connection: close`; serves static HTML page
- Verified: TCP connection table shows `listening on port 80 (id=0)` in serial log

### `userspace/libc/syscall.asm` — socket wrappers added
- New syscall numbers: `SYS_MMAP=9`, `SYS_MUNMAP=11`, `SYS_SOCKET=41`, `SYS_CONNECT=42`, `SYS_ACCEPT=43`, `SYS_SENDTO=44`, `SYS_RECVFROM=45`, `SYS_SHUTDOWN=48`, `SYS_BIND=49`, `SYS_LISTEN=50`
- New wrappers: `mmap` (SYSCALL6), `munmap` (SYSCALL2), `socket` (SYSCALL3), `connect` (SYSCALL3), `accept` (SYSCALL3), `bind` (SYSCALL3), `listen` (SYSCALL2), `shutdown` (SYSCALL2)
- `send` — custom: remaps 4th arg (flags) to r10, sets r8/r9=0, calls SYS_SENDTO
- `recv` — custom: remaps to SYS_RECVFROM with NULL addr/addrlen
- `sendto` / `recvfrom` — SYSCALL6 (r10 = 4th arg)

### `userspace/libc/include/sys/socket.h` — new header
- `struct sockaddr_in` (packed): `sin_family`, `sin_port`, `sin_addr`, `sin_zero[8]`
- `AF_INET=2`, `SOCK_STREAM=1`, `SOCK_DGRAM=2`
- `htons`/`ntohs`/`htonl`/`ntohl` as static inline byte-swap functions
- Declarations for all socket functions

### `userspace/programs/init/init.c` — updated
- Before the shell loop: forks pid for httpd (`fork` + `execve("/bin/httpd", ...)`, parent does NOT waitpid — httpd runs as background daemon)
- Shell loop unchanged

### `src/syscall/syscall.c` — httpd path added to execve
- Added `extern const u8 httpd_elf_data[]; extern const u32 httpd_elf_size;`
- Extended path lookup: sequential `strcmp`-style check for `/bin/shell` then `/bin/httpd`

### `Makefile` — httpd_elf embed
- `userspace` target now generates `src/init_launch/httpd_elf.c` via `xxd -i`
- `HTTPD_ELF` variable and `src/init_launch/httpd_elf.c` rule added (same pattern as shell_elf.c)

### `userspace/programs/Makefile` — httpd target added
- Compiles `httpd/httpd.c` → `$(BUILD)/httpd.o` → links with crt0 + libc.a

### `src/init_launch/httpd_elf.c` — stub committed
- Placeholder `httpd_elf_data[] = { 0 }` so the build works before `make userspace`; overwritten by make

## Key decisions

- **mmap as bare PMM bump** — no VMA tracking, no munmap reclaim; sufficient for malloc's `brk`-fallback and future dlopen; a real implementation would need a red-black tree of VMAs in process_t
- **httpd as static ELF, no real .so** — "dynlink" means the kernel provides the mmap primitive; the actual "dynamic linking" step (loading .so, resolving PLT) is deferred; httpd is statically linked, which is correct for an embedded kernel that has no persistent filesystem yet
- **Background httpd** — init forks httpd without waitpid; httpd runs indefinitely in its own process; init continues to respawn the shell; both coexist in the round-robin scheduler
- **Accept-yield loop** — `accept()` returns -EAGAIN if no connection ready; httpd calls `read(STDIN_FILENO, &dummy, 0)` (zero-length read, immediately returns 0) to yield the scheduler tick without blocking; avoids busy-spin while waiting for connections
- **send/recv as sendto/recvfrom wrappers** — our kernel socket layer only implements sendto/recvfrom; `send`/`recv` in userspace just pass NULL for addr/addrlen

## Verified build output

```
[INFO] [dynlink] mmap/munmap ready (anon window 0x10000000-0x50000000)
[INFO] [init] launching PID 1 entry=0x400000 usp=0x7ffff000
[DEBUG] [process] forked pid=1 → child pid=2 ...
[DEBUG] [process] forked pid=1 → child pid=3 ...
[INFO] [elf] loaded 2 segment(s), entry=0x400000   (httpd)
[INFO] [elf] loaded 2 segment(s), entry=0x400000   (shell)
[INFO] [tcp] listening on port 80 (id=0)
```
(zero warnings kernel + userspace; all previous module lines unchanged)

## Known traps discovered

- **`accept()` returning EAGAIN needs a yield** — a tight `for(;;) { accept(); }` loop without a yield burns 100% CPU and starves the timer IRQ, so the scheduler never gets to run the TCP stack to complete the handshake; use a zero-length read or any other yield point in the retry loop
- **userspace `<string.h>` must be explicitly included** — even though libc.a contains `strlen`, the `-nostdinc` flag means the compiler has no implicit string.h; without `#include <string.h>` you get an implicit declaration warning that the zero-warnings policy rejects

---

[← 32](PROMPT_32.md) | [index](README.md) | **33** |
