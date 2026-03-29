#ifndef MODULE_REGISTRY_H
#define MODULE_REGISTRY_H

#include "../lib/module.h"

/* Initialize all registered modules in dependency order. */
void modules_init_all(void);

/* Call dump() on every initialized module (diagnostic). */
void modules_dump_all(void);

#endif /* MODULE_REGISTRY_H */
