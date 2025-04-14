// SPDX-License-Identifier: GPL-2.0
#include <linux/syscalls.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/openat2.h>
SYSCALL_DEFINE4(openat2, int, dfd, const char __user *, filename,
                struct open_how __user *, how, size_t, size)
{
  struct open_how k_how;
  if (size != sizeof(struct open_how))
    return -EINVAL;
  if (copy_from_user(&k_how, how, size))
    return -EFAULT;
  return do_sys_open(dfd, filename, k_how.flags, k_how.mode);
}
