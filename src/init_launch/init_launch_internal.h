#ifndef INIT_LAUNCH_INTERNAL_H
#define INIT_LAUNCH_INTERNAL_H

/* User stack: 4 pages at 0x7FFFF000 downward */
#define USTACK_TOP   0x7FFFF000ULL
#define USTACK_PAGES 4

/* User RFLAGS: interrupt enable, reserved bit 1 */
#define USER_RFLAGS  0x202ULL

#endif /* INIT_LAUNCH_INTERNAL_H */
