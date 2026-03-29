#include "module_registry.h"
#include "../serial/serial.h"

void kernel_main(void) {
    modules_init_all();
    klog(LOG_INFO, "[kernel] NamelessOS v0.1 booting...");
    klog(LOG_INFO, "[kernel] All modules initialized.");
    __asm__ volatile ("sti");
    klog(LOG_INFO, "[kernel] Interrupts enabled.");
    for (;;) __asm__ volatile ("hlt");
}
