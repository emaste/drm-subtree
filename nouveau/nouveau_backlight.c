/*
 * Copyright (C) 2009 Red Hat <mjg@redhat.com>
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

/*
 * Authors:
 *  Matthew Garrett <mjg@redhat.com>
 *
 * Register locations derived from NVClock by Roderick Colenbrander
 */

#include <linux/backlight.h>
#include <linux/acpi.h>

#include "drmP.h"
#include "nouveau_drv.h"
#include "nouveau_drm.h"
#include "nouveau_reg.h"
#include "nouveau_encoder.h"

static int
nv40_get_intensity(struct backlight_device *bd)
{
	struct drm_device *dev = bl_get_data(bd);
	int val = (nv_rd32(dev, NV40_PMC_BACKLIGHT) & NV40_PMC_BACKLIGHT_MASK)
									>> 16;

	return val;
}

static int
nv40_set_intensity(struct backlight_device *bd)
{
	struct drm_device *dev = bl_get_data(bd);
	int val = bd->props.brightness;
	int reg = nv_rd32(dev, NV40_PMC_BACKLIGHT);

	nv_wr32(dev, NV40_PMC_BACKLIGHT,
		 (val << 16) | (reg & ~NV40_PMC_BACKLIGHT_MASK));

	return 0;
}

static const struct backlight_ops nv40_bl_ops = {
	.options = BL_CORE_SUSPENDRESUME,
	.get_brightness = nv40_get_intensity,
	.update_status = nv40_set_intensity,
};

static int
nv40_backlight_init(struct drm_connector *connector)
{
	struct drm_device *dev = connector->dev;
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct backlight_properties props;
	struct backlight_device *bd;

	if (!(nv_rd32(dev, NV40_PMC_BACKLIGHT) & NV40_PMC_BACKLIGHT_MASK))
		return 0;

	memset(&props, 0, sizeof(struct backlight_properties));
	props.type = BACKLIGHT_RAW;
	props.max_brightness = 31;
	bd = backlight_device_register("nv_backlight", &connector->kdev, dev,
				       &nv40_bl_ops, &props);
	if (IS_ERR(bd))
		return PTR_ERR(bd);

	dev_priv->backlight = bd;
	bd->props.brightness = nv40_get_intensity(bd);
	backlight_update_status(bd);

	return 0;
}

static int
nv50_get_intensity(struct backlight_device *bd)
{
	struct nouveau_encoder *nv_encoder = bl_get_data(bd);
	struct drm_device *dev = nv_encoder->base.base.dev;
	int or = nv_encoder->or;

	return nv_rd32(dev, NV50_PDISPLAY_SOR_BACKLIGHT + (or * 0x800));
}

static int
nv50_set_intensity(struct backlight_device *bd)
{
	struct nouveau_encoder *nv_encoder = bl_get_data(bd);
	struct drm_device *dev = nv_encoder->base.base.dev;
	int val = bd->props.brightness;
	int or = nv_encoder->or;

	nv_wr32(dev, NV50_PDISPLAY_SOR_BACKLIGHT + (or * 0x800),
		val | NV50_PDISPLAY_SOR_BACKLIGHT_ENABLE);
	return 0;
}

static const struct backlight_ops nv50_bl_ops = {
	.options = BL_CORE_SUSPENDRESUME,
	.get_brightness = nv50_get_intensity,
	.update_status = nv50_set_intensity,
};

static int
nv50_backlight_init(struct drm_connector *connector)
{
	struct drm_device *dev = connector->dev;
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct nouveau_encoder *nv_encoder;
	struct backlight_properties props;
	struct backlight_device *bd;
	int or;

	nv_encoder = find_encoder(connector, OUTPUT_LVDS);
	if (!nv_encoder) {
		nv_encoder = find_encoder(connector, OUTPUT_DP);
		if (!nv_encoder)
			return -ENODEV;
	}

	or = nv_encoder->or;

	if (!nv_rd32(dev, NV50_PDISPLAY_SOR_BACKLIGHT + (or * 0x800)))
		return 0;

	memset(&props, 0, sizeof(struct backlight_properties));
	props.type = BACKLIGHT_RAW;
	props.max_brightness = 1025;
	bd = backlight_device_register("nv_backlight", &connector->kdev,
				       nv_encoder, &nv50_bl_ops, &props);
	if (IS_ERR(bd))
		return PTR_ERR(bd);

	dev_priv->backlight = bd;
	bd->props.brightness = nv50_get_intensity(bd);
	backlight_update_status(bd);
	return 0;
}

int
nouveau_backlight_init(struct drm_device *dev)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct drm_connector *connector;

#ifdef CONFIG_ACPI
	if (acpi_video_backlight_support()) {
		NV_INFO(dev, "ACPI backlight interface available, "
			     "not registering our own\n");
		return 0;
	}
#endif

	list_for_each_entry(connector, &dev->mode_config.connector_list, head) {
		if (connector->connector_type != DRM_MODE_CONNECTOR_LVDS &&
		    connector->connector_type != DRM_MODE_CONNECTOR_eDP)
			continue;

		switch (dev_priv->card_type) {
		case NV_40:
			return nv40_backlight_init(connector);
		case NV_50:
			return nv50_backlight_init(connector);
		default:
			break;
		}
	}


	return 0;
}

void
nouveau_backlight_exit(struct drm_device *dev)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;

	if (dev_priv->backlight) {
		backlight_device_unregister(dev_priv->backlight);
		dev_priv->backlight = NULL;
	}
}
