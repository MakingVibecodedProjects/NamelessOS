#ifndef SMP_H
#define SMP_H

#include "../lib/types.h"
#include "../lib/module.h"
#include "smp_internal.h"
#include "spinlock.h"

/* Initialise the LAPIC, detect APs via CPUID/ACPI, fire INIT+SIPI.
   Returns 0 on success (even if only 1 CPU is present). */
int smp_init(void);

/* Return the number of CPUs brought online (including BSP). */
u32 smp_cpu_count(void);

/* Return a pointer to the per-CPU data for the calling CPU.
   Reads GS base — valid only after smp_init(). */
cpu_t *smp_this_cpu(void);

/* Return a pointer to the per-CPU data for CPU index id.
   Returns NULL if id >= smp_cpu_count(). */
cpu_t *smp_cpu(u32 id);

/* Write the LAPIC EOI register (used by LAPIC timer handler). */
void lapic_eoi(void);

/* Module descriptor — registered in module_registry. */
extern kernel_module_t mod_smp;

#endif /* SMP_H */
