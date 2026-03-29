#include "e1000.h"
#include "e1000_internal.h"
#include "../../pci/pci.h"
#include "../../vmm/vmm.h"
#include "../../heap/heap.h"
#include "../../serial/serial.h"
#include "../../lib/types.h"
#include "../../lib/string.h"

/* ── Module-private state ─────────────────────────────────────────── */
static e1000_dev_t  dev;
static bool         e1000_ready = false;
static e1000_rx_cb_t rx_callback = NULL;

/* ── MMIO helpers ─────────────────────────────────────────────────── */

static inline void e1000_write(u32 reg, u32 val) {
    *((volatile u32 *)(dev.mmio + reg)) = val;
}

static inline u32 e1000_read(u32 reg) {
    return *((volatile u32 *)(dev.mmio + reg));
}

/* ── EEPROM read (word at word-offset addr) ──────────────────────── */
static u16 eeprom_read(u8 addr) {
    e1000_write(E1000_REG_EERD,
                E1000_EERD_START | ((u32)addr << E1000_EERD_ADDR_SHIFT));
    u32 v;
    /* Spin until DONE bit is set (typically < 100 iterations) */
    for (int i = 0; i < 100000; i++) {
        v = e1000_read(E1000_REG_EERD);
        if (v & E1000_EERD_DONE)
            return (u16)(v >> E1000_EERD_DATA_SHIFT);
    }
    klog(LOG_WARN, "[e1000] EEPROM read timeout at word 0x%x\n", addr);
    return 0;
}

/* ── MAC address ──────────────────────────────────────────────────── */
/* Try EEPROM first; fall back to RAL0/RAH0 which QEMU pre-populates. */
static void read_mac(void) {
    /* Try EEPROM — the 82540EM datasheet says addr 0,1,2 hold the MAC */
    u16 w0 = eeprom_read(0);
    if (w0 != 0) {
        u16 w1 = eeprom_read(1);
        u16 w2 = eeprom_read(2);
        dev.mac[0] = (u8)(w0 & 0xFF);
        dev.mac[1] = (u8)(w0 >> 8);
        dev.mac[2] = (u8)(w1 & 0xFF);
        dev.mac[3] = (u8)(w1 >> 8);
        dev.mac[4] = (u8)(w2 & 0xFF);
        dev.mac[5] = (u8)(w2 >> 8);
        return;
    }
    /* EEPROM not available — read from RAL0/RAH0 (QEMU sets these at reset) */
    u32 ral = e1000_read(E1000_REG_RAL0);
    u32 rah = e1000_read(E1000_REG_RAH0);
    dev.mac[0] = (u8)( ral        & 0xFF);
    dev.mac[1] = (u8)((ral >>  8) & 0xFF);
    dev.mac[2] = (u8)((ral >> 16) & 0xFF);
    dev.mac[3] = (u8)((ral >> 24) & 0xFF);
    dev.mac[4] = (u8)( rah        & 0xFF);
    dev.mac[5] = (u8)((rah >>  8) & 0xFF);
}

/* ── Physical address helper ─────────────────────────────────────── */
/* For kernel heap allocations, the kernel is mapped 1:1-shifted at the
   higher half.  We recover the physical address via vmm_get_phys().   */
static u64 virt_to_phys(void *virt) {
    return vmm_get_phys((u64)virt);
}

/* ── RX ring initialisation ──────────────────────────────────────── */
static int init_rx(void) {
    /* Allocate descriptor ring — must be 16-byte aligned.
       kzalloc returns a slab block which is always ≥8-byte aligned;
       we allocate a little extra and round up. */
    usize ring_bytes = sizeof(e1000_rx_desc_t) * E1000_NUM_RX_DESC + 16;
    u8 *raw = kzalloc(ring_bytes);
    if (!raw) return -1;

    /* Align to 16 bytes */
    u64 aligned = ((u64)raw + 15) & ~(u64)15;
    dev.rx_descs = (e1000_rx_desc_t *)aligned;
    dev.rx_descs_phys = virt_to_phys(dev.rx_descs);

    /* Allocate a receive buffer for every descriptor */
    for (int i = 0; i < E1000_NUM_RX_DESC; i++) {
        dev.rx_bufs[i] = kzalloc(E1000_RX_BUF_SIZE);
        if (!dev.rx_bufs[i]) return -1;
        dev.rx_bufs_phys[i] = virt_to_phys(dev.rx_bufs[i]);
        dev.rx_descs[i].addr   = dev.rx_bufs_phys[i];
        dev.rx_descs[i].status = 0;
    }
    dev.rx_tail = 0;

    /* Program hardware */
    e1000_write(E1000_REG_RDBAL, (u32)(dev.rx_descs_phys & 0xFFFFFFFF));
    e1000_write(E1000_REG_RDBAH, (u32)(dev.rx_descs_phys >> 32));
    e1000_write(E1000_REG_RDLEN, E1000_NUM_RX_DESC * sizeof(e1000_rx_desc_t));
    e1000_write(E1000_REG_RDH,   0);
    e1000_write(E1000_REG_RDT,   E1000_NUM_RX_DESC - 1);

    /* Enable RX: broadcast accept, 2 KB buffers, strip CRC */
    u32 rctl = E1000_RCTL_EN  | E1000_RCTL_BAM |
               E1000_RCTL_BSIZE_2K | E1000_RCTL_SECRC |
               E1000_RCTL_UPE | E1000_RCTL_MPE;
    e1000_write(E1000_REG_RCTL, rctl);
    return 0;
}

/* ── TX ring initialisation ──────────────────────────────────────── */
static int init_tx(void) {
    usize ring_bytes = sizeof(e1000_tx_desc_t) * E1000_NUM_TX_DESC + 16;
    u8 *raw = kzalloc(ring_bytes);
    if (!raw) return -1;

    u64 aligned = ((u64)raw + 15) & ~(u64)15;
    dev.tx_descs = (e1000_tx_desc_t *)aligned;
    dev.tx_descs_phys = virt_to_phys(dev.tx_descs);

    for (int i = 0; i < E1000_NUM_TX_DESC; i++) {
        dev.tx_bufs[i] = kzalloc(E1000_RX_BUF_SIZE);
        if (!dev.tx_bufs[i]) return -1;
        dev.tx_bufs_phys[i] = virt_to_phys(dev.tx_bufs[i]);
        /* Mark all descriptors as done so they appear free */
        dev.tx_descs[i].status = E1000_TXD_STAT_DD;
    }
    dev.tx_tail = 0;

    e1000_write(E1000_REG_TDBAL, (u32)(dev.tx_descs_phys & 0xFFFFFFFF));
    e1000_write(E1000_REG_TDBAH, (u32)(dev.tx_descs_phys >> 32));
    e1000_write(E1000_REG_TDLEN, E1000_NUM_TX_DESC * sizeof(e1000_tx_desc_t));
    e1000_write(E1000_REG_TDH,   0);
    e1000_write(E1000_REG_TDT,   0);

    /* Enable TX: pad short packets, collision threshold=15, distance=63 */
    u32 tctl = E1000_TCTL_EN | E1000_TCTL_PSP |
               (15u << E1000_TCTL_CT_SHIFT) |
               (63u << E1000_TCTL_COLD_SHIFT);
    e1000_write(E1000_REG_TCTL, tctl);

    /* Standard inter-packet gap: IPGT=10, IPGR1=8, IPGR2=6 */
    e1000_write(E1000_REG_TIPG, (6u << 20) | (8u << 10) | 10u);
    return 0;
}

/* ── Module init ──────────────────────────────────────────────────── */
static int e1000_init_impl(void) {
    memset(&dev, 0, sizeof(dev));

    /* Find the NIC on the PCI bus */
    pci_device_t *pci = pci_find_device(E1000_VENDOR_ID, E1000_DEVICE_ID);
    if (!pci) {
        klog(LOG_WARN, "[e1000] Intel 82540EM not found on PCI bus\n");
        return -1;
    }
    klog(LOG_INFO, "[e1000] found at PCI %d:%d.%d BAR0=0x%x\n",
         pci->bus, pci->slot, pci->func, pci->bar[0]);

    /* BAR0 is the MMIO base (strip the low 4 bits which are flags).
       The boot page tables cover the first 4 GB via 1 GB identity pages,
       so the MMIO region (typically 0xfeb80000 on QEMU) is already
       accessible — no additional vmm_map_page calls needed. */
    u64 mmio_phys = (u64)(pci->bar[0] & ~0xFu);
    dev.mmio = (volatile u8 *)mmio_phys;

    /* Enable PCI Bus Master so the NIC can DMA */
    u32 cmd = pci_read32(pci, 0x04);
    pci_write32(pci, 0x04, cmd | (1u << 2));

    /* Full software reset — wait for it to clear */
    e1000_write(E1000_REG_CTRL, e1000_read(E1000_REG_CTRL) | E1000_CTRL_RST);
    for (volatile int i = 0; i < 100000; i++) {}
    if (e1000_read(E1000_REG_CTRL) & E1000_CTRL_RST) {
        klog(LOG_ERROR, "[e1000] reset did not clear\n");
        return -1;
    }

    /* Set link up + auto-speed detect */
    e1000_write(E1000_REG_CTRL,
                e1000_read(E1000_REG_CTRL) | E1000_CTRL_SLU | E1000_CTRL_ASDE);

    /* Clear multicast table */
    for (int i = 0; i < 128; i++)
        e1000_write(E1000_REG_MTA + i * 4, 0);

    /* Read MAC from EEPROM and program receive address filter */
    read_mac();
    u32 ral = ((u32)dev.mac[0])        | ((u32)dev.mac[1] << 8)  |
              ((u32)dev.mac[2] << 16)  | ((u32)dev.mac[3] << 24);
    u32 rah = ((u32)dev.mac[4])        | ((u32)dev.mac[5] << 8)  |
              E1000_RAH_AV;
    e1000_write(E1000_REG_RAL0, ral);
    e1000_write(E1000_REG_RAH0, rah);

    /* Mask all interrupts (we use polled mode) */
    e1000_write(E1000_REG_IMC, 0xFFFFFFFF);
    e1000_read(E1000_REG_ICR);   /* clear any pending */

    /* Set up DMA descriptor rings */
    if (init_rx() < 0) { klog(LOG_ERROR, "[e1000] RX init failed\n"); return -1; }
    if (init_tx() < 0) { klog(LOG_ERROR, "[e1000] TX init failed\n"); return -1; }

    e1000_ready = true;
    klog(LOG_INFO,
         "[e1000] ready — MAC %x:%x:%x:%x:%x:%x\n",
         dev.mac[0], dev.mac[1], dev.mac[2],
         dev.mac[3], dev.mac[4], dev.mac[5]);
    return 0;
}

/* ── Module dump ──────────────────────────────────────────────────── */
static void e1000_dump(void) {
    if (!e1000_ready) {
        klog(LOG_INFO, "[e1000] not ready\n");
        return;
    }
    klog(LOG_INFO, "[e1000] status=0x%x rx_tail=%d tx_tail=%d\n",
         e1000_read(E1000_REG_STATUS), dev.rx_tail, dev.tx_tail);
}

/* ── Public API ───────────────────────────────────────────────────── */

int e1000_send(const u8 *buf, u16 len) {
    if (!e1000_ready) return -1;
    if (len > E1000_MAX_FRAME) return -1;

    u32 idx = dev.tx_tail % E1000_NUM_TX_DESC;

    /* Wait until this descriptor is free (DD bit set) */
    for (int i = 0; i < 100000; i++) {
        if (dev.tx_descs[idx].status & E1000_TXD_STAT_DD)
            goto desc_free;
    }
    klog(LOG_WARN, "[e1000] TX timeout on desc %d\n", idx);
    return -1;

desc_free:
    memcpy(dev.tx_bufs[idx], buf, len);
    dev.tx_descs[idx].addr   = dev.tx_bufs_phys[idx];
    dev.tx_descs[idx].length = len;
    dev.tx_descs[idx].cso    = 0;
    dev.tx_descs[idx].css    = 0;
    dev.tx_descs[idx].special = 0;
    dev.tx_descs[idx].cmd    = E1000_TXD_CMD_EOP |
                               E1000_TXD_CMD_FCS |
                               E1000_TXD_CMD_RS;
    dev.tx_descs[idx].status = 0;   /* clear DD so HW knows it's ready */

    dev.tx_tail = (idx + 1) % E1000_NUM_TX_DESC;
    e1000_write(E1000_REG_TDT, dev.tx_tail);
    return 0;
}

void e1000_register_rx_callback(e1000_rx_cb_t cb) {
    rx_callback = cb;
}

void e1000_get_mac(u8 mac[6]) {
    memcpy(mac, dev.mac, 6);
}

void e1000_poll(void) {
    if (!e1000_ready) return;

    while (true) {
        u32 idx = dev.rx_tail % E1000_NUM_RX_DESC;
        e1000_rx_desc_t *d = &dev.rx_descs[idx];

        if (!(d->status & E1000_RXD_STAT_DD))
            break;   /* no more completed descriptors */

        u16 len = d->length;
        if (len > 0 && (d->status & E1000_RXD_STAT_EOP)) {
            if (rx_callback) {
                /* Deliver a heap copy so the callback can free it cleanly */
                u8 *frame = kmalloc(len);
                if (frame) {
                    memcpy(frame, dev.rx_bufs[idx], len);
                    rx_callback(frame, len);
                }
            }
        }

        /* Give the descriptor back to hardware */
        d->status = 0;
        d->addr   = dev.rx_bufs_phys[idx];

        dev.rx_tail = (idx + 1) % E1000_NUM_RX_DESC;
        e1000_write(E1000_REG_RDT, idx);
    }
}

/* ── Module descriptor ────────────────────────────────────────────── */
kernel_module_t mod_e1000 = {
    .name        = "e1000",
    .init        = e1000_init_impl,
    .dump        = e1000_dump,
    .initialized = false,
};
