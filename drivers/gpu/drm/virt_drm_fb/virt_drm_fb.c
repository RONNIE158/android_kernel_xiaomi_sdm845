/*
 * drivers/gpu/drm/virt_drm_fb.c
 *
 * Virtual DRM framebuffer for headless / VNC Android
 *
 * Author: Bro
 * License: GPL
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/mutex.h>
#include <drm/drm_drv.h>
#include <drm/drm_device.h>
#include <drm/drm_crtc.h>
#include <drm/drm_plane.h>
#include <drm/drm_modes.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>

#define DRIVER_NAME "virt_drm_fb"

struct virt_drm {
    struct drm_device *dev;
    struct drm_fb_helper fb_helper;
    void *fb_mem;
    size_t fb_size;
    struct mutex lock;
    unsigned int width;
    unsigned int height;
    unsigned int bpp;
};

static int virt_drm_fb_create(struct virt_drm *vdrm)
{
    vdrm->fb_size = vdrm->width * vdrm->height * (vdrm->bpp / 8);
    vdrm->fb_mem = vzalloc(vdrm->fb_size);
    if (!vdrm->fb_mem)
        return -ENOMEM;

    mutex_init(&vdrm->lock);
    pr_info("virt_drm: fb_size=%zu, %ux%u@%u\n",
            vdrm->fb_size, vdrm->width, vdrm->height, vdrm->bpp);
    return 0;
}

/* mmap for userspace (VNC daemon) */
static int virt_drm_mmap(struct virt_drm *vdrm, struct vm_area_struct *vma)
{
    unsigned long start = vma->vm_start;
    unsigned long size = vma->vm_end - vma->vm_start;
    unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;
    unsigned long pos, pfn;

    if (offset + size > vdrm->fb_size)
        return -EINVAL;

    pos = (unsigned long)vdrm->fb_mem;

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

/* user-space update hook */
void virt_drm_update_frame(struct virt_drm *vdrm, void *buffer, size_t len)
{
    if (!vdrm || !vdrm->fb_mem || !buffer)
        return;

    if (len > vdrm->fb_size)
        len = vdrm->fb_size;

    mutex_lock(&vdrm->lock);
    memcpy(vdrm->fb_mem, buffer, len);
    mutex_unlock(&vdrm->lock);
}
EXPORT_SYMBOL(virt_drm_update_frame);

static int virt_drm_probe(struct platform_device *pdev)
{
    struct virt_drm *vdrm;

    vdrm = kzalloc(sizeof(*vdrm), GFP_KERNEL);
    if (!vdrm)
        return -ENOMEM;

    vdrm->width = 1920;
    vdrm->height = 1080;
    vdrm->bpp = 32;
    platform_set_drvdata(pdev, vdrm);

    virt_drm_fb_create(vdrm);

    pr_info("virt_drm: virtual DRM framebuffer registered\n");
    return 0;
}

static int virt_drm_remove(struct platform_device *pdev)
{
    struct virt_drm *vdrm = platform_get_drvdata(pdev);

    if (vdrm->fb_mem)
        vfree(vdrm->fb_mem);
    kfree(vdrm);

    pr_info("virt_drm: removed\n");
    return 0;
}

static struct platform_driver virt_drm_driver = {
    .probe = virt_drm_probe,
    .remove = virt_drm_remove,
    .driver = {
        .name = DRIVER_NAME,
        .owner = THIS_MODULE,
    },
};

module_platform_driver(virt_drm_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Bro");
MODULE_DESCRIPTION("Full virtual DRM framebuffer for headless Android/VNC");
