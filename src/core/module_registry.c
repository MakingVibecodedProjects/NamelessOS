#include "module_registry.h"
#include "../serial/serial.h"
#include "../vga/vga.h"
#include "../gdt/gdt.h"
#include "../idt/idt.h"
#include "../pic/pic.h"
#include "../pmm/pmm.h"
#include "../vmm/vmm.h"
#include "../heap/heap.h"
#include "../timer/timer.h"
#include "../keyboard/keyboard.h"
#include "../pci/pci.h"
#include "../ata/ata.h"
#include "../vfs/vfs.h"
#include "../tmpfs/tmpfs.h"
#include "../devfs/devfs.h"
#include "../process/process.h"
#include "../scheduler/scheduler.h"
#include "../syscall/syscall.h"
#include "../elf/elf.h"
#include "../net/e1000/e1000.h"
#include "../net/ethernet/ethernet.h"
#include "../net/arp/arp.h"
#include "../net/ipv4/ipv4.h"
#include "../net/icmp/icmp.h"

/* ── Module table — init order matters ──────────────────────────── */
static kernel_module_t *modules[] = {
    &mod_serial,
    &mod_vga,
    &mod_gdt,
    &mod_idt,
    &mod_pic,
    &mod_pmm,
    &mod_vmm,
    &mod_heap,
    &mod_timer,
    &mod_keyboard,
    &mod_pci,
    &mod_ata,
    &mod_vfs,
    &mod_tmpfs,
    &mod_devfs,
    &mod_process,
    &mod_scheduler,
    &mod_syscall,
    &mod_elf,
    &mod_e1000,
    &mod_ethernet,
    &mod_arp,
    &mod_ipv4,
    &mod_icmp,
};

#define MODULE_COUNT ((int)(sizeof(modules) / sizeof(modules[0])))

/* ── modules_init_all ────────────────────────────────────────────── */
void modules_init_all(void) {
    for (int i = 0; i < MODULE_COUNT; i++) {
        kernel_module_t *m = modules[i];
        if (m->init) {
            int ret = m->init();
            m->initialized = (ret == 0);
        } else {
            m->initialized = true;
        }
    }
}

/* ── modules_dump_all ────────────────────────────────────────────── */
void modules_dump_all(void) {
    for (int i = 0; i < MODULE_COUNT; i++) {
        kernel_module_t *m = modules[i];
        if (m->initialized && m->dump)
            m->dump();
    }
}
