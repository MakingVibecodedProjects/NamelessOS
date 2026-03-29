#ifndef TTY_H
#define TTY_H

#include "../lib/types.h"
#include "../lib/module.h"

/* Initialise TTY: register /dev/tty0 in devfs and wire fds 0/1/2.
   Returns 0 on success. */
int tty_init(void);

/* Module descriptor — registered in module_registry after mod_devfs. */
extern kernel_module_t mod_tty;

#endif /* TTY_H */
