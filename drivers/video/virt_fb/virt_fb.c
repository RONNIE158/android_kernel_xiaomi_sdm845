#include <linux/module.h>
#include <linux/fb.h>
#include <linux/vmalloc.h>
#include <linux/init.h>

static struct fb_info *virt_fb;

static int __init virt_fb_init(void)
{
    virt_fb = framebuffer_alloc(0, NULL);
    if (!virt_fb)
        return -ENOMEM;

    // Misal 1080p 32bpp
    virt_fb->screen_base = vzalloc(1920 * 1080 * 4);
    if (!virt_fb->screen_base) {
        framebuffer_release(virt_fb);
        return -ENOMEM;
    }

    virt_fb->fix.smem_len = 1920 * 1080 * 4;
    virt_fb->var.xres = 1920;
    virt_fb->var.yres = 1080;
    virt_fb->var.bits_per_pixel = 32;

    register_framebuffer(virt_fb);
    pr_info("Virtual framebuffer registered\n");
    return 0;
}

static void __exit virt_fb_exit(void)
{
    unregister_framebuffer(virt_fb);
    framebuffer_release(virt_fb);
    pr_info("Virtual framebuffer unregistered\n");
}

module_init(virt_fb_init);
module_exit(virt_fb_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Bro");
MODULE_DESCRIPTION("Virtual framebuffer for headless/cloud dual display");
