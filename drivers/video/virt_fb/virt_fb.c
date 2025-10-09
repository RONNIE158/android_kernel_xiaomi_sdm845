#include <linux/module.h>
#include <linux/fb.h>
#include <linux/vmalloc.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/uaccess.h>

static struct fb_info *virt_fb;
static void *virt_fb_mem;
static unsigned long virt_fb_mem_size;

#define VIRT_FB_WIDTH  1920
#define VIRT_FB_HEIGHT 1080
#define VIRT_FB_BPP    32

/* Dummy ops (karena tidak ada akselerasi hardware) */
static void virt_fb_fillrect(struct fb_info *info, const struct fb_fillrect *rect) {}
static void virt_fb_copyarea(struct fb_info *info, const struct fb_copyarea *area) {}
static void virt_fb_imageblit(struct fb_info *info, const struct fb_image *image) {}

/* mmap agar userspace bisa baca buffer */
static int virt_fb_mmap(struct fb_info *info, struct vm_area_struct *vma)
{
    unsigned long start = vma->vm_start;
    unsigned long size = vma->vm_end - vma->vm_start;
    unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;
    unsigned long pfn;
    void *page_ptr = virt_fb_mem + offset;

    if (offset + size > virt_fb_mem_size)
        return -EINVAL;

    while (size > 0) {
        pfn = vmalloc_to_pfn(page_ptr);
        if (remap_pfn_range(vma, start, pfn, PAGE_SIZE, vma->vm_page_prot))
            return -EAGAIN;

        start += PAGE_SIZE;
        page_ptr += PAGE_SIZE;
        size -= PAGE_SIZE;
    }

    pr_info("virt_fb: mmap to userspace successful (size=%lu)\n", virt_fb_mem_size);
    return 0;
}

static struct fb_ops virt_fb_ops = {
    .owner        = THIS_MODULE,
    .fb_fillrect  = virt_fb_fillrect,
    .fb_copyarea  = virt_fb_copyarea,
    .fb_imageblit = virt_fb_imageblit,
    .fb_mmap      = virt_fb_mmap,
};

static int __init virt_fb_init(void)
{
    int ret;
    unsigned int width = VIRT_FB_WIDTH, height = VIRT_FB_HEIGHT, bpp = VIRT_FB_BPP;

    virt_fb_mem_size = width * height * (bpp / 8);
    virt_fb_mem = vzalloc(virt_fb_mem_size);
    if (!virt_fb_mem)
        return -ENOMEM;

    virt_fb = framebuffer_alloc(0, NULL);
    if (!virt_fb) {
        vfree(virt_fb_mem);
        return -ENOMEM;
    }

    virt_fb->screen_base = virt_fb_mem;
    virt_fb->fbops = &virt_fb_ops;

    snprintf(virt_fb->fix.id, sizeof(virt_fb->fix.id), "virt_fb");
    virt_fb->fix.smem_start = (unsigned long)virt_fb_mem;
    virt_fb->fix.smem_len = virt_fb_mem_size;
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

    virt_fb->flags = FBINFO_FLAG_DEFAULT;
    virt_fb->fix.mmio_start = 0;
    virt_fb->fix.mmio_len = 0;
    virt_fb->pseudo_palette = NULL;

    ret = register_framebuffer(virt_fb);
    if (ret < 0) {
        framebuffer_release(virt_fb);
        vfree(virt_fb_mem);
        return ret;
    }

    pr_info("✅ virt_fb registered as /dev/fb0 (%ux%u@%u)\n", width, height, bpp);
    return 0;
}

static void __exit virt_fb_exit(void)
{
    unregister_framebuffer(virt_fb);
    vfree(virt_fb_mem);
    framebuffer_release(virt_fb);
    pr_info("🧹 virt_fb removed\n");
}

module_init(virt_fb_init);
module_exit(virt_fb_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Bro");
MODULE_DESCRIPTION("Virtual framebuffer with mmap support for headless Android / VNC");
