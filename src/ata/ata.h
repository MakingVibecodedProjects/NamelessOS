#ifndef ATA_H
#define ATA_H

#include "../lib/types.h"
#include "../lib/module.h"

/* Detect and identify the primary ATA master drive.  Returns 0 on success. */
int  ata_init(void);

/* Read count sectors starting at 28-bit LBA into buf.
   buf must be at least count * 512 bytes.  Returns 0 on success, -1 on error. */
int  ata_read(u32 lba, u8 count, void *buf);

/* Write count sectors starting at 28-bit LBA from buf.
   Returns 0 on success, -1 on error. */
int  ata_write(u32 lba, u8 count, const void *buf);

/* Module descriptor — registered in module_registry. */
extern kernel_module_t mod_ata;

#endif /* ATA_H */
