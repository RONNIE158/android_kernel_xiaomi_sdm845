#include <linux/module.h>
#include <linux/fb.h>
#include <linux/vmalloc.h>
#include <linux/init.h>
#include <linux/string.h>

static struct fb_info *virt_fb;

/* Dummy ops karena kernel tidak punya fb_sys_* */
static void virt_fb_fillrect(struct fb_info *info, const struct fb_fillrect *rect) {}
static void virt_fb_copyarea(struct fb_info *info, const struct fb_copyarea *area) {}
static void virt_fb_imageblit(struct fb_info *info, const struct fb_image *image) {}

static struct fb_ops virt_fb_ops = {
    .owner        = THIS_MODULE,
    .fb_fillrect  = virt_fb_fillrect,
    .fb_copyarea  = virt_fb_copyarea,
    .fb_imageblit = virt_fb_imageblit,
};

static int __init virt_fb_init(void)
{
    int ret;
    unsigned int width, height, bpp;
    size_t fb_size;

    width = 1920;
    height = 1080;
    bpp = 32;
    fb_size = width * height * (bpp / 8);

    virt_fb = framebuffer_alloc(0, NULL);
    if (!virt_fb)
        return -ENOMEM;

    virt_fb->screen_base = vzalloc(fb_size);
    if (!virt_fb->screen_base) {
        framebuffer_release(virt_fb);
        return -ENOMEM;
    }

    virt_fb->fbops = &virt_fb_ops;

    snprintf(virt_fb->fix.id, sizeof(virt_fb->fix.id), "virt_fb");
    virt_fb->fix.smem_start = (unsigned long)virt_fb->screen_base;
    virt_fb->fix.smem_len = fb_size;
    virt_fb->fix.line_length = width * (bpp / 8);
    virt_fb->fix.type = FB_TYPE_PACKED_PIXELS;
    virt_fb->fix.visual = FB_VISUAL_TRUECOLOR;

    virt_fb->var.xres = width;
    virt_fb->var.yres = height;
    virt_fb->var.xres_virtual = width;
    virt_fb->var.yres_virtual = height;
    virt_fb->var.bits_per_pixel = bpp;
    virt_fb->var.red.offset = 16;
    virt_fb->var.red.length = 8;
    virt_fb->var.green.offset = 8;
    virt_fb->var.green.length = 8;
    virt_fb->var.blue.offset = 0;
    virt_fb->var.blue.length = 8;
    virt_fb->var.activate = FB_ACTIVATE_NOW;

    ret = register_framebuffer(virt_fb);
    if (ret < 0) {
        vfree(virt_fb->screen_base);
        framebuffer_release(virt_fb);
        return ret;
    }

    pr_info("✅ Virtual framebuffer registered: %ux%u@%u\n", width, height, bpp);
    return 0;
}

static void __exit virt_fb_exit(void)
{
    unregister_framebuffer(virt_fb);
    vfree(virt_fb->screen_base);
    framebuffer_release(virt_fb);
    pr_info("🧹 Virtual framebuffer unregistered\n");
}

module_init(virt_fb_init);
module_exit(virt_fb_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Bro");
MODULE_DESCRIPTION("Virtual framebuffer for headless/cloud dual display");
