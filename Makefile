# NamelessOS Makefile

CC      := x86_64-linux-gnu-gcc
AS      := nasm
LD      := x86_64-linux-gnu-ld

CFLAGS  := -ffreestanding -nostdlib -nostdinc -mno-red-zone -mcmodel=kernel \
           -fno-pic -fno-pie -mno-sse -mno-sse2 -mno-avx \
           -O2 -Wall -Wextra -std=c11 -I src

ASFLAGS := -f elf64
LDFLAGS := -T kernel.ld -nostdlib

BUILD   := build
TARGET  := $(BUILD)/namelessos.elf
ISO     := $(BUILD)/namelessos.iso

# Collect all .c sources under src/
C_SRCS  := $(shell find src -name '*.c')
C_OBJS  := $(patsubst src/%.c, $(BUILD)/%.o, $(C_SRCS))

# Collect all .asm sources under boot/
ASM_SRCS := $(shell find boot -name '*.asm')
ASM_OBJS := $(patsubst boot/%.asm, $(BUILD)/boot/%.o, $(ASM_SRCS))

ALL_OBJS := $(ASM_OBJS) $(C_OBJS)

.PHONY: all iso run debug clean

all: $(TARGET)

# ── ELF build ──────────────────────────────────────────────────────
$(TARGET): $(ALL_OBJS) kernel.ld
	@mkdir -p $(BUILD)
	$(LD) $(LDFLAGS) -o $@ $(ALL_OBJS)
	@echo "Linked: $@"

# Compile .c → .o
$(BUILD)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Assemble .asm → .o
$(BUILD)/boot/%.o: boot/%.asm
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

# ── ISO build (GRUB2 Multiboot2 boot) ──────────────────────────────
iso: $(ISO)

$(ISO): $(TARGET)
	@mkdir -p $(BUILD)/iso/boot/grub
	cp $(TARGET) $(BUILD)/iso/boot/namelessos.elf
	cp grub.cfg  $(BUILD)/iso/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO) $(BUILD)/iso 2>/dev/null
	@echo "ISO: $(ISO)"

# ── Run in QEMU (boots via ISO → GRUB → kernel) ────────────────────
run: $(ISO)
	qemu-system-x86_64 \
		-cdrom $(ISO) \
		-m 512M \
		-serial stdio \
		-netdev user,id=net0 -device e1000,netdev=net0 \
		-display none \
		-boot d

# ── Debug: QEMU + GDB stub on :1234 ────────────────────────────────
debug: $(ISO)
	qemu-system-x86_64 \
		-cdrom $(ISO) \
		-m 512M \
		-serial stdio \
		-netdev user,id=net0 -device e1000,netdev=net0 \
		-display none \
		-boot d \
		-s -S

# ── Clean ───────────────────────────────────────────────────────────
clean:
	rm -rf $(BUILD)
