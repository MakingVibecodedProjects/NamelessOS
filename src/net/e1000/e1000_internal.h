/* e1000_internal.h — private constants and structures for the Intel e1000 driver */
#ifndef E1000_INTERNAL_H
#define E1000_INTERNAL_H

#include "../../lib/types.h"

/* ── PCI identity ─────────────────────────────────────────────────── */
#define E1000_VENDOR_ID     0x8086
#define E1000_DEVICE_ID     0x100E   /* 82540EM (QEMU default) */

/* ── MMIO register offsets ───────────────────────────────────────── */
#define E1000_REG_CTRL      0x0000   /* Device Control */
#define E1000_REG_STATUS    0x0008   /* Device Status */
#define E1000_REG_EECD      0x0010   /* EEPROM/Flash Control */
#define E1000_REG_EERD      0x0014   /* EEPROM Read */
#define E1000_REG_ICR       0x00C0   /* Interrupt Cause Read */
#define E1000_REG_IMS       0x00D0   /* Interrupt Mask Set */
#define E1000_REG_IMC       0x00D8   /* Interrupt Mask Clear */
#define E1000_REG_RCTL      0x0100   /* Receive Control */
#define E1000_REG_TCTL      0x0400   /* Transmit Control */
#define E1000_REG_TIPG      0x0410   /* Transmit IPG */
#define E1000_REG_RDBAL     0x2800   /* RX Desc Base Addr Low */
#define E1000_REG_RDBAH     0x2804   /* RX Desc Base Addr High */
#define E1000_REG_RDLEN     0x2808   /* RX Desc Ring Length (bytes) */
#define E1000_REG_RDH       0x2810   /* RX Desc Head */
#define E1000_REG_RDT       0x2818   /* RX Desc Tail */
#define E1000_REG_TDBAL     0x3800   /* TX Desc Base Addr Low */
#define E1000_REG_TDBAH     0x3804   /* TX Desc Base Addr High */
#define E1000_REG_TDLEN     0x3808   /* TX Desc Ring Length (bytes) */
#define E1000_REG_TDH       0x3810   /* TX Desc Head */
#define E1000_REG_TDT       0x3818   /* TX Desc Tail */
#define E1000_REG_MTA       0x5200   /* Multicast Table Array (128 × 4 B) */
#define E1000_REG_RAL0      0x5400   /* Receive Address Low  [0] */
#define E1000_REG_RAH0      0x5404   /* Receive Address High [0] */

/* ── CTRL bits ────────────────────────────────────────────────────── */
#define E1000_CTRL_RST      (1u << 26)  /* Full reset */
#define E1000_CTRL_ASDE     (1u <<  5)  /* Auto-speed detect enable */
#define E1000_CTRL_SLU      (1u <<  6)  /* Set link up */

/* ── RCTL bits ────────────────────────────────────────────────────── */
#define E1000_RCTL_EN       (1u <<  1)  /* Receive enable */
#define E1000_RCTL_SBP      (1u <<  2)  /* Store bad packets */
#define E1000_RCTL_UPE      (1u <<  3)  /* Unicast promiscuous */
#define E1000_RCTL_MPE      (1u <<  4)  /* Multicast promiscuous */
#define E1000_RCTL_LPE      (1u <<  5)  /* Long packet enable */
#define E1000_RCTL_BAM      (1u << 15)  /* Broadcast accept mode */
#define E1000_RCTL_BSIZE_2K (0u << 16)  /* Buffer size 2 KB (default) */
#define E1000_RCTL_SECRC    (1u << 26)  /* Strip Ethernet CRC */

/* ── TCTL bits ────────────────────────────────────────────────────── */
#define E1000_TCTL_EN       (1u <<  1)  /* Transmit enable */
#define E1000_TCTL_PSP      (1u <<  3)  /* Pad short packets */
#define E1000_TCTL_CT_SHIFT  4          /* Collision threshold field */
#define E1000_TCTL_COLD_SHIFT 12        /* Collision distance field */

/* ── EERD bits ────────────────────────────────────────────────────── */
#define E1000_EERD_START    (1u <<  0)
#define E1000_EERD_DONE     (1u <<  4)
#define E1000_EERD_ADDR_SHIFT 8
#define E1000_EERD_DATA_SHIFT 16

/* ── RAH bits ─────────────────────────────────────────────────────── */
#define E1000_RAH_AV        (1u << 31)  /* Address valid */

/* ── Interrupt bits ───────────────────────────────────────────────── */
#define E1000_ICR_TXDW      (1u <<  0)  /* TX descriptor written back */
#define E1000_ICR_TXQE      (1u <<  1)  /* TX queue empty */
#define E1000_ICR_LSC       (1u <<  2)  /* Link status change */
#define E1000_ICR_RXDMT0    (1u <<  4)  /* RX desc min threshold */
#define E1000_ICR_RXO       (1u <<  6)  /* RX overrun */
#define E1000_ICR_RXT0      (1u <<  7)  /* RX timer */

/* ── Descriptor ring sizes (must be multiples of 8) ──────────────── */
#define E1000_NUM_RX_DESC   32
#define E1000_NUM_TX_DESC   32

/* ── Buffer sizes ─────────────────────────────────────────────────── */
#define E1000_RX_BUF_SIZE   2048
#define E1000_MAX_FRAME     1518

/* ── RX descriptor (hardware layout, must be 16 bytes) ──────────── */
typedef struct __attribute__((packed)) {
    u64  addr;       /* Physical address of receive buffer */
    u16  length;     /* Bytes received */
    u16  checksum;
    u8   status;     /* Status bits */
    u8   errors;
    u16  special;
} e1000_rx_desc_t;

#define E1000_RXD_STAT_DD   (1u << 0)   /* Descriptor done */
#define E1000_RXD_STAT_EOP  (1u << 1)   /* End of packet */

/* ── TX descriptor (hardware layout, must be 16 bytes) ──────────── */
typedef struct __attribute__((packed)) {
    u64  addr;       /* Physical address of transmit buffer */
    u16  length;     /* Bytes to send */
    u8   cso;        /* Checksum offset */
    u8   cmd;        /* Command byte */
    u8   status;     /* Status bits */
    u8   css;        /* Checksum start */
    u16  special;
} e1000_tx_desc_t;

#define E1000_TXD_CMD_EOP   (1u << 0)   /* End of packet */
#define E1000_TXD_CMD_FCS   (1u << 1)   /* Insert FCS/CRC */
#define E1000_TXD_CMD_RS    (1u << 3)   /* Report status */
#define E1000_TXD_STAT_DD   (1u << 0)   /* Descriptor done */

/* ── Driver state ─────────────────────────────────────────────────── */
typedef struct {
    volatile u8     *mmio;          /* MMIO base (kernel VA) */
    u8               mac[6];        /* Our MAC address */

    /* RX ring */
    e1000_rx_desc_t *rx_descs;      /* Virtual address of descriptor ring */
    u64              rx_descs_phys; /* Physical address */
    u8              *rx_bufs[E1000_NUM_RX_DESC];
    u64              rx_bufs_phys[E1000_NUM_RX_DESC];
    u32              rx_tail;       /* Next desc to check for completed RX */

    /* TX ring */
    e1000_tx_desc_t *tx_descs;
    u64              tx_descs_phys;
    u8              *tx_bufs[E1000_NUM_TX_DESC];
    u64              tx_bufs_phys[E1000_NUM_TX_DESC];
    u32              tx_tail;       /* Next desc to use for TX */
} e1000_dev_t;

#endif /* E1000_INTERNAL_H */
