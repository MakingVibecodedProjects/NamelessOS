#include "module_registry.h"
#include "../serial/serial.h"
#include "../lib/types.h"

/* Forward declaration — pmm_init is called before mod_pmm registration
   so it can receive the MB2 pointer directly. */
void pmm_set_mb2(u64 mb2_info_phys);

void kernel_main(u64 mb2_info_phys) {
    pmm_set_mb2(mb2_info_phys);
    modules_init_all();
    klog(LOG_INFO, "[kernel] NamelessOS v0.1 booting...");
    klog(LOG_INFO, "[kernel] All modules initialized.");
    __asm__ volatile ("sti");
    klog(LOG_INFO, "[kernel] Interrupts enabled.");
    for (;;) __asm__ volatile ("hlt");
}
