[← 19](PROMPT_19.md) | [index](README.md) | **20** | [21 →](PROMPT_21.md)

---

# PROMPT_20 — Phase 6 Step 4: init + shell

**Session date:** 2026-03-29
**Status when starting:** Phase 6 Step 3 complete (libc.a + crt0.o built, zero warnings)
**Status when done:** Phase 6 Step 4 complete — `init` and `shell` ELF64 binaries link clean; zero warnings; proper RX/RW segment split

## What was built

- `userspace/programs/init/init.c` — PID 1: prints pid, `fork`s a child that `execve`s `/bin/shell`, `waitpid`s on it, respawns in a loop
- `userspace/programs/shell/shell.c` — interactive shell: `readline` from stdin, `parse` into argv, dispatches `exit`/`cd`/`ls`/`cat` as built-ins (cd/ls/cat stubbed — await syscall wiring in Phase 7), all other commands via `fork` + `execve` + `waitpid`
- `userspace/programs/userspace.ld` — linker script: base `0x400000`; separate `PHDRS` (`text PT_LOAD FLAGS(5)` and `data PT_LOAD FLAGS(6)`) so `.text`/`.rodata` go in R+X segment, `.data`/`.bss` go in R+W segment
- `userspace/programs/Makefile` — compiles each program, links `crt0.o + program.o + libc.a` with `userspace.ld`; output in `build/userspace/programs/`
- Top-level `Makefile` — `userspace` target now runs both `libc` and `programs` sub-makes; `clean` cleans both
- `userspace/libc/include/unistd.h` — added missing declarations: `open`, `waitpid`, `execve`

## Key decisions

- **Separate `PHDRS` in linker script** — without explicit program headers the linker emits one RWX `LOAD` segment (deprecated, linker warns); splitting into text (RX) and data (RW) suppresses the warning and is correct security-wise
- **`.rodata` in the text `PHDR`** — read-only data belongs in the non-writable segment; it shares the `text` `PT_LOAD` entry
- **`init` respawns in a loop** — if the shell exits (user types `exit`), init forks again immediately; standard PID 1 behaviour
- **`cd`/`ls`/`cat` stubbed** — `chdir` and VFS `readdir`/`open` syscalls are wired in Phase 7; stubs print a placeholder so the shell is usable for `exec`-style commands now
- **Entry point `0x400000`** — verified with `readelf -l`; `crt0.asm` `_start` is first in `.text`, so it lands at exactly the ELF entry point

## Verified build output

```
LD: ../../build/userspace/programs/init
LD: ../../build/userspace/programs/shell
```
(zero warnings, zero errors; both ELFs are statically linked, entry=0x400000, two clean LOAD segments)

## Phase 6 complete ✓

1. ✅ Per-process page tables, COW fork, CR3 switch
2. ✅ ELF64 loader (PT_LOAD mapper)
3. ✅ Userspace libc (crt0, syscall wrappers, string/malloc/printf)
4. ✅ init (PID 1) + shell (cd/ls/cat/exec)

## Next session

[PROMPT_21 →](PROMPT_21.md) — Phase 7 Step 1: e1000 NIC driver (`src/net/e1000/`).

---

[← 19](PROMPT_19.md) | [index](README.md) | **20** | [21 →](PROMPT_21.md)
