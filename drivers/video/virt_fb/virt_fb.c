#include <linux/module.h>
#include <linux/fb.h>
#include <linux/vmalloc.h>
#include <linux/init.h>
#include <linux/string.h>

static struct fb_info *virt_fb;

static struct fb_ops virt_fb_ops = {
    .owner        = THIS_MODULE,
    .fb_read      = fb_sys_read,
    .fb_write     = fb_sys_write,
    .fb_fillrect  = sys_fillrect,
    .fb_copyarea  = sys_copyarea,
    .fb_imageblit = sys_imageblit,
};

static int __init virt_fb_init(void)
{
    int ret;

    virt_fb = framebuffer_alloc(0, NULL);
    if (!virt_fb)
        return -ENOMEM;

    /* Resolusi default: 1080p 32bpp */
    u32 width = 1920, height = 1080, bpp = 32;
    size_t fb_size = width * height * (bpp / 8);

    virt_fb->screen_base = vzalloc(fb_size);
    if (!virt_fb->screen_base) {
        framebuffer_release(virt_fb);
        return -ENOMEM;
    }

    virt_fb->fbops = &virt_fb_ops;
    virt_fb->fix = (struct fb_fix_screeninfo){
        .id = "virt_fb",
        .smem_start = (unsigned long)virt_fb->screen_base,
        .smem_len = fb_size,
        .type = FB_TYPE_PACKED_PIXELS,
        .visual = FB_VISUAL_TRUECOLOR,
        .line_length = width * (bpp / 8),
    };

    virt_fb->var = (struct fb_var_screeninfo){
        .xres = width,
        .yres = height,
        .xres_virtual = width,
        .yres_virtual = height,
        .bits_per_pixel = bpp,
        .red = {16, 8, 0},
        .green = {8, 8, 0},
        .blue = {0, 8, 0},
        .activate = FB_ACTIVATE_NOW,
    };

    ret = register_framebuffer(virt_fb);
    if (ret < 0) {
        vfree(virt_fb->screen_base);
        framebuffer_release(virt_fb);
        return ret;
    }

    pr_info("Virtual framebuffer registered (%ux%u@%u)\n",
            width, height, bpp);
    return 0;
}

static void __exit virt_fb_exit(void)
{
    unregister_framebuffer(virt_fb);
    vfree(virt_fb->screen_base);
    framebuffer_release(virt_fb);
    pr_info("Virtual framebuffer unregistered\n");
}

module_init(virt_fb_init);
module_exit(virt_fb_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Bro");
MODULE_DESCRIPTION("Virtual framebuffer for headless/cloud dual display");
