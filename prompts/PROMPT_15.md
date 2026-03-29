[← 14](PROMPT_14.md) | [index](README.md) | **15** | 16 →

---

# PROMPT_15 — Phase 5 Step 2: Scheduler

**Session date:** 2026-03-29
**Status when starting:** Phase 5 Step 1 complete (process table, TCB, idle pid=0, kthread_create ready)
**Status when done:** Phase 5 Step 2 complete — preemptive round-robin scheduler with ASM context switch live

## What was built

- `src/scheduler/scheduler.h` — `scheduler_init()`, `scheduler_yield()`, `scheduler_tick()`, `mod_scheduler`
- `src/scheduler/scheduler.c` — circular singly-linked run queue via `process_t::next`; `scheduler_tick` increments counter and yields every `SCHED_TICKS_PER_SLICE` (10 ms); `scheduler_yield` scans for next READY/RUNNING process, updates states, calls `process_set_current` then `context_switch`
- `src/scheduler/scheduler.asm` — `context_switch(old_ctx, new_ctx)`: saves rbx/rbp/r12–r15/rsp and the return address from `[rsp]` into `*old_ctx`; restores the same from `*new_ctx`; jumps directly to `new_ctx->rip` via `jmp qword [rsi+56]`

## Key decisions

- **Circular list via `process_t::next`** — no separate queue struct; `run_queue` head pointer advances on each yield; `run_queue_add` inserts after head
- **`context_switch` uses `jmp`, not `ret`** — for fresh threads `rip = fn` and `rsp = stack_top-8`; the 8-byte gap holds a zero sentinel so a returning thread faults cleanly; `jmp` avoids popping that sentinel on first switch-in
- **Return address saved from `[rsp]`** — `context_switch` is a normal `call`; the real resume RIP sits as the return address on top of the current stack; reading `[rsp]` before touching RSP captures it correctly
- **`process_set_current` before `context_switch`** — the new thread sees itself as `process_current()` from its very first instruction
- **`old_ctx = &next->ctx` when current is NULL** — guards the boot edge case where `process_current()` returns NULL; dummy save goes into next's own context (harmless)
- **`SCHED_TICKS_PER_SLICE = 10`** — 10 ms time slice at 1000 Hz PIT; tunable constant in `scheduler.c`
- **`.note.GNU-stack` at end of `.asm`** — added after code to suppress linker "executable stack" deprecation warning without breaking the symbol placement

## Verified serial output

```
[INFO] [process] process subsystem ready, idle pid=0
[INFO] [scheduler] round-robin scheduler ready
[INFO] [kernel] NamelessOS v0.1 booting...
[INFO] [kernel] All modules initialized.
[INFO] [kernel] Interrupts enabled.
```

## Next session

PROMPT_16 — Syscall: SYSCALL/SYSRET dispatch table, read/write/open/close/exit/getpid/fork/execve/waitpid/mmap/munmap/brk.

---

[← 14](PROMPT_14.md) | [index](README.md) | **15** | 16 →
