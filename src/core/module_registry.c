#include "module_registry.h"
#include "../serial/serial.h"
#include "../vga/vga.h"
#include "../gdt/gdt.h"

/* ── Module table — init order matters ──────────────────────────── */
static kernel_module_t *modules[] = {
    &mod_serial,
    &mod_vga,
    &mod_gdt,
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
