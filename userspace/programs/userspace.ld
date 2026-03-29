/* userspace.ld — linker script for userspace ELF64 programs
   Programs load at 0x400000 (below kernel higher-half at 0xFFFFFFFF80000000) */

OUTPUT_FORMAT("elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)

PHDRS {
    text  PT_LOAD FLAGS(5);   /* R+X */
    data  PT_LOAD FLAGS(6);   /* R+W */
}

SECTIONS {
    . = 0x400000;

    .text : { *(.text .text.*) }       :text
    .rodata : { *(.rodata .rodata.*) } :text

    . = ALIGN(4096);
    .data : { *(.data .data.*) }       :data
    .bss  : { *(COMMON) *(.bss .bss.*) } :data

    /DISCARD/ : {
        *(.note.GNU-stack)
        *(.comment)
        *(.eh_frame)
        *(.eh_frame_hdr)
    }
}
