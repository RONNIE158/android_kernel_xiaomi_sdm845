// SPDX-License-Identifier: GPL-2.0
#include <linux/syscalls.h>
#include <linux/fs.h>
#include <linux/mount.h>
#include <linux/uaccess.h>
SYSCALL_DEFINE5(mount_setattr, int, dfd, const char __user *, path,
                unsigned int, flags, struct mount_attr __user *, uattr, size_t, size)
{
    if (size != sizeof(struct mount_attr))
        return -EINVAL;
    return mount_setattr_internal(dfd, path, flags, uattr);
}
