#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "../lib/types.h"
#include "../lib/module.h"

/* Initialise PS/2 keyboard, register IRQ1 handler.  Returns 0 on success. */
int  keyboard_init(void);

/* Pop one ASCII character from the buffer.
   Returns the character, or -1 if the buffer is empty. */
i32  keyboard_getc(void);

/* Module descriptor — registered in module_registry. */
extern kernel_module_t mod_keyboard;

#endif /* KEYBOARD_H */
