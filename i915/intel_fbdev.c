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
#include <linux/sysrq.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/vga_switcheroo.h>

#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include <drm/drm_fb_helper.h>
#include "intel_drv.h"
#include <drm/i915_drm.h>
#include "i915_drv.h"

static struct fb_ops intelfb_ops = {
	.owner = THIS_MODULE,
	.fb_check_var = drm_fb_helper_check_var,
	.fb_set_par = drm_fb_helper_set_par,
	.fb_fillrect = cfb_fillrect,
	.fb_copyarea = cfb_copyarea,
	.fb_imageblit = cfb_imageblit,
	.fb_pan_display = drm_fb_helper_pan_display,
	.fb_blank = drm_fb_helper_blank,
	.fb_setcmap = drm_fb_helper_setcmap,
	.fb_debug_enter = drm_fb_helper_debug_enter,
	.fb_debug_leave = drm_fb_helper_debug_leave,
};

static int intelfb_alloc(struct drm_fb_helper *helper,
			 struct drm_fb_helper_surface_size *sizes)
{
	struct intel_fbdev *ifbdev =
		container_of(helper, struct intel_fbdev, helper);
	struct drm_framebuffer *fb;
	struct drm_device *dev = helper->dev;
	struct drm_mode_fb_cmd2 mode_cmd = {};
	struct drm_i915_gem_object *obj;
	int size, ret;

	/* we don't do packed 24bpp */
	if (sizes->surface_bpp == 24)
		sizes->surface_bpp = 32;

	mode_cmd.width = sizes->surface_width;
	mode_cmd.height = sizes->surface_height;

	mode_cmd.pitches[0] = ALIGN(mode_cmd.width *
				    DIV_ROUND_UP(sizes->surface_bpp, 8), 64);
	mode_cmd.pixel_format = drm_mode_legacy_fb_format(sizes->surface_bpp,
							  sizes->surface_depth);

	size = mode_cmd.pitches[0] * mode_cmd.height;
	size = ALIGN(size, PAGE_SIZE);
	obj = i915_gem_object_create_stolen(dev, size);
	if (obj == NULL)
		obj = i915_gem_alloc_object(dev, size);
	if (!obj) {
		DRM_ERROR("failed to allocate framebuffer\n");
		ret = -ENOMEM;
		goto out;
	}

	/* Flush everything out, we'll be doing GTT only from now on */
	ret = intel_pin_and_fence_fb_obj(dev, obj, NULL);
	if (ret) {
		DRM_ERROR("failed to pin obj: %d\n", ret);
		goto out_unref;
	}

	fb = __intel_framebuffer_create(dev, &mode_cmd, obj);
	if (IS_ERR(fb)) {
		ret = PTR_ERR(fb);
		goto out_unpin;
	}

	ifbdev->fb = to_intel_framebuffer(fb);

	return 0;

out_unpin:
	i915_gem_object_ggtt_unpin(obj);
out_unref:
	drm_gem_object_unreference(&obj->base);
out:
	return ret;
}

static int intelfb_create(struct drm_fb_helper *helper,
			  struct drm_fb_helper_surface_size *sizes)
{
	struct intel_fbdev *ifbdev =
		container_of(helper, struct intel_fbdev, helper);
	struct intel_framebuffer *intel_fb = ifbdev->fb;
	struct drm_device *dev = helper->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct fb_info *info;
	struct drm_framebuffer *fb;
	struct drm_i915_gem_object *obj;
	int size, ret;
	bool prealloc = false;

	mutex_lock(&dev->struct_mutex);

	if (!intel_fb || WARN_ON(!intel_fb->obj)) {
		DRM_DEBUG_KMS("no BIOS fb, allocating a new one\n");
		ret = intelfb_alloc(helper, sizes);
		if (ret)
			goto out_unlock;
		intel_fb = ifbdev->fb;
	} else {
		DRM_DEBUG_KMS("re-using BIOS fb\n");
		prealloc = true;
		sizes->fb_width = intel_fb->base.width;
		sizes->fb_height = intel_fb->base.height;
	}

	obj = intel_fb->obj;
	size = obj->base.size;

	info = framebuffer_alloc(0, &dev->pdev->dev);
	if (!info) {
		ret = -ENOMEM;
		goto out_unpin;
	}

	info->par = helper;

	fb = &ifbdev->fb->base;

	ifbdev->helper.fb = fb;
	ifbdev->helper.fbdev = info;

	strcpy(info->fix.id, "inteldrmfb");

	info->flags = FBINFO_DEFAULT | FBINFO_CAN_FORCE_OUTPUT;
	info->fbops = &intelfb_ops;

	ret = fb_alloc_cmap(&info->cmap, 256, 0);
	if (ret) {
		ret = -ENOMEM;
		goto out_unpin;
	}
	/* setup aperture base/size for vesafb takeover */
	info->apertures = alloc_apertures(1);
	if (!info->apertures) {
		ret = -ENOMEM;
		goto out_unpin;
	}
	info->apertures->ranges[0].base = dev->mode_config.fb_base;
	info->apertures->ranges[0].size = dev_priv->gtt.mappable_end;

	info->fix.smem_start = dev->mode_config.fb_base + i915_gem_obj_ggtt_offset(obj);
	info->fix.smem_len = size;

	info->screen_base =
		ioremap_wc(dev_priv->gtt.mappable_base + i915_gem_obj_ggtt_offset(obj),
			   size);
	if (!info->screen_base) {
		ret = -ENOSPC;
		goto out_unpin;
	}
	info->screen_size = size;

	/* This driver doesn't need a VT switch to restore the mode on resume */
	info->skip_vt_switch = true;

	drm_fb_helper_fill_fix(info, fb->pitches[0], fb->depth);
	drm_fb_helper_fill_var(info, &ifbdev->helper, sizes->fb_width, sizes->fb_height);

	/* If the object is shmemfs backed, it will have given us zeroed pages.
	 * If the object is stolen however, it will be full of whatever
	 * garbage was left in there.
	 */
	if (ifbdev->fb->obj->stolen && !prealloc)
		memset_io(info->screen_base, 0, info->screen_size);

	/* Use default scratch pixmap (info->pixmap.flags = FB_PIXMAP_SYSTEM) */

	DRM_DEBUG_KMS("allocated %dx%d fb: 0x%08lx, bo %p\n",
		      fb->width, fb->height,
		      i915_gem_obj_ggtt_offset(obj), obj);

	mutex_unlock(&dev->struct_mutex);
	vga_switcheroo_client_fb_set(dev->pdev, info);
	return 0;

out_unpin:
	i915_gem_object_ggtt_unpin(obj);
	drm_gem_object_unreference(&obj->base);
out_unlock:
	mutex_unlock(&dev->struct_mutex);
	return ret;
}

/** Sets the color ramps on behalf of RandR */
static void intel_crtc_fb_gamma_set(struct drm_crtc *crtc, u16 red, u16 green,
				    u16 blue, int regno)
{
	struct intel_crtc *intel_crtc = to_intel_crtc(crtc);

	intel_crtc->lut_r[regno] = red >> 8;
	intel_crtc->lut_g[regno] = green >> 8;
	intel_crtc->lut_b[regno] = blue >> 8;
}

static void intel_crtc_fb_gamma_get(struct drm_crtc *crtc, u16 *red, u16 *green,
				    u16 *blue, int regno)
{
	struct intel_crtc *intel_crtc = to_intel_crtc(crtc);

	*red = intel_crtc->lut_r[regno] << 8;
	*green = intel_crtc->lut_g[regno] << 8;
	*blue = intel_crtc->lut_b[regno] << 8;
}

static struct drm_fb_helper_crtc *
intel_fb_helper_crtc(struct drm_fb_helper *fb_helper, struct drm_crtc *crtc)
{
	int i;

	for (i = 0; i < fb_helper->crtc_count; i++)
		if (fb_helper->crtc_info[i].mode_set.crtc == crtc)
			return &fb_helper->crtc_info[i];

	return NULL;
}

/*
 * Try to read the BIOS display configuration and use it for the initial
 * fb configuration.
 *
 * The BIOS or boot loader will generally create an initial display
 * configuration for us that includes some set of active pipes and displays.
 * This routine tries to figure out which pipes and connectors are active
 * and stuffs them into the crtcs and modes array given to us by the
 * drm_fb_helper code.
 *
 * The overall sequence is:
 *   intel_fbdev_init - from driver load
 *     intel_fbdev_init_bios - initialize the intel_fbdev using BIOS data
 *     drm_fb_helper_init - build fb helper structs
 *     drm_fb_helper_single_add_all_connectors - more fb helper structs
 *   intel_fbdev_initial_config - apply the config
 *     drm_fb_helper_initial_config - call ->probe then register_framebuffer()
 *         drm_setup_crtcs - build crtc config for fbdev
 *           intel_fb_initial_config - find active connectors etc
 *         drm_fb_helper_single_fb_probe - set up fbdev
 *           intelfb_create - re-use or alloc fb, build out fbdev structs
 *
 * Note that we don't make special consideration whether we could actually
 * switch to the selected modes without a full modeset. E.g. when the display
 * is in VGA mode we need to recalculate watermarks and set a new high-res
 * framebuffer anyway.
 */
static bool intel_fb_initial_config(struct drm_fb_helper *fb_helper,
				    struct drm_fb_helper_crtc **crtcs,
				    struct drm_display_mode **modes,
				    bool *enabled, int width, int height)
{
	struct drm_device *dev = fb_helper->dev;
	int i, j;
	bool *save_enabled;
	bool fallback = true;
	int num_connectors_enabled = 0;
	int num_connectors_detected = 0;

	/*
	 * If the user specified any force options, just bail here
	 * and use that config.
	 */
	for (i = 0; i < fb_helper->connector_count; i++) {
		struct drm_fb_helper_connector *fb_conn;
		struct drm_connector *connector;

		fb_conn = fb_helper->connector_info[i];
		connector = fb_conn->connector;

		if (!enabled[i])
			continue;

		if (connector->force != DRM_FORCE_UNSPECIFIED)
			return false;
	}

	save_enabled = kcalloc(dev->mode_config.num_connector, sizeof(bool),
			       GFP_KERNEL);
	if (!save_enabled)
		return false;

	memcpy(save_enabled, enabled, dev->mode_config.num_connector);

	for (i = 0; i < fb_helper->connector_count; i++) {
		struct drm_fb_helper_connector *fb_conn;
		struct drm_connector *connector;
		struct drm_encoder *encoder;
		struct drm_fb_helper_crtc *new_crtc;

		fb_conn = fb_helper->connector_info[i];
		connector = fb_conn->connector;

		if (connector->status == connector_status_connected)
			num_connectors_detected++;

		if (!enabled[i]) {
			DRM_DEBUG_KMS("connector %d not enabled, skipping\n",
				      connector->base.id);
			continue;
		}

		encoder = connector->encoder;
		if (!encoder || WARN_ON(!encoder->crtc)) {
			DRM_DEBUG_KMS("connector %d has no encoder or crtc, skipping\n",
				      connector->base.id);
			enabled[i] = false;
			continue;
		}

		num_connectors_enabled++;

		new_crtc = intel_fb_helper_crtc(fb_helper, encoder->crtc);

		/*
		 * Make sure we're not trying to drive multiple connectors
		 * with a single CRTC, since our cloning support may not
		 * match the BIOS.
		 */
		for (j = 0; j < fb_helper->connector_count; j++) {
			if (crtcs[j] == new_crtc) {
				DRM_DEBUG_KMS("fallback: cloned configuration\n");
				fallback = true;
				goto out;
			}
		}

		DRM_DEBUG_KMS("looking for cmdline mode on connector %d\n",
			      fb_conn->connector->base.id);

		/* go for command line mode first */
		modes[i] = drm_pick_cmdline_mode(fb_conn, width, height);

		/* try for preferred next */
		if (!modes[i]) {
			DRM_DEBUG_KMS("looking for preferred mode on connector %d\n",
				      fb_conn->connector->base.id);
			modes[i] = drm_has_preferred_mode(fb_conn, width,
							  height);
		}

		/* last resort: use current mode */
		if (!modes[i]) {
			/*
			 * IMPORTANT: We want to use the adjusted mode (i.e.
			 * after the panel fitter upscaling) as the initial
			 * config, not the input mode, which is what crtc->mode
			 * usually contains. But since our current fastboot
			 * code puts a mode derived from the post-pfit timings
			 * into crtc->mode this works out correctly. We don't
			 * use hwmode anywhere right now, so use it for this
			 * since the fb helper layer wants a pointer to
			 * something we own.
			 */
			intel_mode_from_pipe_config(&encoder->crtc->hwmode,
						    &to_intel_crtc(encoder->crtc)->config);
			modes[i] = &encoder->crtc->hwmode;
		}
		crtcs[i] = new_crtc;

		DRM_DEBUG_KMS("connector %s on crtc %d: %s\n",
			      drm_get_connector_name(connector),
			      encoder->crtc->base.id,
			      modes[i]->name);

		fallback = false;
	}

	/*
	 * If the BIOS didn't enable everything it could, fall back to have the
	 * same user experiencing of lighting up as much as possible like the
	 * fbdev helper library.
	 */
	if (num_connectors_enabled != num_connectors_detected &&
	    num_connectors_enabled < INTEL_INFO(dev)->num_pipes) {
		DRM_DEBUG_KMS("fallback: Not all outputs enabled\n");
		DRM_DEBUG_KMS("Enabled: %i, detected: %i\n", num_connectors_enabled,
			      num_connectors_detected);
		fallback = true;
	}

out:
	if (fallback) {
		DRM_DEBUG_KMS("Not using firmware configuration\n");
		memcpy(enabled, save_enabled, dev->mode_config.num_connector);
		kfree(save_enabled);
		return false;
	}

	kfree(save_enabled);
	return true;
}

static struct drm_fb_helper_funcs intel_fb_helper_funcs = {
	.initial_config = intel_fb_initial_config,
	.gamma_set = intel_crtc_fb_gamma_set,
	.gamma_get = intel_crtc_fb_gamma_get,
	.fb_probe = intelfb_create,
};

static void intel_fbdev_destroy(struct drm_device *dev,
				struct intel_fbdev *ifbdev)
{
	if (ifbdev->helper.fbdev) {
		struct fb_info *info = ifbdev->helper.fbdev;

		unregister_framebuffer(info);
		iounmap(info->screen_base);
		if (info->cmap.len)
			fb_dealloc_cmap(&info->cmap);

		framebuffer_release(info);
	}

	drm_fb_helper_fini(&ifbdev->helper);

	drm_framebuffer_unregister_private(&ifbdev->fb->base);
	drm_framebuffer_remove(&ifbdev->fb->base);
}

/*
 * Build an intel_fbdev struct using a BIOS allocated framebuffer, if possible.
 * The core display code will have read out the current plane configuration,
 * so we use that to figure out if there's an object for us to use as the
 * fb, and if so, we re-use it for the fbdev configuration.
 *
 * Note we only support a single fb shared across pipes for boot (mostly for
 * fbcon), so we just find the biggest and use that.
 */
static bool intel_fbdev_init_bios(struct drm_device *dev,
				 struct intel_fbdev *ifbdev)
{
	struct intel_framebuffer *fb = NULL;
	struct drm_crtc *crtc;
	struct intel_crtc *intel_crtc;
	struct intel_plane_config *plane_config = NULL;
	unsigned int max_size = 0;

	if (!i915.fastboot)
		return false;

	/* Find the largest fb */
	list_for_each_entry(crtc, &dev->mode_config.crtc_list, head) {
		intel_crtc = to_intel_crtc(crtc);

		if (!intel_crtc->active || !crtc->primary->fb) {
			DRM_DEBUG_KMS("pipe %c not active or no fb, skipping\n",
				      pipe_name(intel_crtc->pipe));
			continue;
		}

		if (intel_crtc->plane_config.size > max_size) {
			DRM_DEBUG_KMS("found possible fb from plane %c\n",
				      pipe_name(intel_crtc->pipe));
			plane_config = &intel_crtc->plane_config;
			fb = to_intel_framebuffer(crtc->primary->fb);
			max_size = plane_config->size;
		}
	}

	if (!fb) {
		DRM_DEBUG_KMS("no active fbs found, not using BIOS config\n");
		goto out;
	}

	/* Now make sure all the pipes will fit into it */
	list_for_each_entry(crtc, &dev->mode_config.crtc_list, head) {
		unsigned int cur_size;

		intel_crtc = to_intel_crtc(crtc);

		if (!intel_crtc->active) {
			DRM_DEBUG_KMS("pipe %c not active, skipping\n",
				      pipe_name(intel_crtc->pipe));
			continue;
		}

		DRM_DEBUG_KMS("checking plane %c for BIOS fb\n",
			      pipe_name(intel_crtc->pipe));

		/*
		 * See if the plane fb we found above will fit on this
		 * pipe.  Note we need to use the selected fb's bpp rather
		 * than the current pipe's, since they could be different.
		 */
		cur_size = intel_crtc->config.adjusted_mode.crtc_hdisplay *
			intel_crtc->config.adjusted_mode.crtc_vdisplay;
		DRM_DEBUG_KMS("pipe %c area: %d\n", pipe_name(intel_crtc->pipe),
			      cur_size);
		cur_size *= fb->base.bits_per_pixel / 8;
		DRM_DEBUG_KMS("total size %d (bpp %d)\n", cur_size,
			      fb->base.bits_per_pixel / 8);

		if (cur_size > max_size) {
			DRM_DEBUG_KMS("fb not big enough for plane %c (%d vs %d)\n",
				      pipe_name(intel_crtc->pipe),
				      cur_size, max_size);
			plane_config = NULL;
			fb = NULL;
			break;
		}

		DRM_DEBUG_KMS("fb big enough for plane %c (%d >= %d)\n",
			      pipe_name(intel_crtc->pipe),
			      max_size, cur_size);
	}

	if (!fb) {
		DRM_DEBUG_KMS("BIOS fb not suitable for all pipes, not using\n");
		goto out;
	}

	ifbdev->preferred_bpp = fb->base.bits_per_pixel;
	ifbdev->fb = fb;

	drm_framebuffer_reference(&ifbdev->fb->base);

	/* Final pass to check if any active pipes don't have fbs */
	list_for_each_entry(crtc, &dev->mode_config.crtc_list, head) {
		intel_crtc = to_intel_crtc(crtc);

		if (!intel_crtc->active)
			continue;

		WARN(!crtc->primary->fb,
		     "re-used BIOS config but lost an fb on crtc %d\n",
		     crtc->base.id);
	}


	DRM_DEBUG_KMS("using BIOS fb for initial console\n");
	return true;

out:

	return false;
}

int intel_fbdev_init(struct drm_device *dev)
{
	struct intel_fbdev *ifbdev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	int ret;

	if (WARN_ON(INTEL_INFO(dev)->num_pipes == 0))
		return -ENODEV;

	ifbdev = kzalloc(sizeof(struct intel_fbdev), GFP_KERNEL);
	if (ifbdev == NULL)
		return -ENOMEM;

	ifbdev->helper.funcs = &intel_fb_helper_funcs;
	if (!intel_fbdev_init_bios(dev, ifbdev))
		ifbdev->preferred_bpp = 32;

	ret = drm_fb_helper_init(dev, &ifbdev->helper,
				 INTEL_INFO(dev)->num_pipes, 4);
	if (ret) {
		kfree(ifbdev);
		return ret;
	}

	dev_priv->fbdev = ifbdev;
	drm_fb_helper_single_add_all_connectors(&ifbdev->helper);

	return 0;
}

void intel_fbdev_initial_config(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_fbdev *ifbdev = dev_priv->fbdev;

	/* Due to peculiar init order wrt to hpd handling this is separate. */
	drm_fb_helper_initial_config(&ifbdev->helper, ifbdev->preferred_bpp);
}

void intel_fbdev_fini(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	if (!dev_priv->fbdev)
		return;

	intel_fbdev_destroy(dev, dev_priv->fbdev);
	kfree(dev_priv->fbdev);
	dev_priv->fbdev = NULL;
}

void intel_fbdev_set_suspend(struct drm_device *dev, int state)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_fbdev *ifbdev = dev_priv->fbdev;
	struct fb_info *info;

	if (!ifbdev)
		return;

	info = ifbdev->helper.fbdev;

	/* On resume from hibernation: If the object is shmemfs backed, it has
	 * been restored from swap. If the object is stolen however, it will be
	 * full of whatever garbage was left in there.
	 */
	if (state == FBINFO_STATE_RUNNING && ifbdev->fb->obj->stolen)
		memset_io(info->screen_base, 0, info->screen_size);

	fb_set_suspend(info, state);
}

void intel_fbdev_output_poll_changed(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	if (dev_priv->fbdev)
		drm_fb_helper_hotplug_event(&dev_priv->fbdev->helper);
}

void intel_fbdev_restore_mode(struct drm_device *dev)
{
	int ret;
	struct drm_i915_private *dev_priv = dev->dev_private;

	if (!dev_priv->fbdev)
		return;

	drm_modeset_lock_all(dev);

	ret = drm_fb_helper_restore_fbdev_mode(&dev_priv->fbdev->helper);
	if (ret)
		DRM_DEBUG("failed to restore crtc mode\n");

	drm_modeset_unlock_all(dev);
}
