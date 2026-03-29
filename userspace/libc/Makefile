# userspace/libc/Makefile — builds libc.a and crt0.o

CC    := x86_64-linux-gnu-gcc
AS    := nasm
AR    := x86_64-linux-gnu-ar

GCC_INC := $(shell x86_64-linux-gnu-gcc -ffreestanding -print-file-name=include)

CFLAGS := -ffreestanding -nostdlib -nostdinc \
          -mno-red-zone -mcmodel=small \
          -fno-pic -fno-pie \
          -mno-sse -mno-sse2 -mno-avx \
          -O2 -Wall -Wextra -std=c11 \
          -I include -I $(GCC_INC)

ASFLAGS := -f elf64

BUILD   := ../../build/userspace/libc

C_SRCS  := string.c malloc.c printf.c
C_OBJS  := $(patsubst %.c, $(BUILD)/%.o, $(C_SRCS))

ASM_SRCS := syscall.asm
ASM_OBJS := $(patsubst %.asm, $(BUILD)/%.o, $(ASM_SRCS))

CRT0_OBJ := $(BUILD)/crt0.o

LIB     := $(BUILD)/libc.a

.PHONY: all clean

all: $(LIB) $(CRT0_OBJ)

$(LIB): $(C_OBJS) $(ASM_OBJS)
	@mkdir -p $(dir $@)
	$(AR) rcs $@ $^
	@echo "AR: $@"

$(CRT0_OBJ): crt0.asm
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@
	@echo "AS: $@"

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.asm
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

clean:
	rm -rf $(BUILD)
