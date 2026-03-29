#ifndef PCI_H
#define PCI_H

#include "../lib/types.h"
#include "../lib/module.h"

/* Describes a discovered PCI device. */
typedef struct {
    u8  bus;
    u8  slot;
    u8  func;
    u16 vendor_id;
    u16 device_id;
    u8  class_code;
    u8  subclass;
    u8  prog_if;
    u8  int_line;
    u32 bar[6];
} pci_device_t;

/* Enumerate all PCI buses and populate the device table.  Returns 0. */
int           pci_init(void);

/* Return a pointer to the first device matching vendor+device ID, or NULL. */
pci_device_t *pci_find_device(u16 vendor, u16 device);

/* Read one 32-bit config dword at byte offset off for the given device. */
u32           pci_read32(pci_device_t *dev, u8 off);

/* Write one 32-bit config dword. */
void          pci_write32(pci_device_t *dev, u8 off, u32 val);

/* Module descriptor — registered in module_registry. */
extern kernel_module_t mod_pci;

#endif /* PCI_H */
