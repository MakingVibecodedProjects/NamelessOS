# userspace/programs/Makefile — builds init, shell, httpd ELF64 binaries

CC      := x86_64-linux-gnu-gcc
LD      := x86_64-linux-gnu-ld

GCC_INC := $(shell x86_64-linux-gnu-gcc -ffreestanding -print-file-name=include)

CFLAGS  := -ffreestanding -nostdlib -nostdinc \
           -mno-red-zone -mcmodel=small \
           -fno-pic -fno-pie \
           -mno-sse -mno-sse2 -mno-avx \
           -O2 -Wall -Wextra -std=c11 \
           -I ../libc/include -I $(GCC_INC)

LIBC    := ../../build/userspace/libc/libc.a
CRT0    := ../../build/userspace/libc/crt0.o
BUILD   := ../../build/userspace/programs

LDFLAGS := -nostdlib -static \
           -T userspace.ld

.PHONY: all clean init shell httpd

all: init shell httpd

init: $(BUILD)/init

shell: $(BUILD)/shell

httpd: $(BUILD)/httpd

$(BUILD)/init: init/init.c $(LIBC) $(CRT0) userspace.ld
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -c init/init.c -o $(BUILD)/init.o
	$(LD) $(LDFLAGS) -o $@ $(CRT0) $(BUILD)/init.o $(LIBC)
	@echo "LD: $@"

$(BUILD)/shell: shell/shell.c $(LIBC) $(CRT0) userspace.ld
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -c shell/shell.c -o $(BUILD)/shell.o
	$(LD) $(LDFLAGS) -o $@ $(CRT0) $(BUILD)/shell.o $(LIBC)
	@echo "LD: $@"

$(BUILD)/httpd: httpd/httpd.c $(LIBC) $(CRT0) userspace.ld
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -c httpd/httpd.c -o $(BUILD)/httpd.o
	$(LD) $(LDFLAGS) -o $@ $(CRT0) $(BUILD)/httpd.o $(LIBC)
	@echo "LD: $@"

clean:
	rm -rf $(BUILD)
