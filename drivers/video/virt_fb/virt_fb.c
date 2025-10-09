/*
 * drivers/video/virt_fb/virt_fb.c
 *
 * Virtual framebuffer for headless / VNC usage.
 *
 * - Uses vmalloc_user() so userspace can mmap() the buffer safely.
 * - Provides minimal fbdev ops and fb_mmap implementation.
 *
 * Author: Bro
 * License: GPL
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fb.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/moduleparam.h>

static struct fb_info *virt_fb;
static void *virt_fb_mem;
static unsigned long virt_fb_mem_size;

/* Default params (can be changed before build as needed) */
static unsigned int fb_width = 1920;
static unsigned int fb_height = 1080;
static unsigned int fb_bpp = 32;

module_param(fb_width, uint, 0444);
MODULE_PARM_DESC(fb_width, "Virt FB width");
module_param(fb_height, uint, 0444);
MODULE_PARM_DESC(fb_height, "Virt FB height");
module_param(fb_bpp, uint, 0444);
MODULE_PARM_DESC(fb_bpp, "Virt FB bits-per-pixel (commonly 32)");

/* Dummy ops: no hardware acceleration, so use no-op helpers */
static void virt_fb_fillrect(struct fb_info *info, const struct fb_fillrect *rect) { (void)info; (void)rect; }
static void virt_fb_copyarea(struct fb_info *info, const struct fb_copyarea *area) { (void)info; (void)area; }
static void virt_fb_imageblit(struct fb_info *info, const struct fb_image *image) { (void)info; (void)image; }

/* mmap: map vmalloc_user() memory into userspace */
static int virt_fb_mmap(struct fb_info *info, struct vm_area_struct *vma)
{
    unsigned long start;
    unsigned long size;
    unsigned long offset;
    unsigned long pos;
    unsigned long pfn;
    int ret = 0;

    start = vma->vm_start;
    size = vma->vm_end - vma->vm_start;
    offset = vma->vm_pgoff << PAGE_SHIFT;

    if (!info || !info->fix.smem_len)
        return -EINVAL;

    if (offset + size > info->fix.smem_len)
        return -EINVAL;

    pos = (unsigned long)info->screen_base + offset;

    /* remap each page from vmalloc area to user vma */
    while (size > 0) {
        pfn = vmalloc_to_pfn((void *)pos);
        if (!pfn) {
            ret = -EFAULT;
            break;
        }

        if (remap_pfn_range(vma, start, pfn, PAGE_SIZE, vma->vm_page_prot)) {
            ret = -EAGAIN;
            break;
        }

        start += PAGE_SIZE;
        pos += PAGE_SIZE;
        size -= PAGE_SIZE;
    }

    if (!ret)
        pr_debug("virt_fb: mmap to userspace successful (mem=%lu)\n", info->fix.smem_len);

    return ret;
}

static const struct fb_ops virt_fb_ops = {
    .owner        = THIS_MODULE,
    .fb_fillrect  = virt_fb_fillrect,
    .fb_copyarea  = virt_fb_copyarea,
    .fb_imageblit = virt_fb_imageblit,
    .fb_mmap      = virt_fb_mmap,
};

static int __init virt_fb_init(void)
{
    int ret;
    unsigned int width;
    unsigned int height;
    unsigned int bpp;
    unsigned long line_len;
    unsigned long mem_size;
    struct fb_fix_screeninfo fix;
    struct fb_var_screeninfo var;

    width = fb_width;
    height = fb_height;
    bpp = fb_bpp;

    /* calculate sizes */
    line_len = width * (bpp / 8);
    mem_size = line_len * height;

    /* allocate vmalloc_user area so userspace mmap works */
    virt_fb_mem = vmalloc_user(mem_size);
    if (!virt_fb_mem) {
        pr_err("virt_fb: vmalloc_user(%lu) failed\n", mem_size);
        return -ENOMEM;
    }

    /* get fb_info structure */
    virt_fb = framebuffer_alloc(0, NULL);
    if (!virt_fb) {
        pr_err("virt_fb: framebuffer_alloc failed\n");
        vfree(virt_fb_mem);
        return -ENOMEM;
    }

    /* setup fb_info */
    virt_fb->screen_base = virt_fb_mem;
    virt_fb->fbops = &virt_fb_ops;

    /* fill fix and var structures */
    memset(&fix, 0, sizeof(fix));
    snprintf(fix.id, sizeof(fix.id), "virt_fb");
    fix.smem_start = (unsigned long)virt_fb_mem; /* user-space should not rely on this */
    fix.smem_len = mem_size;
    fix.line_length = line_len;
    fix.type = FB_TYPE_PACKED_PIXELS;
    fix.visual = FB_VISUAL_TRUECOLOR;

    memcpy(&virt_fb->fix, &fix, sizeof(fix));

    memset(&var, 0, sizeof(var));
    var.xres = width;
    var.yres = height;
    var.xres_virtual = width;
    var.yres_virtual = height;
    var.bits_per_pixel = bpp;
    var.red.offset = (bpp == 32) ? 16 : 11;
    var.red.length = (bpp == 32) ? 8 : 5;
    var.green.offset = (bpp == 32) ? 8 : 5;
    var.green.length = (bpp == 32) ? 8 : 6;
    var.blue.offset = (bpp == 32) ? 0 : 0;
    var.blue.length = (bpp == 32) ? 8 : 5;
    var.activate = FB_ACTIVATE_NOW;

    memcpy(&virt_fb->var, &var, sizeof(var));

    virt_fb->fix.mmio_start = 0;
    virt_fb->fix.mmio_len = 0;
    virt_fb->flags = FBINFO_FLAG_DEFAULT;
    virt_fb->pseudo_palette = NULL;

    /* register to kernel framebuffer subsystem */
    ret = register_framebuffer(virt_fb);
    if (ret < 0) {
        pr_err("virt_fb: register_framebuffer failed: %d\n", ret);
        framebuffer_release(virt_fb);
        vfree(virt_fb_mem);
        return ret;
    }

    virt_fb_mem_size = mem_size;

    pr_info("virt_fb: registered /dev/fb0 (%ux%u@%u) mem=%lu bytes\n",
            width, height, bpp, virt_fb_mem_size);

    return 0;
}

static void __exit virt_fb_exit(void)
{
    if (virt_fb) {
        unregister_framebuffer(virt_fb);
        framebuffer_release(virt_fb);
        virt_fb = NULL;
    }

    if (virt_fb_mem) {
        vfree(virt_fb_mem);
        virt_fb_mem = NULL;
    }

    pr_info("virt_fb: removed\n");
}

module_init(virt_fb_init);
module_exit(virt_fb_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Bro");
MODULE_DESCRIPTION("Virtual framebuffer with mmap support for headless Android / VNC");
