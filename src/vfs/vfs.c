#include "vfs.h"
#include "vfs_internal.h"
#include "../lib/string.h"
#include "../serial/serial.h"

/* ── Global root node ────────────────────────────────────────────── */
vfs_node_t *vfs_root = NULL;

/* ── Filesystem registry ─────────────────────────────────────────── */
static filesystem_t *fs_table[VFS_MAX_FS];
static u32           fs_count = 0;

/* ── File-descriptor table ───────────────────────────────────────── */
typedef struct {
    vfs_node_t *node;
    u32         offset;
    bool        open;
} fd_entry_t;

static fd_entry_t fd_table[VFS_MAX_FDS];

/* ── Node vtable helpers ─────────────────────────────────────────── */

i32 vfs_read(vfs_node_t *node, u32 offset, u32 size, u8 *buf) {
    if (!node || !node->ops || !node->ops->read) return -1;
    vfs_node_t *target = (node->flags & VFS_MOUNTPOINT) ? node->ptr : node;
    return target->ops->read(target, offset, size, buf);
}

i32 vfs_write(vfs_node_t *node, u32 offset, u32 size, const u8 *buf) {
    if (!node || !node->ops || !node->ops->write) return -1;
    vfs_node_t *target = (node->flags & VFS_MOUNTPOINT) ? node->ptr : node;
    return target->ops->write(target, offset, size, buf);
}

int vfs_open(vfs_node_t *node) {
    if (!node) return -1;
    vfs_node_t *target = (node->flags & VFS_MOUNTPOINT) ? node->ptr : node;
    if (target->ops && target->ops->open)
        return target->ops->open(target);
    return 0;
}

void vfs_close(vfs_node_t *node) {
    if (!node) return;
    vfs_node_t *target = (node->flags & VFS_MOUNTPOINT) ? node->ptr : node;
    if (target->ops && target->ops->close)
        target->ops->close(target);
}

int vfs_readdir(vfs_node_t *node, u32 index, vfs_node_t *out_node) {
    if (!node || !(node->flags & VFS_DIR)) return -1;
    vfs_node_t *target = (node->flags & VFS_MOUNTPOINT) ? node->ptr : node;
    if (!target->ops || !target->ops->readdir) return -1;
    return target->ops->readdir(target, index, out_node);
}

int vfs_finddir(vfs_node_t *node, const char *name, vfs_node_t *out_node) {
    if (!node || !(node->flags & VFS_DIR)) return -1;
    vfs_node_t *target = (node->flags & VFS_MOUNTPOINT) ? node->ptr : node;
    if (!target->ops || !target->ops->finddir) return -1;
    return target->ops->finddir(target, name, out_node);
}

/* ── Filesystem registration ─────────────────────────────────────── */

int vfs_register_fs(filesystem_t *fs) {
    if (!fs || fs_count >= VFS_MAX_FS) return -1;
    fs_table[fs_count++] = fs;
    klog(LOG_DEBUG, "[vfs] registered filesystem: %s", fs->name);
    return 0;
}

int vfs_mount(const char *fs_name, const char *path, vfs_node_t *dev) {
    /* Find the filesystem backend */
    filesystem_t *fs = NULL;
    for (u32 i = 0; i < fs_count; i++) {
        if (strcmp(fs_table[i]->name, fs_name) == 0) {
            fs = fs_table[i];
            break;
        }
    }
    if (!fs) {
        klog(LOG_WARN, "[vfs] mount: filesystem '%s' not registered", fs_name);
        return -1;
    }

    vfs_node_t *root = fs->mount(dev);
    if (!root) {
        klog(LOG_WARN, "[vfs] mount: %s failed to mount", fs_name);
        return -1;
    }

    /* If mounting on "/", set vfs_root directly */
    if (strcmp(path, "/") == 0) {
        vfs_root = root;
        klog(LOG_INFO, "[vfs] mounted %s on /", fs_name);
        return 0;
    }

    /* Otherwise find the target node and make it a mountpoint */
    if (!vfs_root) {
        klog(LOG_WARN, "[vfs] mount: no root, cannot mount on %s", path);
        return -1;
    }

    /* Simple single-level path walk: only supports "/dir" */
    const char *name = path;
    if (*name == '/') name++;
    vfs_node_t target;
    if (vfs_finddir(vfs_root, name, &target) != 0) {
        klog(LOG_WARN, "[vfs] mount: path not found: %s", path);
        return -1;
    }
    target.flags  |= VFS_MOUNTPOINT;
    target.ptr     = root;
    klog(LOG_INFO, "[vfs] mounted %s on %s", fs_name, path);
    return 0;
}

/* ── File-descriptor table ───────────────────────────────────────── */

int vfs_fd_open(vfs_node_t *node) {
    if (!node) return -1;
    for (int i = 0; i < VFS_MAX_FDS; i++) {
        if (!fd_table[i].open) {
            fd_table[i].node   = node;
            fd_table[i].offset = 0;
            fd_table[i].open   = true;
            vfs_open(node);
            return i;
        }
    }
    return -1;   /* out of file descriptors */
}

i32 vfs_fd_read(int fd, u32 size, u8 *buf) {
    if (fd < 0 || fd >= VFS_MAX_FDS || !fd_table[fd].open) return -1;
    fd_entry_t *e   = &fd_table[fd];
    i32         ret = vfs_read(e->node, e->offset, size, buf);
    if (ret > 0) e->offset += (u32)ret;
    return ret;
}

i32 vfs_fd_write(int fd, u32 size, const u8 *buf) {
    if (fd < 0 || fd >= VFS_MAX_FDS || !fd_table[fd].open) return -1;
    fd_entry_t *e   = &fd_table[fd];
    i32         ret = vfs_write(e->node, e->offset, size, buf);
    if (ret > 0) e->offset += (u32)ret;
    return ret;
}

void vfs_fd_close(int fd) {
    if (fd < 0 || fd >= VFS_MAX_FDS || !fd_table[fd].open) return;
    vfs_close(fd_table[fd].node);
    fd_table[fd].open   = false;
    fd_table[fd].node   = NULL;
    fd_table[fd].offset = 0;
}

/* ── vfs_dump ────────────────────────────────────────────────────── */
static void vfs_dump(void) {
    u32 open_fds = 0;
    for (int i = 0; i < VFS_MAX_FDS; i++)
        if (fd_table[i].open) open_fds++;
    klog(LOG_DEBUG, "[vfs] root=%s fs_count=%u open_fds=%u",
         vfs_root ? vfs_root->name : "(none)",
         (unsigned)fs_count, (unsigned)open_fds);
}

/* ── vfs_init ────────────────────────────────────────────────────── */
int vfs_init(void) {
    fs_count = 0;
    vfs_root = NULL;
    for (int i = 0; i < VFS_MAX_FDS; i++) {
        fd_table[i].open   = false;
        fd_table[i].node   = NULL;
        fd_table[i].offset = 0;
    }
    klog(LOG_INFO, "[vfs] VFS ready (max %d fs, %d fds)",
         VFS_MAX_FS, VFS_MAX_FDS);
    return 0;
}

/* ── module descriptor ───────────────────────────────────────────── */
kernel_module_t mod_vfs = {
    .name        = "vfs",
    .initialized = false,
    .init        = vfs_init,
    .dump        = vfs_dump,
    .shutdown    = NULL,
};
