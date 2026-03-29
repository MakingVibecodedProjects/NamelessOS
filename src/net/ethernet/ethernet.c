#include "ethernet.h"
#include "ethernet_internal.h"
#include "../e1000/e1000.h"
#include "../../heap/heap.h"
#include "../../serial/serial.h"
#include "../../lib/string.h"
#include "../../lib/types.h"

/* ── Handler table ─────────────────────────────────────────────────── */
typedef struct {
    u16              ethertype;
    eth_rx_handler_t handler;
} eth_entry_t;

static eth_entry_t handlers[ETH_MAX_HANDLERS];
static u32         handler_count = 0;

/* ── Our MAC address ───────────────────────────────────────────────── */
static u8 our_mac[6];

/* ── ethernet_rx — called by e1000 for every received frame ─────────
   buf is a heap allocation; we must kfree it when done.             */
static void ethernet_rx(u8 *buf, u16 total_len) {
    if (total_len < ETH_HEADER_LEN) {
        kfree(buf);
        return;
    }

    eth_hdr_t *hdr = (eth_hdr_t *)buf;
    u16 type       = ntohs(hdr->type);
    u16 payload_len = (u16)(total_len - ETH_HEADER_LEN);
    const u8 *payload = buf + ETH_HEADER_LEN;

    /* Dispatch to the registered handler for this ethertype */
    for (u32 i = 0; i < handler_count; i++) {
        if (handlers[i].ethertype == type) {
            handlers[i].handler(payload, payload_len, hdr->src);
            break;
        }
    }

    kfree(buf);
}

/* ── Public API ────────────────────────────────────────────────────── */

int ethernet_register(u16 ethertype, eth_rx_handler_t handler) {
    if (handler_count >= ETH_MAX_HANDLERS) return -1;
    handlers[handler_count].ethertype = ethertype;
    handlers[handler_count].handler   = handler;
    handler_count++;
    return 0;
}

int ethernet_send(const u8 dst[6], u16 type,
                  const u8 *payload, u16 len) {
    if (len > ETH_MAX_PAYLOAD) return -1;

    /* Build frame in a temporary heap buffer */
    u16 frame_len = (u16)(ETH_HEADER_LEN + len);
    u8 *frame = kmalloc(frame_len);
    if (!frame) return -1;

    eth_hdr_t *hdr = (eth_hdr_t *)frame;
    memcpy(hdr->dst, dst, 6);
    memcpy(hdr->src, our_mac, 6);
    hdr->type = htons(type);
    memcpy(frame + ETH_HEADER_LEN, payload, len);

    int ret = e1000_send(frame, frame_len);
    kfree(frame);
    return ret;
}

void ethernet_poll(void) {
    e1000_poll();
}

void ethernet_get_mac(u8 mac[6]) {
    memcpy(mac, our_mac, 6);
}

/* ── Module init / dump ─────────────────────────────────────────────── */
static int ethernet_init_impl(void) {
    handler_count = 0;

    /* Retrieve our MAC from the NIC driver */
    e1000_get_mac(our_mac);

    /* Register ourselves as the NIC's receive callback */
    e1000_register_rx_callback(ethernet_rx);

    klog(LOG_INFO,
         "[ethernet] ready — MAC %x:%x:%x:%x:%x:%x\n",
         our_mac[0], our_mac[1], our_mac[2],
         our_mac[3], our_mac[4], our_mac[5]);
    return 0;
}

static void ethernet_dump(void) {
    klog(LOG_INFO, "[ethernet] %u ethertype handler(s) registered\n",
         (unsigned)handler_count);
}

/* ── Module descriptor ─────────────────────────────────────────────── */
kernel_module_t mod_ethernet = {
    .name        = "ethernet",
    .init        = ethernet_init_impl,
    .dump        = ethernet_dump,
    .initialized = false,
};
