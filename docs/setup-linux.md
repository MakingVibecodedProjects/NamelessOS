# Setup — Linux / CI

## Dependencies

```bash
# Debian / Ubuntu
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

# Arch Linux
sudo pacman -S --needed \
    x86_64-linux-gnu-gcc \
    nasm \
    qemu-full \
    grub \
    xorriso \
    mtools

# Fedora
sudo dnf install -y \
    gcc-x86_64-linux-gnu \
    nasm \
    qemu-system-x86 \
    grub2-tools \
    xorriso \
    mtools
```

## Build

```bash
make iso   # build/namelessos.iso
make run   # launch QEMU, serial → stdio
```

## CI (GitHub Actions example)

```yaml
- name: Install deps
  run: |
    sudo apt-get update
    sudo apt-get install -y gcc-x86-64-linux-gnu nasm qemu-system-x86 \
      grub-pc-bin grub-common xorriso mtools

- name: Build ISO
  run: make iso
```

The ISO is deterministic given the same toolchain versions. No runtime secrets
or external fetches are needed during the build.
