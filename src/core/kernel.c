#include "module_registry.h"
#include "../serial/serial.h"
#include "../scheduler/scheduler.h"
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
    /* Hand off to the scheduler.  This call switches to pid 1 (init).
       When all other processes have run, the scheduler switches back here
       and idle_fn takes over from the idle process context. */
    /* Drop into the idle loop — yields the CPU whenever other processes are ready. */
    for (;;) {
        scheduler_yield();
        __asm__ volatile ("hlt");
    }
}
