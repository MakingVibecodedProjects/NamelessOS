[← 33](PROMPT_33.md) | [index](README.md) | **34** |

---

# PROMPT_34 — Phase 8 race-fix: mmap_next per-process, httpd kernel-launched

**Session date:** 2026-03-29
**Status when starting:** Phase 8 complete but intermittent: sometimes httpd started, sometimes shell started, then page fault on second run
**Status when done:** All Phase 8 features stable — deterministic boot, httpd and shell both start every time, no crashes

## What was fixed

### Root cause 1 — `mmap_next` was a global static in `dynlink.c`

When two processes (httpd + shell) ran `sys_mmap` concurrently, they incremented the same global bump pointer.  Each process has its own PML4, so both processes mapped pages at the same virtual addresses inside their respective address spaces — but the pointer advanced only once, so subsequent allocations overlapped.  Also, after `execve`, the old image's bump value carried over into the new image.

**Fix in `src/dynlink/dynlink.c`:**
- Removed `static u64 mmap_next = MMAP_BASE;`
- Removed stale pre-process check `if (mmap_next + pages * PAGE_SIZE > MMAP_LIMIT)` (referred to removed global)
- `sys_mmap` now uses `process_current()->mmap_next`; lazily initialises to `MMAP_BASE` on first use

**Fix in `src/process/process.h`:**
- Added `u64 mmap_next;` field to `process_t` (per-process anonymous mmap bump pointer)

**Fix in `src/syscall/syscall.c` `sys_execve`:**
- Added `p->mmap_next = 0;` when replacing the address space, so the first mmap in the new image resets to `MMAP_BASE`

### Root cause 2 — httpd and shell forked simultaneously from userspace `init`

`init` forked both httpd and shell before waiting for either.  Both children called `execve` on COW clones of the same PML4.  Concurrent `vmm_fork_pml4` / `vmm_destroy_user_pml4` on sibling clones of the same parent corrupted page-table reference counts, causing a page fault in `scheduler_yield` roughly half the time.

**Fix in `src/init_launch/init_launch.c`:**
- Added `launch_user_process(name, elf_data, elf_size)` helper: allocates a fresh independent PML4 via `vmm_create_user_pml4`, loads the ELF, allocates a user stack, allocates a kernel stack via `kmalloc(KSTACK_SIZE)` (higher-half, present in every PML4), builds the `[SS, user_RSP, RFLAGS, CS, RIP]` iretq frame, calls `kthread_create` + overrides kstack/pml4/ctx, calls `scheduler_add`
- At the end of `init_launch_impl` (after `scheduler_add(p)` for PID 1): calls `launch_user_process("httpd", httpd_elf_data, httpd_elf_size)` — httpd gets its own clean PML4, independent of init's

**Fix in `userspace/programs/init/init.c`:**
- Removed httpd fork entirely; init now only runs the shell respawn loop
- httpd is launched by the kernel before init ever runs; the two processes have no shared ancestry at the PML4 level

### Stale-object build gotcha

Adding `mmap_next` to `process_t` changed the struct layout (8 bytes wider).  WSL2 clock skew meant `make` considered many `.o` files up to date — the scheduler's `context_switch` was still compiled against the old layout, accessing `process_t::next` at offset `0x98` (152) instead of `0xa0` (160).  This produced a NULL dereference at `scheduler_yield+0x30`.  Fixed by `make clean && make iso` after the struct change.

## Verified build output

```
[INFO] [dynlink] mmap/munmap ready (anon window 0x10000000-0x50000000)
[INFO] [init] launching PID 1 entry=0x400000 usp=0x7ffff000
[INFO] [init] launched 'httpd' pid=2 entry=0x400000
[INFO] [kernel] All modules initialized.
[INFO] [kernel] Interrupts enabled.
[INFO] [tcp] listening on port 80 (id=0)
[DEBUG] [process] forked pid=1 → child pid=3 ...
[INFO] [elf] loaded 2 segment(s), entry=0x400000   ← shell
```
(clean boot, no page faults, both processes run independently in the round-robin scheduler; QEMU idles at shell prompt waiting for keyboard input)

## Known traps discovered

- **`mmap_next` must be per-process, not a global** — concurrent mmap from two processes advances the same pointer into the same VA range inside different address spaces; also survives execve incorrectly if not zeroed on address-space replacement
- **Launch independent daemons from the kernel, not userspace fork** — sibling `execve` calls on COW clones of the same parent PML4 corrupt page-table state; give each daemon its own PML4 from `vmm_create_user_pml4` at kernel launch time
- **`make clean` is mandatory after adding a struct field when WSL2 timestamps are stale** — the scheduler's `next` pointer offset changed from `0x98` to `0xa0`; the stale `.o` dereferenced the wrong offset and faulted at `scheduler_yield+0x30` with CR2=0x8

---

[← 33](PROMPT_33.md) | [index](README.md) | **34** |
