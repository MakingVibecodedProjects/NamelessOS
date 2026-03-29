# PROMPT_14 — Phase 5 Step 1: Process

**Session date:** 2026-03-29
**Status when starting:** Phase 4 complete (ATA, VFS, tmpfs, devfs all working)
**Status when done:** Phase 5 Step 1 complete — process subsystem, idle pid=0, kthread_create ready

## What was built

- `src/process/process_internal.h` — `PROC_MAX=64`, `KSTACK_SIZE=8192`, process state constants (`PROC_UNUSED`…`PROC_ZOMBIE`), `cpu_context_t` (rbx, rbp, r12–r15, rsp, rip — callee-saved only)
- `src/process/process.h` — `process_t` (pid, state, name[32], kstack, ctx, next), `process_init()`, `kthread_create()`, `process_exit()`, `process_current()`, `process_get()`
- `src/process/process.c` — static 64-slot table; pid 0 = idle (boot context, no kstack); `kthread_create` allocates 8 KB kstack with `kmalloc`, sets up initial `cpu_context_t` (rip=fn, rsp=stack_top-8); `process_exit` marks ZOMBIE and halts

## Key decisions

- **pid = table index** — O(1) lookup with no hash map; `alloc_pid` scans for first `PROC_UNUSED` slot
- **Idle process uses boot stack** — pid 0 kstack is NULL; scheduler must never free it
- **`rsp = stack_top - 8`** — initial RSP is 8 bytes below the top so the first `ret` from the thread entry trampoline sees a valid return address (set to 0 → faults cleanly if fn returns without calling `process_exit`)
- **No context switch yet** — that's the scheduler's job; process.c only manages the table and stacks

## Verified serial output

```
[INFO] [process] process subsystem ready, idle pid=0
```

## Next session prompt

Implement **Phase 5 Step 2**: `src/scheduler/` — preemptive round-robin scheduler.

- `src/scheduler/scheduler_internal.h` — nothing needed beyond process.h types
- `src/scheduler/scheduler.h` — `scheduler_init()`, `scheduler_yield()`, `scheduler_tick()` (called from timer callback), `mod_scheduler`
- `src/scheduler/scheduler.asm` — `context_switch(cpu_context_t *old, cpu_context_t *new)`: save callee-saved regs + RSP into `old`, load from `new`, jump to `new->rip`
- `src/scheduler/scheduler.c`:
  - Circular run queue of `process_t *` (linked via `->next`)
  - `scheduler_init()`: add idle (pid 0) to run queue, register `scheduler_tick` as a timer callback
  - `scheduler_tick()`: called every IRQ0; increment tick counter; every N ticks call `scheduler_yield()`
  - `scheduler_yield()`: pick next READY process from run queue, call `context_switch(old_ctx, new_ctx)`
  - Skip ZOMBIE/BLOCKED processes during selection
  - Log `[scheduler] round-robin scheduler ready`
- Register `mod_scheduler` after `mod_process` in module_registry
- Zero warnings policy applies
