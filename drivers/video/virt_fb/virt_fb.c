/*
 * drivers/video/virt_fb/virt_fb.c
 *
 * Secondary virtual framebuffer updated directly from HWC/KGSL for headless VNC.
 *
 * Author: Bro
 * License: GPL
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fb.h>
#include <linux/vmalloc.h>
#include <linux/mutex.h>
#include <linux/string.h>
#include <linux/notifier.h>
#include <linux/platform_device.h>

static struct fb_info *virt_fb;
static void *virt_fb_mem;
static unsigned long virt_fb_mem_size;
static DEFINE_MUTEX(virt_fb_lock);

/* Default params */
static unsigned int fb_width = 1920;
static unsigned int fb_height = 1080;
static unsigned int fb_bpp = 32;

module_param(fb_width, uint, 0444);
MODULE_PARM_DESC(fb_width, "Virt FB width");
module_param(fb_height, uint, 0444);
MODULE_PARM_DESC(fb_height, "Virt FB height");
module_param(fb_bpp, uint, 0444);
MODULE_PARM_DESC(fb_bpp, "Virt FB bits-per-pixel");

/* --- FB OPS --- */
static void virt_fb_fillrect(struct fb_info *info, const struct fb_fillrect *rect) {}
static void virt_fb_copyarea(struct fb_info *info, const struct fb_copyarea *area) {}
static void virt_fb_imageblit(struct fb_info *info, const struct fb_image *image) {}

static int virt_fb_mmap(struct fb_info *info, struct vm_area_struct *vma)
{
    unsigned long start = vma->vm_start;
    unsigned long size = vma->vm_end - vma->vm_start;
    unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;
    unsigned long pos, pfn;

    if (!info || offset + size > info->fix.smem_len)
        return -EINVAL;

    pos = (unsigned long)info->screen_base + offset;

    while (size > 0) {
        pfn = vmalloc_to_pfn((void *)pos);
        if (!pfn)
            return -EFAULT;
        if (remap_pfn_range(vma, start, pfn, PAGE_SIZE, vma->vm_page_prot))
            return -EAGAIN;

        start += PAGE_SIZE;
        pos += PAGE_SIZE;
        size -= PAGE_SIZE;
    }

    return 0;
}

static struct fb_ops virt_fb_ops = {
    .owner        = THIS_MODULE,
    .fb_fillrect  = virt_fb_fillrect,
    .fb_copyarea  = virt_fb_copyarea,
    .fb_imageblit = virt_fb_imageblit,
    .fb_mmap      = virt_fb_mmap,
};

/* --- Update virt_fb from HWC/KGSL buffer --- */
static void virt_fb_update_from_hwc(void *hwc_buffer, size_t len)
{
    if (!virt_fb_mem || !hwc_buffer)
        return;

    if (len > virt_fb_mem_size)
        len = virt_fb_mem_size;

    mutex_lock(&virt_fb_lock);
    memcpy(virt_fb_mem, hwc_buffer, len);
    mutex_unlock(&virt_fb_lock);
}

/* --- FB NOTIFIER HOOK --- */
static int virt_fb_fb_notifier(struct notifier_block *nb, unsigned long event, void *data)
{
    struct fb_event *ev = data;

    if (event == FB_EVENT_UPDATE && ev->info && ev->info->screen_base)
        virt_fb_update_from_hwc(ev->info->screen_base, ev->info->fix.smem_len);

    return 0;
}

static struct notifier_block virt_fb_nb = {
    .notifier_call = virt_fb_fb_notifier,
};

/* --- INIT/EXIT --- */
static int __init virt_fb_init(void)
{
    int ret;
    unsigned long line_len = fb_width * (fb_bpp/8);
    unsigned long mem_size = line_len * fb_height;
    struct fb_fix_screeninfo fix;
    struct fb_var_screeninfo var;

    /* allocate framebuffer memory */
    virt_fb_mem = vmalloc_user(mem_size);
    if (!virt_fb_mem)
        return -ENOMEM;

    /* allocate fb_info */
    virt_fb = framebuffer_alloc(0, NULL);
    if (!virt_fb) {
        vfree(virt_fb_mem);
        return -ENOMEM;
    }

    virt_fb->screen_base = virt_fb_mem;
    virt_fb->fbops = &virt_fb_ops;

    /* setup fix info */
    memset(&fix, 0, sizeof(fix));
    snprintf(fix.id, sizeof(fix.id), "virt_fb1");
    fix.smem_len = mem_size;
    fix.line_length = line_len;
    fix.type = FB_TYPE_PACKED_PIXELS;
    fix.visual = FB_VISUAL_TRUECOLOR;
    memcpy(&virt_fb->fix, &fix, sizeof(fix));

    /* setup var info */
    memset(&var, 0, sizeof(var));
    var.xres = fb_width;
    var.yres = fb_height;
    var.xres_virtual = fb_width;
    var.yres_virtual = fb_height;
    var.bits_per_pixel = fb_bpp;
    var.red.offset = 16; var.red.length = 8;
    var.green.offset = 8; var.green.length = 8;
    var.blue.offset = 0; var.blue.length = 8;
    var.activate = FB_ACTIVATE_NOW;
    memcpy(&virt_fb->var, &var, sizeof(var));

    virt_fb->flags = FBINFO_FLAG_DEFAULT;
    virt_fb->pseudo_palette = NULL;

    /* register framebuffer */
    ret = register_framebuffer(virt_fb);
    if (ret < 0) {
        framebuffer_release(virt_fb);
        vfree(virt_fb_mem);
        return ret;
    }

    virt_fb_mem_size = mem_size;

    /* register notifier for automatic HWC update */
    fb_register_client(&virt_fb_nb);

    pr_info("virt_fb: registered /dev/fb1 and hooked to HWC/KGSL\n");

    return 0;
}

static void __exit virt_fb_exit(void)
{
    fb_unregister_client(&virt_fb_nb);

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
MODULE_DESCRIPTION("Secondary virtual framebuffer auto-updated from HWC/KGSL for VNC");
