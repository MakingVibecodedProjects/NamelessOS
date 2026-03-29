#ifndef MODULE_H
#define MODULE_H

#include "types.h"

/* Descriptor for a kernel subsystem module.
   Every module declares one of these and registers it with module_registry. */
typedef struct {
    const char *name;
    bool        initialized;
    int       (*init)(void);
    void      (*dump)(void);
    void      (*shutdown)(void);
} kernel_module_t;

#endif /* MODULE_H */
