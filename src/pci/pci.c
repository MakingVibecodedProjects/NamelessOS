#include "pci.h"
#include "pci_internal.h"
#include "../serial/serial.h"

/* ── Device table ────────────────────────────────────────────────── */
static pci_device_t devices[PCI_MAX_DEVICES];
static u32          device_count = 0;

/* ── I/O helpers ─────────────────────────────────────────────────── */
static inline void outl(u16 port, u32 val) {
    __asm__ volatile ("outl %0, %1" : : "a"(val), "Nd"(port));
}
static inline u32 inl(u16 port) {
    u32 val;
    __asm__ volatile ("inl %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

/* ── Raw config read (32-bit aligned dword) ──────────────────────── */
static u32 config_read32(u8 bus, u8 slot, u8 func, u8 off) {
    outl(PCI_CONFIG_ADDRESS, PCI_ADDR(bus, slot, func, off));
    return inl(PCI_CONFIG_DATA);
}

/* ── Raw config read helpers (narrower widths) ───────────────────── */
static u16 config_read16(u8 bus, u8 slot, u8 func, u8 off) {
    u32 dword = config_read32(bus, slot, func, off & 0xFC);
    return (u16)(dword >> ((off & 2) * 8));
}
static u8 config_read8(u8 bus, u8 slot, u8 func, u8 off) {
    u32 dword = config_read32(bus, slot, func, off & 0xFC);
    return (u8)(dword >> ((off & 3) * 8));
}

/* ── Public config read/write via pci_device_t ───────────────────── */
u32 pci_read32(pci_device_t *dev, u8 off) {
    return config_read32(dev->bus, dev->slot, dev->func, off);
}
void pci_write32(pci_device_t *dev, u8 off, u32 val) {
    outl(PCI_CONFIG_ADDRESS, PCI_ADDR(dev->bus, dev->slot, dev->func, off));
    outl(PCI_CONFIG_DATA, val);
}

/* ── probe_function ──────────────────────────────────────────────── */
static void probe_function(u8 bus, u8 slot, u8 func) {
    u16 vendor = config_read16(bus, slot, func, PCI_OFF_VENDOR);
    if (vendor == 0xFFFF) return;   /* no device */

    if (device_count >= PCI_MAX_DEVICES) return;

    pci_device_t *d = &devices[device_count++];
    d->bus       = bus;
    d->slot      = slot;
    d->func      = func;
    d->vendor_id = vendor;
    d->device_id = config_read16(bus, slot, func, PCI_OFF_DEVICE);
    d->class_code= config_read8 (bus, slot, func, PCI_OFF_CLASS);
    d->subclass  = config_read8 (bus, slot, func, PCI_OFF_SUBCLASS);
    d->prog_if   = config_read8 (bus, slot, func, PCI_OFF_PROG_IF);
    d->int_line  = config_read8 (bus, slot, func, PCI_OFF_INT_LINE);

    for (u8 i = 0; i < 6; i++)
        d->bar[i] = config_read32(bus, slot, func,
                                  (u8)(PCI_OFF_BAR0 + i * 4));

    klog(LOG_DEBUG,
         "[pci]   %x:%x.%x vendor=%x device=%x class=%x:%x",
         (unsigned)bus, (unsigned)slot, (unsigned)func,
         (unsigned)d->vendor_id, (unsigned)d->device_id,
         (unsigned)d->class_code, (unsigned)d->subclass);
}

/* ── probe_slot ──────────────────────────────────────────────────── */
static void probe_slot(u8 bus, u8 slot) {
    u16 vendor = config_read16(bus, slot, 0, PCI_OFF_VENDOR);
    if (vendor == 0xFFFF) return;

    probe_function(bus, slot, 0);

    /* Check for multi-function device */
    u8 htype = config_read8(bus, slot, 0, PCI_OFF_HEADER_TYPE);
    if (htype & PCI_HTYPE_MULTI) {
        for (u8 func = 1; func < PCI_MAX_FUNC; func++)
            probe_function(bus, slot, func);
    }
}

/* ── Public API ──────────────────────────────────────────────────── */

pci_device_t *pci_find_device(u16 vendor, u16 device) {
    for (u32 i = 0; i < device_count; i++) {
        if (devices[i].vendor_id == vendor &&
            devices[i].device_id == device)
            return &devices[i];
    }
    return NULL;
}

/* ── pci_dump ────────────────────────────────────────────────────── */
static void pci_dump(void) {
    klog(LOG_DEBUG, "[pci] %u device(s) enumerated", (unsigned)device_count);
}

/* ── pci_init ────────────────────────────────────────────────────── */
int pci_init(void) {
    device_count = 0;

    for (u32 bus = 0; bus < PCI_MAX_BUS; bus++)
        for (u32 slot = 0; slot < PCI_MAX_SLOT; slot++)
            probe_slot((u8)bus, (u8)slot);

    klog(LOG_INFO, "[pci] found %u device(s)", (unsigned)device_count);
    return 0;
}

/* ── module descriptor ───────────────────────────────────────────── */
kernel_module_t mod_pci = {
    .name        = "pci",
    .initialized = false,
    .init        = pci_init,
    .dump        = pci_dump,
    .shutdown    = NULL,
};
