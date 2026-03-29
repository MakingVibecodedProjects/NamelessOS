#include "tmpfs.h"
#include "tmpfs_internal.h"
#include "../lib/string.h"
#include "../serial/serial.h"
#include "../heap/heap.h"
#include "../vfs/vfs.h"

/* ── Inode pool ──────────────────────────────────────────────────── */
static tmpfs_inode_t inode_pool[TMPFS_MAX_NODES];

/* ── VFS node pool (one per open inode reference) ────────────────── */
static vfs_node_t node_pool[TMPFS_MAX_NODES];

/* Forward declaration of ops */
static fs_ops_t tmpfs_ops;

/* ── alloc_inode ─────────────────────────────────────────────────── */
static tmpfs_inode_t *alloc_inode(void) {
    for (int i = 0; i < TMPFS_MAX_NODES; i++) {
        if (!inode_pool[i].used) {
            inode_pool[i].used        = true;
            inode_pool[i].child_count = 0;
            inode_pool[i].data        = NULL;
            inode_pool[i].size        = 0;
            return &inode_pool[i];
        }
    }
    return NULL;
}

/* ── inode_to_node ───────────────────────────────────────────────── */
static void inode_to_node(tmpfs_inode_t *in, vfs_node_t *out) {
    strncpy(out->name, in->name, VFS_NAME_MAX - 1);
    out->name[VFS_NAME_MAX - 1] = '\0';
    out->flags = in->flags;
    out->inode = (u32)(usize)(in - inode_pool);
    out->size  = in->size;
    out->uid   = 0;
    out->gid   = 0;
    out->mask  = 0755;
    out->ops   = &tmpfs_ops;
    out->ptr   = NULL;
}

/* ── node_to_inode ───────────────────────────────────────────────── */
static tmpfs_inode_t *node_to_inode(vfs_node_t *node) {
    if (node->inode >= TMPFS_MAX_NODES) return NULL;
    return &inode_pool[node->inode];
}

/* ── fs_ops implementations ──────────────────────────────────────── */

static i32 tmpfs_read(vfs_node_t *node, u32 offset, u32 size, u8 *buf) {
    tmpfs_inode_t *in = node_to_inode(node);
    if (!in || !(in->flags & VFS_FILE)) return -1;
    if (offset >= in->size) return 0;
    u32 avail = in->size - offset;
    u32 n     = (size < avail) ? size : avail;
    memcpy(buf, in->data + offset, n);
    return (i32)n;
}

static i32 tmpfs_write(vfs_node_t *node, u32 offset, u32 size, const u8 *buf) {
    tmpfs_inode_t *in = node_to_inode(node);
    if (!in || !(in->flags & VFS_FILE)) return -1;

    u32 end = offset + size;
    if (end > TMPFS_MAX_FILE_SIZE) return -1;

    if (end > in->size) {
        u8 *newdata = (u8 *)krealloc(in->data, end);
        if (!newdata) return -1;
        /* Zero any gap between old size and new offset */
        if (offset > in->size)
            memset(newdata + in->size, 0, offset - in->size);
        in->data = newdata;
        in->size = end;
        node->size = end;
    }
    memcpy(in->data + offset, buf, size);
    return (i32)size;
}

static int tmpfs_open(vfs_node_t *node) {
    (void)node;
    return 0;
}

static void tmpfs_close(vfs_node_t *node) {
    (void)node;
}

static int tmpfs_readdir(vfs_node_t *node, u32 index, vfs_node_t *out) {
    tmpfs_inode_t *in = node_to_inode(node);
    if (!in || !(in->flags & VFS_DIR)) return -1;
    if (index >= in->child_count) return -1;
    inode_to_node(in->children[index], out);
    return 0;
}

static int tmpfs_finddir(vfs_node_t *node, const char *name, vfs_node_t *out) {
    tmpfs_inode_t *in = node_to_inode(node);
    if (!in || !(in->flags & VFS_DIR)) return -1;
    for (u32 i = 0; i < in->child_count; i++) {
        if (strcmp(in->children[i]->name, name) == 0) {
            inode_to_node(in->children[i], out);
            return 0;
        }
    }
    return -1;
}

static fs_ops_t tmpfs_ops = {
    .read    = tmpfs_read,
    .write   = tmpfs_write,
    .open    = tmpfs_open,
    .close   = tmpfs_close,
    .readdir = tmpfs_readdir,
    .finddir = tmpfs_finddir,
};

/* ── tmpfs_mount ─────────────────────────────────────────────────── */
static vfs_node_t *tmpfs_mount(vfs_node_t *dev) {
    (void)dev;
    tmpfs_inode_t *root = alloc_inode();
    if (!root) return NULL;
    strncpy(root->name, "/", sizeof(root->name) - 1);
    root->flags = VFS_DIR;
    inode_to_node(root, &node_pool[0]);
    return &node_pool[0];
}

/* ── Public: create file/dir inside tmpfs ────────────────────────── */
tmpfs_inode_t *tmpfs_create(vfs_node_t *parent, const char *name, u32 flags) {
    tmpfs_inode_t *pin = node_to_inode(parent);
    if (!pin || !(pin->flags & VFS_DIR)) return NULL;
    if (pin->child_count >= TMPFS_MAX_CHILDREN) return NULL;

    tmpfs_inode_t *in = alloc_inode();
    if (!in) return NULL;
    strncpy(in->name, name, sizeof(in->name) - 1);
    in->flags = flags;
    pin->children[pin->child_count++] = in;
    return in;
}

/* ── Public inode_to_node wrapper ───────────────────────────────── */
void tmpfs_inode_to_node(tmpfs_inode_t *in, vfs_node_t *out) {
    inode_to_node(in, out);
}

/* ── filesystem_t descriptor ─────────────────────────────────────── */
filesystem_t tmpfs_fs = {
    .name  = "tmpfs",
    .mount = tmpfs_mount,
};

/* ── tmpfs_dump ──────────────────────────────────────────────────── */
static void tmpfs_dump(void) {
    u32 used = 0;
    for (int i = 0; i < TMPFS_MAX_NODES; i++)
        if (inode_pool[i].used) used++;
    klog(LOG_DEBUG, "[tmpfs] %u/%u inodes used", used, (u32)TMPFS_MAX_NODES);
}

/* ── tmpfs_init ──────────────────────────────────────────────────── */
int tmpfs_init(void) {
    /* Clear inode pool */
    for (int i = 0; i < TMPFS_MAX_NODES; i++)
        inode_pool[i].used = false;

    vfs_register_fs(&tmpfs_fs);
    if (vfs_mount("tmpfs", "/", NULL) != 0) {
        klog(LOG_ERROR, "[tmpfs] failed to mount on /");
        return -1;
    }
    klog(LOG_INFO, "[tmpfs] mounted on /");
    return 0;
}

/* ── module descriptor ───────────────────────────────────────────── */
kernel_module_t mod_tmpfs = {
    .name        = "tmpfs",
    .initialized = false,
    .init        = tmpfs_init,
    .dump        = tmpfs_dump,
    .shutdown    = NULL,
};
