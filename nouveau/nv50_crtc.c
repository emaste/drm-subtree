/*
 * Copyright (C) 2008 Maarten Maathuis.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE COPYRIGHT OWNER(S) AND/OR ITS SUPPLIERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include "drmP.h"
#include "drm_mode.h"
#include "drm_crtc_helper.h"

#define NOUVEAU_DMA_DEBUG (nouveau_reg_debug & NOUVEAU_REG_DEBUG_EVO)
#include "nouveau_reg.h"
#include "nouveau_drv.h"
#include "nouveau_hw.h"
#include "nouveau_encoder.h"
#include "nouveau_crtc.h"
#include "nouveau_fb.h"
#include "nouveau_connector.h"
#include "nv50_display.h"

static void
nv50_crtc_lut_load(struct drm_crtc *crtc)
{
	struct nouveau_crtc *nv_crtc = nouveau_crtc(crtc);
	void __iomem *lut = nvbo_kmap_obj_iovirtual(nv_crtc->lut.nvbo);
	int i;

	NV_DEBUG_KMS(crtc->dev, "\n");

	for (i = 0; i < 256; i++) {
		writew(nv_crtc->lut.r[i] >> 2, lut + 8*i + 0);
		writew(nv_crtc->lut.g[i] >> 2, lut + 8*i + 2);
		writew(nv_crtc->lut.b[i] >> 2, lut + 8*i + 4);
	}

	if (nv_crtc->lut.depth == 30) {
		writew(nv_crtc->lut.r[i - 1] >> 2, lut + 8*i + 0);
		writew(nv_crtc->lut.g[i - 1] >> 2, lut + 8*i + 2);
		writew(nv_crtc->lut.b[i - 1] >> 2, lut + 8*i + 4);
	}
}

int
nv50_crtc_blank(struct nouveau_crtc *nv_crtc, bool blanked)
{
	struct drm_device *dev = nv_crtc->base.dev;
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct nouveau_channel *evo = nv50_display(dev)->master;
	int index = nv_crtc->index, ret;

	NV_DEBUG_KMS(dev, "index %d\n", nv_crtc->index);
	NV_DEBUG_KMS(dev, "%s\n", blanked ? "blanked" : "unblanked");

	if (blanked) {
		nv_crtc->cursor.hide(nv_crtc, false);

		ret = RING_SPACE(evo, dev_priv->chipset != 0x50 ? 7 : 5);
		if (ret) {
			NV_ERROR(dev, "no space while blanking crtc\n");
			return ret;
		}
		BEGIN_RING(evo, 0, NV50_EVO_CRTC(index, CLUT_MODE), 2);
		OUT_RING(evo, NV50_EVO_CRTC_CLUT_MODE_BLANK);
		OUT_RING(evo, 0);
		if (dev_priv->chipset != 0x50) {
			BEGIN_RING(evo, 0, NV84_EVO_CRTC(index, CLUT_DMA), 1);
			OUT_RING(evo, NV84_EVO_CRTC_CLUT_DMA_HANDLE_NONE);
		}

		BEGIN_RING(evo, 0, NV50_EVO_CRTC(index, FB_DMA), 1);
		OUT_RING(evo, NV50_EVO_CRTC_FB_DMA_HANDLE_NONE);
	} else {
		if (nv_crtc->cursor.visible)
			nv_crtc->cursor.show(nv_crtc, false);
		else
			nv_crtc->cursor.hide(nv_crtc, false);

		ret = RING_SPACE(evo, dev_priv->chipset != 0x50 ? 10 : 8);
		if (ret) {
			NV_ERROR(dev, "no space while unblanking crtc\n");
			return ret;
		}
		BEGIN_RING(evo, 0, NV50_EVO_CRTC(index, CLUT_MODE), 2);
		OUT_RING(evo, nv_crtc->lut.depth == 8 ?
				NV50_EVO_CRTC_CLUT_MODE_OFF :
				NV50_EVO_CRTC_CLUT_MODE_ON);
		OUT_RING(evo, nv_crtc->lut.nvbo->bo.offset >> 8);
		if (dev_priv->chipset != 0x50) {
			BEGIN_RING(evo, 0, NV84_EVO_CRTC(index, CLUT_DMA), 1);
			OUT_RING(evo, NvEvoVRAM);
		}

		BEGIN_RING(evo, 0, NV50_EVO_CRTC(index, FB_OFFSET), 2);
		OUT_RING(evo, nv_crtc->fb.offset >> 8);
		OUT_RING(evo, 0);
		BEGIN_RING(evo, 0, NV50_EVO_CRTC(index, FB_DMA), 1);
		if (dev_priv->chipset != 0x50)
			if (nv_crtc->fb.tile_flags == 0x7a00 ||
			    nv_crtc->fb.tile_flags == 0xfe00)
				OUT_RING(evo, NvEvoFB32);
			else
			if (nv_crtc->fb.tile_flags == 0x7000)
				OUT_RING(evo, NvEvoFB16);
			else
				OUT_RING(evo, NvEvoVRAM_LP);
		else
			OUT_RING(evo, NvEvoVRAM_LP);
	}

	nv_crtc->fb.blanked = blanked;
	return 0;
}

static int
nv50_crtc_set_dither(struct nouveau_crtc *nv_crtc, bool on, bool update)
{
	struct drm_device *dev = nv_crtc->base.dev;
	struct nouveau_channel *evo = nv50_display(dev)->master;
	int ret;

	NV_DEBUG_KMS(dev, "\n");

	ret = RING_SPACE(evo, 2 + (update ? 2 : 0));
	if (ret) {
		NV_ERROR(dev, "no space while setting dither\n");
		return ret;
	}

	BEGIN_RING(evo, 0, NV50_EVO_CRTC(nv_crtc->index, DITHER_CTRL), 1);
	if (on)
		OUT_RING(evo, NV50_EVO_CRTC_DITHER_CTRL_ON);
	else
		OUT_RING(evo, NV50_EVO_CRTC_DITHER_CTRL_OFF);

	if (update) {
		BEGIN_RING(evo, 0, NV50_EVO_UPDATE, 1);
		OUT_RING(evo, 0);
		FIRE_RING(evo);
	}

	return 0;
}

struct nouveau_connector *
nouveau_crtc_connector_get(struct nouveau_crtc *nv_crtc)
{
	struct drm_device *dev = nv_crtc->base.dev;
	struct drm_connector *connector;
	struct drm_crtc *crtc = to_drm_crtc(nv_crtc);

	/* The safest approach is to find an encoder with the right crtc, that
	 * is also linked to a connector. */
	list_for_each_entry(connector, &dev->mode_config.connector_list, head) {
		if (connector->encoder)
			if (connector->encoder->crtc == crtc)
				return nouveau_connector(connector);
	}

	return NULL;
}

static int
nv50_crtc_set_scale(struct nouveau_crtc *nv_crtc, int scaling_mode, bool update)
{
	struct nouveau_connector *nv_connector;
	struct drm_crtc *crtc = &nv_crtc->base;
	struct drm_device *dev = crtc->dev;
	struct nouveau_channel *evo = nv50_display(dev)->master;
	struct drm_display_mode *mode = &crtc->mode;
	u32 ctrl = 0, oX, oY;
	int ret;

	NV_DEBUG_KMS(dev, "\n");

	nv_connector = nouveau_crtc_connector_get(nv_crtc);
	if (!nv_connector || !nv_connector->native_mode) {
		NV_ERROR(dev, "no native mode, forcing panel scaling\n");
		scaling_mode = DRM_MODE_SCALE_NONE;
	}

	/* start off at the resolution we programmed the crtc for, this
	 * effectively handles NONE/FULL scaling
	 */
	if (scaling_mode != DRM_MODE_SCALE_NONE) {
		oX = nv_connector->native_mode->hdisplay;
		oY = nv_connector->native_mode->vdisplay;
	} else {
		oX = mode->hdisplay;
		oY = mode->vdisplay;
	}

	/* add overscan compensation if necessary, will keep the aspect
	 * ratio the same as the backend mode unless overridden by the
	 * user setting both hborder and vborder properties.
	 */
	if (nv_connector && ( nv_connector->underscan == UNDERSCAN_ON ||
			     (nv_connector->underscan == UNDERSCAN_AUTO &&
			      nv_connector->edid &&
			      drm_detect_hdmi_monitor(nv_connector->edid)))) {
		u32 bX = nv_connector->underscan_hborder;
		u32 bY = nv_connector->underscan_vborder;
		u32 aspect = (oY << 19) / oX;

		if (bX) {
			oX -= (bX * 2);
			if (bY) oY -= (bY * 2);
			else    oY  = ((oX * aspect) + (aspect / 2)) >> 19;
		} else {
			oX -= (oX >> 4) + 32;
			if (bY) oY -= (bY * 2);
			else    oY  = ((oX * aspect) + (aspect / 2)) >> 19;
		}
	}

	/* handle CENTER/ASPECT scaling, taking into account the areas
	 * removed already for overscan compensation
	 */
	switch (scaling_mode) {
	case DRM_MODE_SCALE_CENTER:
		oX = min((u32)mode->hdisplay, oX);
		oY = min((u32)mode->vdisplay, oY);
		/* fall-through */
	case DRM_MODE_SCALE_ASPECT:
		if (oY < oX) {
			u32 aspect = (mode->hdisplay << 19) / mode->vdisplay;
			oX = ((oY * aspect) + (aspect / 2)) >> 19;
		} else {
			u32 aspect = (mode->vdisplay << 19) / mode->hdisplay;
			oY = ((oX * aspect) + (aspect / 2)) >> 19;
		}
		break;
	default:
		break;
	}

	if (mode->hdisplay != oX || mode->vdisplay != oY ||
	    mode->flags & DRM_MODE_FLAG_INTERLACE ||
	    mode->flags & DRM_MODE_FLAG_DBLSCAN)
		ctrl |= NV50_EVO_CRTC_SCALE_CTRL_ACTIVE;

	ret = RING_SPACE(evo, 5);
	if (ret)
		return ret;

	BEGIN_RING(evo, 0, NV50_EVO_CRTC(nv_crtc->index, SCALE_CTRL), 1);
	OUT_RING  (evo, ctrl);
	BEGIN_RING(evo, 0, NV50_EVO_CRTC(nv_crtc->index, SCALE_RES1), 2);
	OUT_RING  (evo, oY << 16 | oX);
	OUT_RING  (evo, oY << 16 | oX);

	if (update) {
		nv50_display_flip_stop(crtc);
		nv50_display_sync(dev);
		nv50_display_flip_next(crtc, crtc->fb, NULL);
	}

	return 0;
}

int
nv50_crtc_set_clock(struct drm_device *dev, int head, int pclk)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct pll_lims pll;
	uint32_t reg1, reg2;
	int ret, N1, M1, N2, M2, P;

	ret = get_pll_limits(dev, PLL_VPLL0 + head, &pll);
	if (ret)
		return ret;

	if (pll.vco2.maxfreq) {
		ret = nv50_calc_pll(dev, &pll, pclk, &N1, &M1, &N2, &M2, &P);
		if (ret <= 0)
			return 0;

		NV_DEBUG(dev, "pclk %d out %d NM1 %d %d NM2 %d %d P %d\n",
			 pclk, ret, N1, M1, N2, M2, P);

		reg1 = nv_rd32(dev, pll.reg + 4) & 0xff00ff00;
		reg2 = nv_rd32(dev, pll.reg + 8) & 0x8000ff00;
		nv_wr32(dev, pll.reg + 0, 0x10000611);
		nv_wr32(dev, pll.reg + 4, reg1 | (M1 << 16) | N1);
		nv_wr32(dev, pll.reg + 8, reg2 | (P << 28) | (M2 << 16) | N2);
	} else
	if (dev_priv->chipset < NV_C0) {
		ret = nva3_calc_pll(dev, &pll, pclk, &N1, &N2, &M1, &P);
		if (ret <= 0)
			return 0;

		NV_DEBUG(dev, "pclk %d out %d N %d fN 0x%04x M %d P %d\n",
			 pclk, ret, N1, N2, M1, P);

		reg1 = nv_rd32(dev, pll.reg + 4) & 0xffc00000;
		nv_wr32(dev, pll.reg + 0, 0x50000610);
		nv_wr32(dev, pll.reg + 4, reg1 | (P << 16) | (M1 << 8) | N1);
		nv_wr32(dev, pll.reg + 8, N2);
	} else {
		ret = nva3_calc_pll(dev, &pll, pclk, &N1, &N2, &M1, &P);
		if (ret <= 0)
			return 0;

		NV_DEBUG(dev, "pclk %d out %d N %d fN 0x%04x M %d P %d\n",
			 pclk, ret, N1, N2, M1, P);

		nv_mask(dev, pll.reg + 0x0c, 0x00000000, 0x00000100);
		nv_wr32(dev, pll.reg + 0x04, (P << 16) | (N1 << 8) | M1);
		nv_wr32(dev, pll.reg + 0x10, N2 << 16);
	}

	return 0;
}

static void
nv50_crtc_destroy(struct drm_crtc *crtc)
{
	struct drm_device *dev;
	struct nouveau_crtc *nv_crtc;

	if (!crtc)
		return;

	dev = crtc->dev;
	nv_crtc = nouveau_crtc(crtc);

	NV_DEBUG_KMS(dev, "\n");

	drm_crtc_cleanup(&nv_crtc->base);

	nouveau_bo_unmap(nv_crtc->lut.nvbo);
	nouveau_bo_ref(NULL, &nv_crtc->lut.nvbo);
	nouveau_bo_unmap(nv_crtc->cursor.nvbo);
	nouveau_bo_ref(NULL, &nv_crtc->cursor.nvbo);
	kfree(nv_crtc->mode);
	kfree(nv_crtc);
}

int
nv50_crtc_cursor_set(struct drm_crtc *crtc, struct drm_file *file_priv,
		     uint32_t buffer_handle, uint32_t width, uint32_t height)
{
	struct drm_device *dev = crtc->dev;
	struct nouveau_crtc *nv_crtc = nouveau_crtc(crtc);
	struct nouveau_bo *cursor = NULL;
	struct drm_gem_object *gem;
	int ret = 0, i;

	if (!buffer_handle) {
		nv_crtc->cursor.hide(nv_crtc, true);
		return 0;
	}

	if (width != 64 || height != 64)
		return -EINVAL;

	gem = drm_gem_object_lookup(dev, file_priv, buffer_handle);
	if (!gem)
		return -ENOENT;
	cursor = nouveau_gem_object(gem);

	ret = nouveau_bo_map(cursor);
	if (ret)
		goto out;

	/* The simple will do for now. */
	for (i = 0; i < 64 * 64; i++)
		nouveau_bo_wr32(nv_crtc->cursor.nvbo, i, nouveau_bo_rd32(cursor, i));

	nouveau_bo_unmap(cursor);

	nv_crtc->cursor.set_offset(nv_crtc, nv_crtc->cursor.nvbo->bo.offset);
	nv_crtc->cursor.show(nv_crtc, true);

out:
	drm_gem_object_unreference_unlocked(gem);
	return ret;
}

int
nv50_crtc_cursor_move(struct drm_crtc *crtc, int x, int y)
{
	struct nouveau_crtc *nv_crtc = nouveau_crtc(crtc);

	nv_crtc->cursor.set_pos(nv_crtc, x, y);
	return 0;
}

static void
nv50_crtc_gamma_set(struct drm_crtc *crtc, u16 *r, u16 *g, u16 *b,
		    uint32_t start, uint32_t size)
{
	int end = (start + size > 256) ? 256 : start + size, i;
	struct nouveau_crtc *nv_crtc = nouveau_crtc(crtc);

	for (i = start; i < end; i++) {
		nv_crtc->lut.r[i] = r[i];
		nv_crtc->lut.g[i] = g[i];
		nv_crtc->lut.b[i] = b[i];
	}

	/* We need to know the depth before we upload, but it's possible to
	 * get called before a framebuffer is bound.  If this is the case,
	 * mark the lut values as dirty by setting depth==0, and it'll be
	 * uploaded on the first mode_set_base()
	 */
	if (!nv_crtc->base.fb) {
		nv_crtc->lut.depth = 0;
		return;
	}

	nv50_crtc_lut_load(crtc);
}

static void
nv50_crtc_save(struct drm_crtc *crtc)
{
	NV_ERROR(crtc->dev, "!!\n");
}

static void
nv50_crtc_restore(struct drm_crtc *crtc)
{
	NV_ERROR(crtc->dev, "!!\n");
}

static const struct drm_crtc_funcs nv50_crtc_funcs = {
	.save = nv50_crtc_save,
	.restore = nv50_crtc_restore,
	.cursor_set = nv50_crtc_cursor_set,
	.cursor_move = nv50_crtc_cursor_move,
	.gamma_set = nv50_crtc_gamma_set,
	.set_config = drm_crtc_helper_set_config,
	.page_flip = nouveau_crtc_page_flip,
	.destroy = nv50_crtc_destroy,
};

static void
nv50_crtc_dpms(struct drm_crtc *crtc, int mode)
{
}

static void
nv50_crtc_prepare(struct drm_crtc *crtc)
{
	struct nouveau_crtc *nv_crtc = nouveau_crtc(crtc);
	struct drm_device *dev = crtc->dev;

	NV_DEBUG_KMS(dev, "index %d\n", nv_crtc->index);

	nv50_display_flip_stop(crtc);
	drm_vblank_pre_modeset(dev, nv_crtc->index);
	nv50_crtc_blank(nv_crtc, true);
}

static void
nv50_crtc_commit(struct drm_crtc *crtc)
{
	struct drm_device *dev = crtc->dev;
	struct nouveau_crtc *nv_crtc = nouveau_crtc(crtc);

	NV_DEBUG_KMS(dev, "index %d\n", nv_crtc->index);

	nv50_crtc_blank(nv_crtc, false);
	drm_vblank_post_modeset(dev, nv_crtc->index);
	nv50_display_sync(dev);
	nv50_display_flip_next(crtc, crtc->fb, NULL);
}

static bool
nv50_crtc_mode_fixup(struct drm_crtc *crtc, struct drm_display_mode *mode,
		     struct drm_display_mode *adjusted_mode)
{
	return true;
}

static int
nv50_crtc_do_mode_set_base(struct drm_crtc *crtc,
			   struct drm_framebuffer *passed_fb,
			   int x, int y, bool atomic)
{
	struct nouveau_crtc *nv_crtc = nouveau_crtc(crtc);
	struct drm_device *dev = nv_crtc->base.dev;
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct nouveau_channel *evo = nv50_display(dev)->master;
	struct drm_framebuffer *drm_fb;
	struct nouveau_framebuffer *fb;
	int ret;

	NV_DEBUG_KMS(dev, "index %d\n", nv_crtc->index);

	/* no fb bound */
	if (!atomic && !crtc->fb) {
		NV_DEBUG_KMS(dev, "No FB bound\n");
		return 0;
	}

	/* If atomic, we want to switch to the fb we were passed, so
	 * now we update pointers to do that.  (We don't pin; just
	 * assume we're already pinned and update the base address.)
	 */
	if (atomic) {
		drm_fb = passed_fb;
		fb = nouveau_framebuffer(passed_fb);
	} else {
		drm_fb = crtc->fb;
		fb = nouveau_framebuffer(crtc->fb);
		/* If not atomic, we can go ahead and pin, and unpin the
		 * old fb we were passed.
		 */
		ret = nouveau_bo_pin(fb->nvbo, TTM_PL_FLAG_VRAM);
		if (ret)
			return ret;

		if (passed_fb) {
			struct nouveau_framebuffer *ofb = nouveau_framebuffer(passed_fb);
			nouveau_bo_unpin(ofb->nvbo);
		}
	}

	nv_crtc->fb.offset = fb->nvbo->bo.offset;
	nv_crtc->fb.tile_flags = nouveau_bo_tile_layout(fb->nvbo);
	nv_crtc->fb.cpp = drm_fb->bits_per_pixel / 8;
	if (!nv_crtc->fb.blanked && dev_priv->chipset != 0x50) {
		ret = RING_SPACE(evo, 2);
		if (ret)
			return ret;

		BEGIN_RING(evo, 0, NV50_EVO_CRTC(nv_crtc->index, FB_DMA), 1);
		OUT_RING  (evo, fb->r_dma);
	}

	ret = RING_SPACE(evo, 12);
	if (ret)
		return ret;

	BEGIN_RING(evo, 0, NV50_EVO_CRTC(nv_crtc->index, FB_OFFSET), 5);
	OUT_RING  (evo, nv_crtc->fb.offset >> 8);
	OUT_RING  (evo, 0);
	OUT_RING  (evo, (drm_fb->height << 16) | drm_fb->width);
	OUT_RING  (evo, fb->r_pitch);
	OUT_RING  (evo, fb->r_format);

	BEGIN_RING(evo, 0, NV50_EVO_CRTC(nv_crtc->index, CLUT_MODE), 1);
	OUT_RING  (evo, fb->base.depth == 8 ?
		   NV50_EVO_CRTC_CLUT_MODE_OFF : NV50_EVO_CRTC_CLUT_MODE_ON);

	BEGIN_RING(evo, 0, NV50_EVO_CRTC(nv_crtc->index, COLOR_CTRL), 1);
	OUT_RING  (evo, NV50_EVO_CRTC_COLOR_CTRL_COLOR);
	BEGIN_RING(evo, 0, NV50_EVO_CRTC(nv_crtc->index, FB_POS), 1);
	OUT_RING  (evo, (y << 16) | x);

	if (nv_crtc->lut.depth != fb->base.depth) {
		nv_crtc->lut.depth = fb->base.depth;
		nv50_crtc_lut_load(crtc);
	}

	return 0;
}

static int
nv50_crtc_mode_set(struct drm_crtc *crtc, struct drm_display_mode *mode,
		   struct drm_display_mode *adjusted_mode, int x, int y,
		   struct drm_framebuffer *old_fb)
{
	struct drm_device *dev = crtc->dev;
	struct nouveau_channel *evo = nv50_display(dev)->master;
	struct nouveau_crtc *nv_crtc = nouveau_crtc(crtc);
	struct nouveau_connector *nv_connector = NULL;
	uint32_t hsync_dur,  vsync_dur, hsync_start_to_end, vsync_start_to_end;
	uint32_t hunk1, vunk1, vunk2a, vunk2b;
	int ret;

	/* Find the connector attached to this CRTC */
	nv_connector = nouveau_crtc_connector_get(nv_crtc);

	*nv_crtc->mode = *adjusted_mode;

	NV_DEBUG_KMS(dev, "index %d\n", nv_crtc->index);

	hsync_dur = adjusted_mode->hsync_end - adjusted_mode->hsync_start;
	vsync_dur = adjusted_mode->vsync_end - adjusted_mode->vsync_start;
	hsync_start_to_end = adjusted_mode->htotal - adjusted_mode->hsync_start;
	vsync_start_to_end = adjusted_mode->vtotal - adjusted_mode->vsync_start;
	/* I can't give this a proper name, anyone else can? */
	hunk1 = adjusted_mode->htotal -
		adjusted_mode->hsync_start + adjusted_mode->hdisplay;
	vunk1 = adjusted_mode->vtotal -
		adjusted_mode->vsync_start + adjusted_mode->vdisplay;
	/* Another strange value, this time only for interlaced adjusted_modes. */
	vunk2a = 2 * adjusted_mode->vtotal -
		 adjusted_mode->vsync_start + adjusted_mode->vdisplay;
	vunk2b = adjusted_mode->vtotal -
		 adjusted_mode->vsync_start + adjusted_mode->vtotal;

	if (adjusted_mode->flags & DRM_MODE_FLAG_INTERLACE) {
		vsync_dur /= 2;
		vsync_start_to_end  /= 2;
		vunk1 /= 2;
		vunk2a /= 2;
		vunk2b /= 2;
		/* magic */
		if (adjusted_mode->flags & DRM_MODE_FLAG_DBLSCAN) {
			vsync_start_to_end -= 1;
			vunk1 -= 1;
			vunk2a -= 1;
			vunk2b -= 1;
		}
	}

	ret = RING_SPACE(evo, 19);
	if (ret)
		return ret;

	BEGIN_RING(evo, 0, NV50_EVO_CRTC(nv_crtc->index, CLOCK), 2);
	OUT_RING(evo, adjusted_mode->clock | 0x800000);
	OUT_RING(evo, (adjusted_mode->flags & DRM_MODE_FLAG_INTERLACE) ? 2 : 0);

	BEGIN_RING(evo, 0, NV50_EVO_CRTC(nv_crtc->index, DISPLAY_START), 5);
	OUT_RING(evo, 0);
	OUT_RING(evo, (adjusted_mode->vtotal << 16) | adjusted_mode->htotal);
	OUT_RING(evo, (vsync_dur - 1) << 16 | (hsync_dur - 1));
	OUT_RING(evo, (vsync_start_to_end - 1) << 16 |
			(hsync_start_to_end - 1));
	OUT_RING(evo, (vunk1 - 1) << 16 | (hunk1 - 1));

	if (adjusted_mode->flags & DRM_MODE_FLAG_INTERLACE) {
		BEGIN_RING(evo, 0, NV50_EVO_CRTC(nv_crtc->index, UNK0824), 1);
		OUT_RING(evo, (vunk2b - 1) << 16 | (vunk2a - 1));
	} else {
		OUT_RING(evo, 0);
		OUT_RING(evo, 0);
	}

	BEGIN_RING(evo, 0, NV50_EVO_CRTC(nv_crtc->index, UNK082C), 1);
	OUT_RING  (evo, 0);
	/* required to make display sync channel not hate life */
	BEGIN_RING(evo, 0, NV50_EVO_CRTC(nv_crtc->index, UNK900), 1);
	OUT_RING  (evo, 0x00000311);

	/* This is the actual resolution of the mode. */
	BEGIN_RING(evo, 0, NV50_EVO_CRTC(nv_crtc->index, REAL_RES), 1);
	OUT_RING(evo, (mode->vdisplay << 16) | mode->hdisplay);
	BEGIN_RING(evo, 0, NV50_EVO_CRTC(nv_crtc->index, SCALE_CENTER_OFFSET), 1);
	OUT_RING(evo, NV50_EVO_CRTC_SCALE_CENTER_OFFSET_VAL(0, 0));

	nv_crtc->set_dither(nv_crtc, nv_connector->use_dithering, false);
	nv_crtc->set_scale(nv_crtc, nv_connector->scaling_mode, false);

	return nv50_crtc_do_mode_set_base(crtc, old_fb, x, y, false);
}

static int
nv50_crtc_mode_set_base(struct drm_crtc *crtc, int x, int y,
			struct drm_framebuffer *old_fb)
{
	int ret;

	nv50_display_flip_stop(crtc);
	ret = nv50_crtc_do_mode_set_base(crtc, old_fb, x, y, false);
	if (ret)
		return ret;

	ret = nv50_display_sync(crtc->dev);
	if (ret)
		return ret;

	return nv50_display_flip_next(crtc, crtc->fb, NULL);
}

static int
nv50_crtc_mode_set_base_atomic(struct drm_crtc *crtc,
			       struct drm_framebuffer *fb,
			       int x, int y, enum mode_set_atomic state)
{
	int ret;

	nv50_display_flip_stop(crtc);
	ret = nv50_crtc_do_mode_set_base(crtc, fb, x, y, true);
	if (ret)
		return ret;

	return nv50_display_sync(crtc->dev);
}

static const struct drm_crtc_helper_funcs nv50_crtc_helper_funcs = {
	.dpms = nv50_crtc_dpms,
	.prepare = nv50_crtc_prepare,
	.commit = nv50_crtc_commit,
	.mode_fixup = nv50_crtc_mode_fixup,
	.mode_set = nv50_crtc_mode_set,
	.mode_set_base = nv50_crtc_mode_set_base,
	.mode_set_base_atomic = nv50_crtc_mode_set_base_atomic,
	.load_lut = nv50_crtc_lut_load,
};

int
nv50_crtc_create(struct drm_device *dev, int index)
{
	struct nouveau_crtc *nv_crtc = NULL;
	int ret, i;

	NV_DEBUG_KMS(dev, "\n");

	nv_crtc = kzalloc(sizeof(*nv_crtc), GFP_KERNEL);
	if (!nv_crtc)
		return -ENOMEM;

	nv_crtc->mode = kzalloc(sizeof(*nv_crtc->mode), GFP_KERNEL);
	if (!nv_crtc->mode) {
		kfree(nv_crtc);
		return -ENOMEM;
	}

	/* Default CLUT parameters, will be activated on the hw upon
	 * first mode set.
	 */
	for (i = 0; i < 256; i++) {
		nv_crtc->lut.r[i] = i << 8;
		nv_crtc->lut.g[i] = i << 8;
		nv_crtc->lut.b[i] = i << 8;
	}
	nv_crtc->lut.depth = 0;

	ret = nouveau_bo_new(dev, 4096, 0x100, TTM_PL_FLAG_VRAM,
			     0, 0x0000, &nv_crtc->lut.nvbo);
	if (!ret) {
		ret = nouveau_bo_pin(nv_crtc->lut.nvbo, TTM_PL_FLAG_VRAM);
		if (!ret)
			ret = nouveau_bo_map(nv_crtc->lut.nvbo);
		if (ret)
			nouveau_bo_ref(NULL, &nv_crtc->lut.nvbo);
	}

	if (ret) {
		kfree(nv_crtc->mode);
		kfree(nv_crtc);
		return ret;
	}

	nv_crtc->index = index;

	/* set function pointers */
	nv_crtc->set_dither = nv50_crtc_set_dither;
	nv_crtc->set_scale = nv50_crtc_set_scale;

	drm_crtc_init(dev, &nv_crtc->base, &nv50_crtc_funcs);
	drm_crtc_helper_add(&nv_crtc->base, &nv50_crtc_helper_funcs);
	drm_mode_crtc_set_gamma_size(&nv_crtc->base, 256);

	ret = nouveau_bo_new(dev, 64*64*4, 0x100, TTM_PL_FLAG_VRAM,
			     0, 0x0000, &nv_crtc->cursor.nvbo);
	if (!ret) {
		ret = nouveau_bo_pin(nv_crtc->cursor.nvbo, TTM_PL_FLAG_VRAM);
		if (!ret)
			ret = nouveau_bo_map(nv_crtc->cursor.nvbo);
		if (ret)
			nouveau_bo_ref(NULL, &nv_crtc->cursor.nvbo);
	}

	nv50_cursor_init(nv_crtc);
	return 0;
}
