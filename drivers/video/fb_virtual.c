// SPDX-License-Identifier: GPL-2.0
/*
 * Hybrid Virtual Framebuffer + VNC streamer
 * Author: Bro
 * Description: Virtual framebuffer untuk headless Android, bisa di-VNC-kan real-time
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/fb.h>
#include <linux/vmalloc.h>
#include <linux/kthread.h>
#include <linux/net.h>
#include <linux/in.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/uaccess.h>

#define VNC_PORT 5900
#define WIDTH 1920
#define HEIGHT 1080
#define BPP 32

static struct fb_info *virt_fbinfo;
static struct socket *vnc_sock;
static struct task_struct *vnc_thread;
static int vnc_running = 1;
static char *vnc_fb_buffer; // buffer heap full-frame

/* ----- Gabut test ----- */
static void gabut(void)
{
    pr_info("virt_fb_vnc: Gabut! Modul berhasil dipanggil.\n");
}

/* ----- Virtual framebuffer init/exit ----- */
static int __init virt_fb_init(void)
{
    int ret;

    virt_fbinfo = framebuffer_alloc(0, NULL);
    if (!virt_fbinfo)
        return -ENOMEM;

    virt_fbinfo->screen_base = vzalloc(WIDTH * HEIGHT * (BPP/8));
    if (!virt_fbinfo->screen_base) {
        framebuffer_release(virt_fbinfo);
        return -ENOMEM;
    }

    virt_fbinfo->fix.smem_len = WIDTH * HEIGHT * (BPP/8);
    virt_fbinfo->var.xres = WIDTH;
    virt_fbinfo->var.yres = HEIGHT;
    virt_fbinfo->var.bits_per_pixel = BPP;

    ret = register_framebuffer(virt_fbinfo);
    if (ret < 0) {
        framebuffer_release(virt_fbinfo);
        return ret;
    }

    pr_info("virt_fb: Virtual framebuffer registered (%dx%d)\n", WIDTH, HEIGHT);

    gabut(); // panggilan log untuk test modul

    return 0;
}

static void __exit virt_fb_exit(void)
{
    unregister_framebuffer(virt_fbinfo);
    framebuffer_release(virt_fbinfo);
    pr_info("virt_fb: Virtual framebuffer unregistered\n");
}

/* ----- Ambil buffer framebuffer ----- */
static void fb_get_framebuffer(char *buf, size_t size)
{
    size_t copy_size;

    if (!virt_fbinfo || !virt_fbinfo->screen_base) {
        memset(buf, 0xFF, size); // fallback
        return;
    }

    copy_size = min(size, (size_t)virt_fbinfo->fix.smem_len);
    memcpy(buf, virt_fbinfo->screen_base, copy_size);
}

/* ----- Input virtual (dummy) ----- */
static void handle_client_input_virtual(struct socket *client)
{
    char buf[256];
    int ret;

    ret = kernel_recvmsg(client, &(struct msghdr){0},
                         (struct kvec[]){{.iov_base = buf, .iov_len = sizeof(buf)}},
                         1, sizeof(buf), MSG_DONTWAIT);
    if (ret > 0)
        pr_info("virt_fb_vnc: virtual input %d bytes\n", ret);
}

/* ----- Thread VNC ----- */
static int vnc_stream_thread(void *data)
{
    struct sockaddr_in saddr;
    struct socket *client;
    int ret;
    int fb_size;

    fb_size = WIDTH * HEIGHT * (BPP/8);

    vnc_fb_buffer = kmalloc(fb_size, GFP_KERNEL);
    if (!vnc_fb_buffer)
        return -ENOMEM;

    ret = sock_create(AF_INET, SOCK_STREAM, IPPROTO_TCP, &vnc_sock);
    if (ret < 0) goto out_free;

    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = htonl(INADDR_ANY);
    saddr.sin_port = htons(VNC_PORT);

    ret = vnc_sock->ops->bind(vnc_sock, (struct sockaddr *)&saddr, sizeof(saddr));
    if (ret < 0) goto out_sock;
    ret = vnc_sock->ops->listen(vnc_sock, 1);
    if (ret < 0) goto out_sock;

    pr_info("virt_fb_vnc: waiting for client on port %d...\n", VNC_PORT);

    while (vnc_running) {
        client = NULL;
        ret = vnc_sock->ops->accept(vnc_sock, &client, O_NONBLOCK);
        if (ret == 0 && client) {
            pr_info("virt_fb_vnc: client connected\n");

            while (vnc_running) {
                fb_get_framebuffer(vnc_fb_buffer, fb_size);

                kernel_sendmsg(client, &(struct msghdr){0},
                               (struct kvec[]){{.iov_base = vnc_fb_buffer, .iov_len = fb_size}},
                               1, fb_size);

                handle_client_input_virtual(client);
                msleep(33); // ~30fps
            }

            sock_release(client);
        }
        msleep(100);
    }

out_sock:
    if (vnc_sock)
        sock_release(vnc_sock);
out_free:
    kfree(vnc_fb_buffer);
    return ret;
}

/* ----- Module init/exit ----- */
static int __init virt_fb_vnc_init(void)
{
    int ret = virt_fb_init();
    if (ret) return ret;

    vnc_thread = kthread_run(vnc_stream_thread, NULL, "virt_fb_vnc");
    if (IS_ERR(vnc_thread)) return PTR_ERR(vnc_thread);

    pr_info("virt_fb_vnc: module initialized\n");
    return 0;
}

static void __exit virt_fb_vnc_exit(void)
{
    vnc_running = 0;
    if (vnc_thread)
        kthread_stop(vnc_thread);

    virt_fb_exit();
    pr_info("virt_fb_vnc: module exited\n");
}

module_init(virt_fb_vnc_init);
module_exit(virt_fb_vnc_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Bro");
MODULE_DESCRIPTION("Hybrid Virtual Framebuffer + VNC streamer for Android headless");
