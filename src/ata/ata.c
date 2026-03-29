#include "ata.h"
#include "ata_internal.h"
#include "../serial/serial.h"

/* ── Drive state ─────────────────────────────────────────────────── */
static u32 ata_total_sectors = 0;   /* 0 = no drive */
static u16 ata_base          = 0;   /* I/O base of the channel with the drive */

/* ── I/O helpers ─────────────────────────────────────────────────── */
static inline void outb(u16 port, u8 val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}
static inline u8 inb(u16 port) {
    u8 val;
    __asm__ volatile ("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}
static inline u16 inw(u16 port) {
    u16 val;
    __asm__ volatile ("inw %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}
static inline void outw(u16 port, u16 val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

/* ── poll helpers ────────────────────────────────────────────────── */
static u8 poll_bsy(u16 base) {
    for (u32 i = 0; i < 0x100000; i++) {
        u8 s = inb((u16)(base + ATA_REG_STATUS));
        if (!(s & ATA_SR_BSY)) return s;
    }
    return 0xFF;
}
static u8 poll_drq(u16 base) {
    for (u32 i = 0; i < 0x100000; i++) {
        u8 s = inb((u16)(base + ATA_REG_STATUS));
        if (s & ATA_SR_ERR) return s;
        if (s & ATA_SR_BSY) continue;   /* still busy */
        if (s & ATA_SR_DRQ) return s;
    }
    return 0xFF;
}

/* ── try_identify ────────────────────────────────────────────────── */
/* Try IDENTIFY on one channel/drive. Returns total sectors, or 0. */
static u32 try_identify(u16 base, u16 ctrl) {
    /* Software reset */
    outb(ctrl, 0x04);
    /* 400ns delay: read alt status 4 times */
    for (int i = 0; i < 4; i++) inb(ctrl);
    outb(ctrl, 0x00);
    for (int i = 0; i < 4; i++) inb(ctrl);

    /* Select master */
    outb((u16)(base + ATA_REG_DRIVE), 0xA0);
    /* 400ns settle */
    for (int i = 0; i < 4; i++) inb(ctrl);

    u8 st = poll_bsy(base);
    /* 0xFF = floating bus (no device on channel) */
    if (st == 0xFF) return 0;

    /* Send IDENTIFY */
    outb((u16)(base + ATA_REG_CMD), ATA_CMD_IDENTIFY);
    for (int i = 0; i < 4; i++) inb(ctrl);

    st = inb((u16)(base + ATA_REG_STATUS));
    if (st == 0x00) return 0;   /* no device */

    /* Check mid/hi — ATAPI devices set 0x14/0xEB after IDENTIFY */
    u8 mid = inb((u16)(base + ATA_REG_LBA_MID));
    u8 hi  = inb((u16)(base + ATA_REG_LBA_HI));
    if ((mid == 0x14 && hi == 0xEB) ||
        (mid == 0x69 && hi == 0x96)) return 0;   /* ATAPI / SATA PI */

    st = poll_drq(base);
    if (st & ATA_SR_ERR) return 0;
    if (st == 0xFF)      return 0;

    u16 ident[256];
    for (u32 i = 0; i < 256; i++)
        ident[i] = inw((u16)(base + ATA_REG_DATA));

    u32 sectors = ((u32)ident[ATA_IDENT_SECTORS28 + 1] << 16) |
                   (u32)ident[ATA_IDENT_SECTORS28];
    return sectors;
}

/* ── lba_setup ───────────────────────────────────────────────────── */
static void lba_setup(u32 lba, u8 count) {
    outb((u16)(ata_base + ATA_REG_DRIVE),
         (u8)(ATA_DRIVE_MASTER | ((lba >> 24) & 0x0F)));
    outb((u16)(ata_base + ATA_REG_SECCOUNT), count);
    outb((u16)(ata_base + ATA_REG_LBA_LO),  (u8)(lba));
    outb((u16)(ata_base + ATA_REG_LBA_MID), (u8)(lba >>  8));
    outb((u16)(ata_base + ATA_REG_LBA_HI),  (u8)(lba >> 16));
}

/* ── Public API ──────────────────────────────────────────────────── */

int ata_read(u32 lba, u8 count, void *buf) {
    if (!ata_total_sectors) return -1;
    u8 st = poll_bsy(ata_base);
    if (st == 0xFF) return -1;
    lba_setup(lba, count);
    outb((u16)(ata_base + ATA_REG_CMD), ATA_CMD_READ);
    u16 *ptr = (u16 *)buf;
    for (u8 s = 0; s < count; s++) {
        st = poll_drq(ata_base);
        if (st & ATA_SR_ERR) return -1;
        for (u32 w = 0; w < ATA_SECTOR_WORDS; w++)
            *ptr++ = inw((u16)(ata_base + ATA_REG_DATA));
    }
    return 0;
}

int ata_write(u32 lba, u8 count, const void *buf) {
    if (!ata_total_sectors) return -1;
    u8 st = poll_bsy(ata_base);
    if (st == 0xFF) return -1;
    lba_setup(lba, count);
    outb((u16)(ata_base + ATA_REG_CMD), ATA_CMD_WRITE);
    const u16 *ptr = (const u16 *)buf;
    for (u8 s = 0; s < count; s++) {
        st = poll_drq(ata_base);
        if (st & ATA_SR_ERR) return -1;
        for (u32 w = 0; w < ATA_SECTOR_WORDS; w++)
            outw((u16)(ata_base + ATA_REG_DATA), *ptr++);
    }
    poll_bsy(ata_base);
    outb((u16)(ata_base + ATA_REG_CMD), ATA_CMD_FLUSH);
    poll_bsy(ata_base);
    return 0;
}

/* ── ata_dump ────────────────────────────────────────────────────── */
static void ata_dump(void) {
    if (ata_total_sectors)
        klog(LOG_DEBUG, "[ata] drive: %u sectors on base 0x%x",
             (unsigned)ata_total_sectors, (unsigned)ata_base);
    else
        klog(LOG_DEBUG, "[ata] no drive");
}

/* ── ata_init ────────────────────────────────────────────────────── */
int ata_init(void) {
    /* Try primary channel first, then secondary */
    u32 sectors = try_identify(ATA_PRIMARY_BASE, ATA_PRIMARY_CTRL);
    if (sectors) {
        ata_base          = ATA_PRIMARY_BASE;
        ata_total_sectors = sectors;
    } else {
        sectors = try_identify(ATA_SECONDARY_BASE, ATA_SECONDARY_CTRL);
        if (sectors) {
            ata_base          = ATA_SECONDARY_BASE;
            ata_total_sectors = sectors;
        }
    }

    if (!ata_total_sectors) {
        klog(LOG_INFO, "[ata] no drive detected");
        return 0;
    }

    u32 mb = ata_total_sectors / 2048;
    klog(LOG_INFO, "[ata] drive 0: %u sectors (%u MB)",
         (unsigned)ata_total_sectors, (unsigned)mb);
    return 0;
}

/* ── module descriptor ───────────────────────────────────────────── */
kernel_module_t mod_ata = {
    .name        = "ata",
    .initialized = false,
    .init        = ata_init,
    .dump        = ata_dump,
    .shutdown    = NULL,
};
