#include "devfs.h"
#include "../serial/serial.h"
#include "../lib/string.h"
#include "../vfs/vfs.h"
#include "../tmpfs/tmpfs.h"
#include "../tmpfs/tmpfs_internal.h"

/* ── Device ops table ────────────────────────────────────────────── */
#define DEVFS_MAX_DEVICES 16

typedef struct {
    char      name[32];
    fs_ops_t *ops;
} devfs_entry_t;

static devfs_entry_t dev_table[DEVFS_MAX_DEVICES];
static u32           dev_count = 0;

/* ── /dev directory node ─────────────────────────────────────────── */
static vfs_node_t  dev_dir;
static fs_ops_t    dev_dir_ops;   /* copy of tmpfs ops + our finddir */

/* ── /dev/null ops ───────────────────────────────────────────────── */
static i32 null_read(vfs_node_t *n, u32 off, u32 sz, u8 *buf) {
    (void)n; (void)off; (void)sz; (void)buf;
    return 0;
}
static i32 null_write(vfs_node_t *n, u32 off, u32 sz, const u8 *buf) {
    (void)n; (void)off; (void)buf;
    return (i32)sz;
}
static fs_ops_t null_ops = {
    .read = null_read, .write = null_write,
    .open = NULL, .close = NULL, .readdir = NULL, .finddir = NULL,
};

/* ── /dev/zero ops ───────────────────────────────────────────────── */
static i32 zero_read(vfs_node_t *n, u32 off, u32 sz, u8 *buf) {
    (void)n; (void)off;
    memset(buf, 0, sz);
    return (i32)sz;
}
static fs_ops_t zero_ops = {
    .read = zero_read, .write = null_write,
    .open = NULL, .close = NULL, .readdir = NULL, .finddir = NULL,
};

/* ── dev_finddir: tmpfs finddir + ops patch ──────────────────────── */
static int dev_finddir(vfs_node_t *node, const char *name, vfs_node_t *out) {
    /* Use tmpfs's own finddir to resolve the inode */
    fs_ops_t *orig = (fs_ops_t *)node->ptr;   /* stashed below */
    if (!orig || !orig->finddir) return -1;
    int ret = orig->finddir(node, name, out);
    if (ret != 0) return -1;
    /* Patch ops to device-specific vtable */
    for (u32 i = 0; i < dev_count; i++) {
        if (strcmp(dev_table[i].name, name) == 0) {
            out->ops = dev_table[i].ops;
            return 0;
        }
    }
    return 0;
}

/* ── devfs_register ──────────────────────────────────────────────── */
int devfs_register(const char *name, u32 flags, fs_ops_t *ops) {
    if (dev_count >= DEVFS_MAX_DEVICES) return -1;
    /* Create inode under /dev */
    tmpfs_inode_t *in = tmpfs_create(&dev_dir, name, flags);
    if (!in) return -1;
    /* Store ops in table */
    strncpy(dev_table[dev_count].name, name,
            sizeof(dev_table[dev_count].name) - 1);
    dev_table[dev_count].ops = ops;
    dev_count++;
    klog(LOG_DEBUG, "[devfs] registered /dev/%s", name);
    return 0;
}

/* ── devfs_get_node ──────────────────────────────────────────────── */
int devfs_get_node(const char *name, vfs_node_t *out) {
    for (u32 i = 0; i < dev_count; i++) {
        if (strcmp(dev_table[i].name, name) == 0) {
            /* Find the tmpfs inode by calling dev_finddir on dev_dir */
            return dev_finddir(&dev_dir, name, out);
        }
    }
    return -1;
}

/* ── devfs_dump ──────────────────────────────────────────────────── */
static void devfs_dump(void) {
    klog(LOG_DEBUG, "[devfs] %u device(s) under /dev", (unsigned)dev_count);
}

/* ── devfs_init ──────────────────────────────────────────────────── */
int devfs_init(void) {
    dev_count = 0;

    if (!vfs_root) {
        klog(LOG_ERROR, "[devfs] no vfs_root — tmpfs not mounted");
        return -1;
    }

    /* Create /dev dir in tmpfs root */
    tmpfs_inode_t *dev_inode = tmpfs_create(vfs_root, "dev", VFS_DIR);
    if (!dev_inode) {
        klog(LOG_ERROR, "[devfs] failed to create /dev inode");
        return -1;
    }
    tmpfs_inode_to_node(dev_inode, &dev_dir);

    /* Wrap the tmpfs ops: copy them, stash the original in ptr, override finddir */
    dev_dir_ops         = *dev_dir.ops;
    dev_dir_ops.finddir = dev_finddir;
    dev_dir.ptr         = (vfs_node_t *)dev_dir.ops;   /* stash original ops */
    dev_dir.ops         = &dev_dir_ops;

    /* Register built-in devices */
    devfs_register("null", VFS_CHARDEV, &null_ops);
    devfs_register("zero", VFS_CHARDEV, &zero_ops);

    klog(LOG_INFO, "[devfs] /dev ready (%u devices)", (unsigned)dev_count);
    return 0;
}

/* ── module descriptor ───────────────────────────────────────────── */
kernel_module_t mod_devfs = {
    .name        = "devfs",
    .initialized = false,
    .init        = devfs_init,
    .dump        = devfs_dump,
    .shutdown    = NULL,
};
