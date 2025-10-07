// drivers/video/fb_virtual/fb_virtual.c
#include <linux/module.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/vmalloc.h>

static struct fb_info *virt_fbinfo;

static int __init virt_fb_init(void)
{
    virt_fbinfo = framebuffer_alloc(0, NULL);
    if (!virt_fbinfo)
        return -ENOMEM;

    virt_fbinfo->screen_base = vzalloc(1920*1080*4);
    if (!virt_fbinfo->screen_base) {
        framebuffer_release(virt_fbinfo);
        return -ENOMEM;
    }

    virt_fbinfo->fix.smem_len = 1920*1080*4;
    virt_fbinfo->var.xres = 1920;
    virt_fbinfo->var.yres = 1080;
    virt_fbinfo->var.bits_per_pixel = 32;

    register_framebuffer(virt_fbinfo);
    pr_info("Virtual framebuffer registered\n");
    return 0;
}

static void __exit virt_fb_exit(void)
{
    unregister_framebuffer(virt_fbinfo);
    framebuffer_release(virt_fbinfo);
    pr_info("Virtual framebuffer unregistered\n");
}

module_init(virt_fb_init);
module_exit(virt_fb_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Bro");
MODULE_DESCRIPTION("Hybrid Virtual Framebuffer for Headless Android");
