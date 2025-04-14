// SPDX-License-Identifier: GPL-2.0
#include <linux/syscalls.h>
#include <linux/fs.h>
#include <linux/mount.h>
#include <linux/uaccess.h>
SYSCALL_DEFINE5(move_mount, int, from_dfd, const char __user *, from_pathname,
                int, to_dfd, const char __user *, to_pathname, unsigned int, flags)
{
    if (flags & MOUNT_BIND) {
        return move_mount_internal(from_dfd, from_pathname, to_dfd, to_pathname, flags);
    }
    return -EINVAL;
}
