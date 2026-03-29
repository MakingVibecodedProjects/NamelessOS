# Setup — Windows (WSL2)

All build tools run inside WSL2. The kernel is compiled with a cross-compiler
targeting `x86_64-linux-gnu`; QEMU and GRUB run inside the WSL2 environment and
output serial to the Windows terminal.

## 1. Install WSL2

Open PowerShell as Administrator:

```powershell
wsl --install
# Reboot when prompted, then open the Ubuntu app to finish setup
```

If WSL is already installed, make sure you're on WSL2:

```powershell
wsl --set-default-version 2
```

## 2. Install build dependencies

Inside the WSL2 Ubuntu terminal:

```bash
sudo apt update && sudo apt install -y \
    gcc-x86-64-linux-gnu \
    nasm \
    qemu-system-x86 \
    grub-pc-bin \
    grub-common \
    xorriso \
    mtools \
    make \
    git
```

Verify:

```bash
x86_64-linux-gnu-gcc --version   # should print gcc 11 or later
nasm --version                    # 2.15+
qemu-system-x86_64 --version      # 6.x or later
grub-mkrescue --version           # 2.06+
```

## 3. Clone the repo (if you haven't already)

```bash
git clone https://github.com/yourname/namelessos.git
cd namelessos
```

Or if you're working from a Windows path, access it via the WSL mount:

```bash
cd /mnt/c/Users/<you>/Documents/kernel_developing
```

## 4. Build and run

```bash
make iso      # compile + assemble + link + wrap in GRUB ISO
make run      # boot in QEMU — serial output lands in this terminal
```

To stop QEMU: `Ctrl-A X`

## 5. Makefile WSL invocation

The `make run` and `make debug` targets in this project call
`qemu-system-x86_64` directly. When running from a Windows shell (CMD / PowerShell),
pipe through WSL:

```powershell
wsl make iso
wsl make run
```

Or open the repo directly in a WSL terminal window (recommended).

## Notes

- Do **not** use `make -j` — the linker script depends on strict object ordering.
- Timestamps inside WSL2 can drift if the Windows clock skews. Run
  `touch src/**/*.c` if `make` claims everything is up to date after a crash.
- QEMU display is set to `none`; all output goes to serial (stdio). No GUI needed.
