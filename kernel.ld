/* NamelessOS linker script
 *
 * Physical layout (load addresses):
 *   0x00100000  .multiboot2 + .text.boot (32-bit entry, page tables, early stack)
 *   ...         rest of kernel (higher half virtual, lower physical)
 *
 * Virtual layout:
 *   0x00100000  .boot     (identity mapped — same VA as PA for 32-bit entry)
 *   0xFFFFFFFF80100000+  kernel proper
 */

ENTRY(start)

KERNEL_PHYS = 0x00100000;
KERNEL_VMA  = 0xFFFFFFFF80000000;

SECTIONS {

    /* ── boot section: 32-bit entry code + page tables ────────────
       VMA == LMA == 0x100000 so the 32-bit code can reference everything
       with plain 32-bit addresses before paging is enabled. */
    . = KERNEL_PHYS;

    .boot ALIGN(4K) : {
        KEEP(*(.multiboot2))
        *(.text.boot)
        *(.bss.mb2)
        . = ALIGN(4K);
        *(.bss.pagetables)
    }

    /* ── higher half kernel ────────────────────────────────────────
       Virtual address jumps to the higher half; physical address
       continues right after the boot section. */
    . += KERNEL_VMA;

    .text ALIGN(4K) : AT(ADDR(.text) - KERNEL_VMA) {
        __kernel_start = .;
        *(.text .text.*)
    }

    .rodata ALIGN(4K) : AT(ADDR(.rodata) - KERNEL_VMA) {
        *(.rodata .rodata.*)
    }

    .data ALIGN(4K) : AT(ADDR(.data) - KERNEL_VMA) {
        *(.data .data.*)
    }

    .bss ALIGN(4K) : AT(ADDR(.bss) - KERNEL_VMA) {
        *(COMMON)
        *(.bss .bss.*)
        __kernel_end = .;
    }

    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
    }
}
