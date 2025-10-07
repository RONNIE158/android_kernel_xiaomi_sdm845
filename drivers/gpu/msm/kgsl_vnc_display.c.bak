// SPDX-License-Identifier: GPL-2.0
/*
 * KGSL GPU framebuffer -> VNC streamer (real-time)
 * Author: beluga
 * Safe for multi-FB and virtual input
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/net.h>
#include <linux/in.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/kgsl.h>
#include <linux/mm.h>
#include <linux/uaccess.h>

#define VNC_PORT 5900

static struct socket *vnc_sock;
static struct task_struct *vnc_thread;
static int vnc_running = 1;

/* Ambil buffer GPU dari KGSL 3D device */
static void kgsl_get_gpu_framebuffer(char *buf, size_t size)
{
    struct kgsl_device *device = kgsl_get_default_device();
    struct kgsl_drawable *draw;
    void *ptr;

    if (!device) {
        memset(buf, 0x55, size); // fallback
        return;
    }

    draw = device->default_drawable;
    if (!draw || !draw->gpu_addr) {
        memset(buf, 0xAA, size); // fallback
        return;
    }

    /* Mapping GPU memory read-only */
    ptr = kgsl_map_user_mem(draw, KGSL_MEM_READONLY);
    if (ptr) {
        size_t copy_size = min(size, (size_t)draw->size);
        memcpy(buf, ptr, copy_size);
        kgsl_unmap_user_mem(draw);
    } else {
        memset(buf, 0xCC, size); // fallback
    }
}

/* Input virtual layer (safe, tidak ganggu input lain) */
static void handle_client_input_virtual(struct socket *client)
{
    char buf[256];
    int ret;

    ret = kernel_recvmsg(client, &(struct msghdr){},
                         (struct kvec[]){{.iov_base = buf, .iov_len = sizeof(buf)}},
                         1, sizeof(buf), MSG_DONTWAIT);
    if (ret > 0)
        pr_info("kgsl_vnc_gpu: virtual input %d bytes\n", ret);
}

static int vnc_stream_thread(void *data)
{
    struct sockaddr_in saddr;
    int ret;

    ret = sock_create(AF_INET, SOCK_STREAM, IPPROTO_TCP, &vnc_sock);
    if (ret < 0) return ret;

    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = htonl(INADDR_ANY);
    saddr.sin_port = htons(VNC_PORT);

    ret = vnc_sock->ops->bind(vnc_sock, (struct sockaddr *)&saddr, sizeof(saddr));
    if (ret < 0) goto out;
    ret = vnc_sock->ops->listen(vnc_sock, 1);
    if (ret < 0) goto out;

    pr_info("kgsl_vnc_gpu: waiting for client on port %d...\n", VNC_PORT);

    while (vnc_running) {
        struct socket *client = NULL;
        ret = vnc_sock->ops->accept(vnc_sock, &client, O_NONBLOCK);
        if (ret == 0 && client) {
            pr_info("kgsl_vnc_gpu: client connected\n");

            while (vnc_running) {
                char fb_data[4096];
                kgsl_get_gpu_framebuffer(fb_data, sizeof(fb_data));

                kernel_sendmsg(client, &(struct msghdr){},
                               (struct kvec[]){{.iov_base = fb_data, .iov_len = sizeof(fb_data)}},
                               1, sizeof(fb_data));

                handle_client_input_virtual(client);
                msleep(33); // ~30fps
            }

            sock_release(client);
        }
        msleep(100);
    }

out:
    if (vnc_sock) sock_release(vnc_sock);
    return 0;
}

static int __init kgsl_vnc_gpu_init(void)
{
    pr_info("kgsl_vnc_gpu: init\n");
    vnc_thread = kthread_run(vnc_stream_thread, NULL, "kgsl_vnc_gpu");
    if (IS_ERR(vnc_thread)) return PTR_ERR(vnc_thread);
    return 0;
}

static void __exit kgsl_vnc_gpu_exit(void)
{
    vnc_running = 0;
    if (vnc_thread) kthread_stop(vnc_thread);
    pr_info("kgsl_vnc_gpu: exit\n");
}

module_init(kgsl_vnc_gpu_init);
module_exit(kgsl_vnc_gpu_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("KGSL GPU -> VNC (real-time, safe)");
MODULE_AUTHOR("beluga");
