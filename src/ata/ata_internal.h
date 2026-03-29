#ifndef ATA_INTERNAL_H
#define ATA_INTERNAL_H

#include "../lib/types.h"

/* ── ATA channel I/O ports ───────────────────────────────────────── */
#define ATA_PRIMARY_BASE    0x1F0
#define ATA_PRIMARY_CTRL    0x3F6
#define ATA_SECONDARY_BASE  0x170
#define ATA_SECONDARY_CTRL  0x376

/* ── Register offsets from base ──────────────────────────────────── */
#define ATA_REG_DATA        0x00   /* R/W 16-bit data port */
#define ATA_REG_ERROR       0x01   /* R   error register */
#define ATA_REG_FEATURES    0x01   /* W   features */
#define ATA_REG_SECCOUNT    0x02   /* R/W sector count */
#define ATA_REG_LBA_LO      0x03   /* R/W LBA bits 0–7 */
#define ATA_REG_LBA_MID     0x04   /* R/W LBA bits 8–15 */
#define ATA_REG_LBA_HI      0x05   /* R/W LBA bits 16–23 */
#define ATA_REG_DRIVE       0x06   /* R/W drive/head select */
#define ATA_REG_STATUS      0x07   /* R   status */
#define ATA_REG_CMD         0x07   /* W   command */

/* ── Status register bits ────────────────────────────────────────── */
#define ATA_SR_ERR          (1 << 0)
#define ATA_SR_DRQ          (1 << 3)
#define ATA_SR_SRV          (1 << 4)
#define ATA_SR_DF           (1 << 5)
#define ATA_SR_RDY          (1 << 6)
#define ATA_SR_BSY          (1 << 7)

/* ── ATA commands ────────────────────────────────────────────────── */
#define ATA_CMD_READ        0x20
#define ATA_CMD_WRITE       0x30
#define ATA_CMD_FLUSH       0xE7
#define ATA_CMD_IDENTIFY    0xEC

/* ── Drive select byte ───────────────────────────────────────────── */
/* LBA mode, master drive, upper 4 LBA bits in bits 0–3 */
#define ATA_DRIVE_MASTER    0xE0

/* ── IDENTIFY response word indices ─────────────────────────────── */
#define ATA_IDENT_SECTORS28 60   /* 28-bit LBA total sectors (2 words) */

/* ── Sector size ─────────────────────────────────────────────────── */
#define ATA_SECTOR_SIZE     512
#define ATA_SECTOR_WORDS    (ATA_SECTOR_SIZE / 2)

#endif /* ATA_INTERNAL_H */
