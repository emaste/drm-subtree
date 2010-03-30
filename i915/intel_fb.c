/*
 * Copyright © 2007 David Airlie
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *     David Airlie
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/slab.h>
#include <linux/sysrq.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/vga_switcheroo.h>

#include "drmP.h"
#include "drm.h"
#include "drm_crtc.h"
#include "drm_fb_helper.h"
#include "intel_drv.h"
#include "i915_drm.h"
#include "i915_drv.h"

struct intel_kernel_fbdev {
	struct drm_fb_helper helper;
	struct intel_framebuffer ifb;
	struct list_head fbdev_list;
	struct drm_display_mode *our_mode;
};

static struct fb_ops intelfb_ops = {
	.owner = THIS_MODULE,
	.fb_check_var = drm_fb_helper_check_var,
	.fb_set_par = drm_fb_helper_set_par,
	.fb_setcolreg = drm_fb_helper_setcolreg,
	.fb_fillrect = cfb_fillrect,
	.fb_copyarea = cfb_copyarea,
	.fb_imageblit = cfb_imageblit,
	.fb_pan_display = drm_fb_helper_pan_display,
	.fb_blank = drm_fb_helper_blank,
	.fb_setcmap = drm_fb_helper_setcmap,
};

static struct drm_fb_helper_funcs intel_fb_helper_funcs = {
	.gamma_set = intel_crtc_fb_gamma_set,
	.gamma_get = intel_crtc_fb_gamma_get,
};


static int intelfb_create(struct drm_device *dev,
			  struct drm_fb_helper_surface_size *sizes,
			  struct intel_kernel_fbdev **ifbdev_p)
{
	struct fb_info *info;
	struct intel_kernel_fbdev *ifbdev;
	struct drm_framebuffer *fb;
	struct intel_framebuffer *intel_fb;
	struct drm_mode_fb_cmd mode_cmd;
	struct drm_gem_object *fbo = NULL;
	struct drm_i915_gem_object *obj_priv;
	struct device *device = &dev->pdev->dev;
	int size, ret, mmio_bar = IS_I9XX(dev) ? 0 : 1;

	/* we don't do packed 24bpp */
	if (sizes->surface_bpp == 24)
		sizes->surface_bpp = 32;

	mode_cmd.width = sizes->surface_width;
	mode_cmd.height = sizes->surface_height;

	mode_cmd.bpp = sizes->surface_bpp;
	mode_cmd.pitch = ALIGN(mode_cmd.width * ((mode_cmd.bpp + 1) / 8), 64);
	mode_cmd.depth = sizes->surface_depth;

	size = mode_cmd.pitch * mode_cmd.height;
	size = ALIGN(size, PAGE_SIZE);
	fbo = drm_gem_object_alloc(dev, size);
	if (!fbo) {
		DRM_ERROR("failed to allocate framebuffer\n");
		ret = -ENOMEM;
		goto out;
	}
	obj_priv = fbo->driver_private;

	mutex_lock(&dev->struct_mutex);

	ret = i915_gem_object_pin(fbo, 64*1024);
	if (ret) {
		DRM_ERROR("failed to pin fb: %d\n", ret);
		goto out_unref;
	}

	/* Flush everything out, we'll be doing GTT only from now on */
	i915_gem_object_set_to_gtt_domain(fbo, 1);

	info = framebuffer_alloc(sizeof(struct intel_kernel_fbdev), device);
	if (!info) {
		ret = -ENOMEM;
		goto out_unpin;
	}

	ifbdev = info->par;
	intel_framebuffer_init(dev, &ifbdev->ifb, &mode_cmd, fbo);

	fb = &ifbdev->ifb.base;

	ifbdev->helper.fb = fb;
	ifbdev->helper.fbdev = info;
	ifbdev->helper.funcs = &intel_fb_helper_funcs;
	ifbdev->helper.dev = dev;

	*ifbdev_p = ifbdev;

	ret = drm_fb_helper_init_crtc_count(&ifbdev->helper, 2,
					    INTELFB_CONN_LIMIT);
	if (ret)
		goto out_unref;

	strcpy(info->fix.id, "inteldrmfb");

	info->flags = FBINFO_DEFAULT;

	info->fbops = &intelfb_ops;


	/* setup aperture base/size for vesafb takeover */
	info->aperture_base = dev->mode_config.fb_base;
	if (IS_I9XX(dev))
		info->aperture_size = pci_resource_len(dev->pdev, 2);
	else
		info->aperture_size = pci_resource_len(dev->pdev, 0);

	info->fix.smem_start = dev->mode_config.fb_base + obj_priv->gtt_offset;
	info->fix.smem_len = size;

	info->flags = FBINFO_DEFAULT;

	info->screen_base = ioremap_wc(dev->agp->base + obj_priv->gtt_offset,
				       size);
	if (!info->screen_base) {
		ret = -ENOSPC;
		goto out_unpin;
	}
	info->screen_size = size;

//	memset(info->screen_base, 0, size);

	drm_fb_helper_fill_fix(info, fb->pitch, fb->depth);
	drm_fb_helper_fill_var(info, &ifbdev->helper, sizes->fb_width, sizes->fb_height);

	/* FIXME: we really shouldn't expose mmio space at all */
	info->fix.mmio_start = pci_resource_start(dev->pdev, mmio_bar);
	info->fix.mmio_len = pci_resource_len(dev->pdev, mmio_bar);

	info->pixmap.size = 64*1024;
	info->pixmap.buf_align = 8;
	info->pixmap.access_align = 32;
	info->pixmap.flags = FB_PIXMAP_SYSTEM;
	info->pixmap.scan_align = 1;

	DRM_DEBUG_KMS("allocated %dx%d fb: 0x%08x, bo %p\n",
			intel_fb->base.width, intel_fb->base.height,
			obj_priv->gtt_offset, fbo);


	mutex_unlock(&dev->struct_mutex);
	vga_switcheroo_client_fb_set(dev->pdev, info);
	return 0;

out_unpin:
	i915_gem_object_unpin(fbo);
out_unref:
	drm_gem_object_unreference(fbo);
	mutex_unlock(&dev->struct_mutex);
out:
	return ret;
}

static int intel_fb_find_or_create_single(struct drm_device *dev,
					  struct drm_fb_helper_surface_size *sizes,
					  struct drm_fb_helper **fb_ptr)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct intel_kernel_fbdev *ifbdev = NULL;
	int new_fb = 0;
	int ret;

	if (!dev_priv->fbdev) {
		ret = intelfb_create(dev, sizes,
				     &ifbdev);
		if (ret)
			return ret;

		dev_priv->fbdev = ifbdev;
		new_fb = 1;
	} else {
		ifbdev = dev_priv->fbdev;
		if (ifbdev->ifb.base.width < sizes->surface_width ||
		    ifbdev->ifb.base.height < sizes->surface_height) {
			DRM_ERROR("Framebuffer not large enough to scale console onto.\n");
			return -EINVAL;
		}
	}

	*fb_ptr = &ifbdev->helper;
	return new_fb;
}

static int intelfb_probe(struct drm_device *dev)
{
	int ret;

	DRM_DEBUG_KMS("\n");
	ret = drm_fb_helper_single_fb_probe(dev, 32, intel_fb_find_or_create_single);
	return ret;
}

int intel_fbdev_destroy(struct drm_device *dev,
			struct intel_kernel_fbdev *ifbdev)
{
	struct fb_info *info;
	struct intel_framebuffer *ifb = &ifbdev->ifb;

	info = ifbdev->helper.fbdev;

	unregister_framebuffer(info);
	iounmap(info->screen_base);
	drm_fb_helper_free(&ifbdev->helper);

	drm_framebuffer_cleanup(&ifb->base);
	drm_gem_object_unreference_unlocked(ifb->obj);

	framebuffer_release(info);

	return 0;
}

int intel_fbdev_init(struct drm_device *dev)
{
	drm_helper_initial_config(dev);
	intelfb_probe(dev);
	return 0;
}

void intel_fbdev_fini(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	intel_fbdev_destroy(dev, dev_priv->fbdev);
	dev_priv->fbdev = NULL;
}
MODULE_LICENSE("GPL and additional rights");
