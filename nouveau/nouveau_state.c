/*
 * Copyright 2005 Stephane Marchesin
 * Copyright 2008 Stuart Bennett
 * All Rights Reserved.
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
 * PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <linux/swab.h>
#include <linux/slab.h>
#include "drmP.h"
#include "drm.h"
#include "drm_sarea.h"
#include "drm_crtc_helper.h"
#include <linux/vgaarb.h>
#include <linux/vga_switcheroo.h>

#include "nouveau_drv.h"
#include <nouveau_drm.h>
#include "nouveau_fbcon.h"
#include <core/ramht.h>
#include "nouveau_pm.h"
#include "nv04_display.h"
#include "nv50_display.h"
#include <engine/fifo.h>
#include "nouveau_fence.h"
#include "nouveau_software.h"

static void nouveau_stub_takedown(struct drm_device *dev) {}
static int nouveau_stub_init(struct drm_device *dev) { return 0; }

static int nouveau_init_engine_ptrs(struct drm_device *dev)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct nouveau_engine *engine = &dev_priv->engine;

	switch (dev_priv->chipset & 0xf0) {
	case 0x00:
		engine->display.early_init	= nv04_display_early_init;
		engine->display.late_takedown	= nv04_display_late_takedown;
		engine->display.create		= nv04_display_create;
		engine->display.destroy		= nv04_display_destroy;
		engine->display.init		= nv04_display_init;
		engine->display.fini		= nv04_display_fini;
		engine->pm.clocks_get		= nv04_pm_clocks_get;
		engine->pm.clocks_pre		= nv04_pm_clocks_pre;
		engine->pm.clocks_set		= nv04_pm_clocks_set;
		break;
	case 0x10:
		engine->display.early_init	= nv04_display_early_init;
		engine->display.late_takedown	= nv04_display_late_takedown;
		engine->display.create		= nv04_display_create;
		engine->display.destroy		= nv04_display_destroy;
		engine->display.init		= nv04_display_init;
		engine->display.fini		= nv04_display_fini;
		engine->pm.clocks_get		= nv04_pm_clocks_get;
		engine->pm.clocks_pre		= nv04_pm_clocks_pre;
		engine->pm.clocks_set		= nv04_pm_clocks_set;
		break;
	case 0x20:
		engine->display.early_init	= nv04_display_early_init;
		engine->display.late_takedown	= nv04_display_late_takedown;
		engine->display.create		= nv04_display_create;
		engine->display.destroy		= nv04_display_destroy;
		engine->display.init		= nv04_display_init;
		engine->display.fini		= nv04_display_fini;
		engine->pm.clocks_get		= nv04_pm_clocks_get;
		engine->pm.clocks_pre		= nv04_pm_clocks_pre;
		engine->pm.clocks_set		= nv04_pm_clocks_set;
		break;
	case 0x30:
		engine->display.early_init	= nv04_display_early_init;
		engine->display.late_takedown	= nv04_display_late_takedown;
		engine->display.create		= nv04_display_create;
		engine->display.destroy		= nv04_display_destroy;
		engine->display.init		= nv04_display_init;
		engine->display.fini		= nv04_display_fini;
		engine->pm.clocks_get		= nv04_pm_clocks_get;
		engine->pm.clocks_pre		= nv04_pm_clocks_pre;
		engine->pm.clocks_set		= nv04_pm_clocks_set;
		engine->pm.voltage_get		= nouveau_voltage_gpio_get;
		engine->pm.voltage_set		= nouveau_voltage_gpio_set;
		break;
	case 0x40:
	case 0x60:
		engine->display.early_init	= nv04_display_early_init;
		engine->display.late_takedown	= nv04_display_late_takedown;
		engine->display.create		= nv04_display_create;
		engine->display.destroy		= nv04_display_destroy;
		engine->display.init		= nv04_display_init;
		engine->display.fini		= nv04_display_fini;
		engine->pm.clocks_get		= nv40_pm_clocks_get;
		engine->pm.clocks_pre		= nv40_pm_clocks_pre;
		engine->pm.clocks_set		= nv40_pm_clocks_set;
		engine->pm.voltage_get		= nouveau_voltage_gpio_get;
		engine->pm.voltage_set		= nouveau_voltage_gpio_set;
		engine->pm.temp_get		= nv40_temp_get;
		engine->pm.pwm_get		= nv40_pm_pwm_get;
		engine->pm.pwm_set		= nv40_pm_pwm_set;
		break;
	case 0x50:
	case 0x80: /* gotta love NVIDIA's consistency.. */
	case 0x90:
	case 0xa0:
		engine->display.early_init	= nv50_display_early_init;
		engine->display.late_takedown	= nv50_display_late_takedown;
		engine->display.create		= nv50_display_create;
		engine->display.destroy		= nv50_display_destroy;
		engine->display.init		= nv50_display_init;
		engine->display.fini		= nv50_display_fini;
		switch (dev_priv->chipset) {
		case 0x84:
		case 0x86:
		case 0x92:
		case 0x94:
		case 0x96:
		case 0x98:
		case 0xa0:
		case 0xaa:
		case 0xac:
		case 0x50:
			engine->pm.clocks_get	= nv50_pm_clocks_get;
			engine->pm.clocks_pre	= nv50_pm_clocks_pre;
			engine->pm.clocks_set	= nv50_pm_clocks_set;
			break;
		default:
			engine->pm.clocks_get	= nva3_pm_clocks_get;
			engine->pm.clocks_pre	= nva3_pm_clocks_pre;
			engine->pm.clocks_set	= nva3_pm_clocks_set;
			break;
		}
		engine->pm.voltage_get		= nouveau_voltage_gpio_get;
		engine->pm.voltage_set		= nouveau_voltage_gpio_set;
		if (dev_priv->chipset >= 0x84)
			engine->pm.temp_get	= nv84_temp_get;
		else
			engine->pm.temp_get	= nv40_temp_get;
		engine->pm.pwm_get		= nv50_pm_pwm_get;
		engine->pm.pwm_set		= nv50_pm_pwm_set;
		break;
	case 0xc0:
		engine->display.early_init	= nv50_display_early_init;
		engine->display.late_takedown	= nv50_display_late_takedown;
		engine->display.create		= nv50_display_create;
		engine->display.destroy		= nv50_display_destroy;
		engine->display.init		= nv50_display_init;
		engine->display.fini		= nv50_display_fini;
		engine->pm.temp_get		= nv84_temp_get;
		engine->pm.clocks_get		= nvc0_pm_clocks_get;
		engine->pm.clocks_pre		= nvc0_pm_clocks_pre;
		engine->pm.clocks_set		= nvc0_pm_clocks_set;
		engine->pm.voltage_get		= nouveau_voltage_gpio_get;
		engine->pm.voltage_set		= nouveau_voltage_gpio_set;
		engine->pm.pwm_get		= nv50_pm_pwm_get;
		engine->pm.pwm_set		= nv50_pm_pwm_set;
		break;
	case 0xd0:
		engine->display.early_init	= nouveau_stub_init;
		engine->display.late_takedown	= nouveau_stub_takedown;
		engine->display.create		= nvd0_display_create;
		engine->display.destroy		= nvd0_display_destroy;
		engine->display.init		= nvd0_display_init;
		engine->display.fini		= nvd0_display_fini;
		engine->pm.temp_get		= nv84_temp_get;
		engine->pm.clocks_get		= nvc0_pm_clocks_get;
		engine->pm.clocks_pre		= nvc0_pm_clocks_pre;
		engine->pm.clocks_set		= nvc0_pm_clocks_set;
		engine->pm.voltage_get		= nouveau_voltage_gpio_get;
		engine->pm.voltage_set		= nouveau_voltage_gpio_set;
		break;
	case 0xe0:
		engine->display.early_init	= nouveau_stub_init;
		engine->display.late_takedown	= nouveau_stub_takedown;
		engine->display.create		= nvd0_display_create;
		engine->display.destroy		= nvd0_display_destroy;
		engine->display.init		= nvd0_display_init;
		engine->display.fini		= nvd0_display_fini;
		break;
	default:
		NV_ERROR(dev, "NV%02x unsupported\n", dev_priv->chipset);
		return 1;
	}

	/* headless mode */
	if (nouveau_modeset == 2) {
		engine->display.early_init = nouveau_stub_init;
		engine->display.late_takedown = nouveau_stub_takedown;
		engine->display.create = nouveau_stub_init;
		engine->display.init = nouveau_stub_init;
		engine->display.destroy = nouveau_stub_takedown;
	}

	return 0;
}

static unsigned int
nouveau_vga_set_decode(void *priv, bool state)
{
	struct drm_device *dev = priv;
	struct drm_nouveau_private *dev_priv = dev->dev_private;

	if (dev_priv->chipset >= 0x40)
		nv_wr32(dev, 0x88054, state);
	else
		nv_wr32(dev, 0x1854, state);

	if (state)
		return VGA_RSRC_LEGACY_IO | VGA_RSRC_LEGACY_MEM |
		       VGA_RSRC_NORMAL_IO | VGA_RSRC_NORMAL_MEM;
	else
		return VGA_RSRC_NORMAL_IO | VGA_RSRC_NORMAL_MEM;
}

static void nouveau_switcheroo_set_state(struct pci_dev *pdev,
					 enum vga_switcheroo_state state)
{
	struct drm_device *dev = pci_get_drvdata(pdev);
	pm_message_t pmm = { .event = PM_EVENT_SUSPEND };
	if (state == VGA_SWITCHEROO_ON) {
		printk(KERN_ERR "VGA switcheroo: switched nouveau on\n");
		dev->switch_power_state = DRM_SWITCH_POWER_CHANGING;
		nouveau_pci_resume(pdev);
		drm_kms_helper_poll_enable(dev);
		dev->switch_power_state = DRM_SWITCH_POWER_ON;
	} else {
		printk(KERN_ERR "VGA switcheroo: switched nouveau off\n");
		dev->switch_power_state = DRM_SWITCH_POWER_CHANGING;
		drm_kms_helper_poll_disable(dev);
		nouveau_switcheroo_optimus_dsm();
		nouveau_pci_suspend(pdev, pmm);
		dev->switch_power_state = DRM_SWITCH_POWER_OFF;
	}
}

static void nouveau_switcheroo_reprobe(struct pci_dev *pdev)
{
	struct drm_device *dev = pci_get_drvdata(pdev);
	nouveau_fbcon_output_poll_changed(dev);
}

static bool nouveau_switcheroo_can_switch(struct pci_dev *pdev)
{
	struct drm_device *dev = pci_get_drvdata(pdev);
	bool can_switch;

	spin_lock(&dev->count_lock);
	can_switch = (dev->open_count == 0);
	spin_unlock(&dev->count_lock);
	return can_switch;
}

static void
nouveau_card_channel_fini(struct drm_device *dev)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;

	if (dev_priv->channel) {
		nouveau_channel_put_unlocked(&dev_priv->channel);
		nouveau_vm_ref(NULL, &dev_priv->chan_vm, NULL);
	}
}

static int
nouveau_card_channel_init(struct drm_device *dev)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct nouveau_channel *chan;
	int ret;

	ret = nouveau_vm_new(dev, 0, (1ULL << 40), 0x1000, &dev_priv->chan_vm);
	if (ret)
		return ret;

	ret = nouveau_channel_alloc(dev, &chan, NULL, NvDmaFB, NvDmaTT);
	dev_priv->channel = chan;
	if (ret)
		return ret;
	mutex_unlock(&dev_priv->channel->mutex);

	nouveau_bo_move_init(chan);
	return 0;
}

static const struct vga_switcheroo_client_ops nouveau_switcheroo_ops = {
	.set_gpu_state = nouveau_switcheroo_set_state,
	.reprobe = nouveau_switcheroo_reprobe,
	.can_switch = nouveau_switcheroo_can_switch,
};

int
nouveau_card_init(struct drm_device *dev)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct nouveau_engine *engine;
	int ret, e = 0;

	vga_client_register(dev->pdev, dev, NULL, nouveau_vga_set_decode);
	vga_switcheroo_register_client(dev->pdev, &nouveau_switcheroo_ops);

	/* Initialise internal driver API hooks */
	ret = nouveau_init_engine_ptrs(dev);
	if (ret)
		goto out;
	engine = &dev_priv->engine;
	spin_lock_init(&dev_priv->channels.lock);
	spin_lock_init(&dev_priv->tile.lock);
	spin_lock_init(&dev_priv->context_switch_lock);
	spin_lock_init(&dev_priv->vm_lock);
	INIT_LIST_HEAD(&dev_priv->classes);

	/* Make the CRTCs and I2C buses accessible */
	ret = engine->display.early_init(dev);
	if (ret)
		goto out;

	/* Parse BIOS tables / Run init tables if card not POSTed */
	ret = nouveau_bios_init(dev);
	if (ret)
		goto out_display_early;

	/* workaround an odd issue on nvc1 by disabling the device's
	 * nosnoop capability.  hopefully won't cause issues until a
	 * better fix is found - assuming there is one...
	 */
	if (dev_priv->chipset == 0xc1) {
		nv_mask(dev, 0x00088080, 0x00000800, 0x00000000);
	}

	ret = nouveau_mem_vram_init(dev);
	if (ret)
		goto out_bios;

	ret = nouveau_mem_gart_init(dev);
	if (ret)
		goto out_ttmvram;

	if (!dev_priv->noaccel) {
		switch (dev_priv->card_type) {
		case NV_04:
			nv04_fifo_create(dev);
			break;
		case NV_10:
		case NV_20:
		case NV_30:
			if (dev_priv->chipset < 0x17)
				nv10_fifo_create(dev);
			else
				nv17_fifo_create(dev);
			break;
		case NV_40:
			nv40_fifo_create(dev);
			break;
		case NV_50:
			if (dev_priv->chipset == 0x50)
				nv50_fifo_create(dev);
			else
				nv84_fifo_create(dev);
			break;
		case NV_C0:
		case NV_D0:
			nvc0_fifo_create(dev);
			break;
		case NV_E0:
			nve0_fifo_create(dev);
			break;
		default:
			break;
		}

		switch (dev_priv->card_type) {
		case NV_04:
			nv04_fence_create(dev);
			break;
		case NV_10:
		case NV_20:
		case NV_30:
		case NV_40:
		case NV_50:
			if (dev_priv->chipset < 0x84)
				nv10_fence_create(dev);
			else
				nv84_fence_create(dev);
			break;
		case NV_C0:
		case NV_D0:
		case NV_E0:
			nvc0_fence_create(dev);
			break;
		default:
			break;
		}

		switch (dev_priv->card_type) {
		case NV_04:
		case NV_10:
		case NV_20:
		case NV_30:
		case NV_40:
			nv04_software_create(dev);
			break;
		case NV_50:
			nv50_software_create(dev);
			break;
		case NV_C0:
		case NV_D0:
		case NV_E0:
			nvc0_software_create(dev);
			break;
		default:
			break;
		}

		switch (dev_priv->card_type) {
		case NV_04:
			nv04_graph_create(dev);
			break;
		case NV_10:
			nv10_graph_create(dev);
			break;
		case NV_20:
		case NV_30:
			nv20_graph_create(dev);
			break;
		case NV_40:
			nv40_graph_create(dev);
			break;
		case NV_50:
			nv50_graph_create(dev);
			break;
		case NV_C0:
		case NV_D0:
			nvc0_graph_create(dev);
			break;
		case NV_E0:
			nve0_graph_create(dev);
			break;
		default:
			break;
		}

		switch (dev_priv->chipset) {
		case 0x84:
		case 0x86:
		case 0x92:
		case 0x94:
		case 0x96:
		case 0xa0:
			nv84_crypt_create(dev);
			break;
		case 0x98:
		case 0xaa:
		case 0xac:
			nv98_crypt_create(dev);
			break;
		}

		switch (dev_priv->card_type) {
		case NV_50:
			switch (dev_priv->chipset) {
			case 0xa3:
			case 0xa5:
			case 0xa8:
				nva3_copy_create(dev);
				break;
			}
			break;
		case NV_C0:
			if (!(nv_rd32(dev, 0x022500) & 0x00000200))
				nvc0_copy_create(dev, 1);
		case NV_D0:
			if (!(nv_rd32(dev, 0x022500) & 0x00000100))
				nvc0_copy_create(dev, 0);
			break;
		default:
			break;
		}

		if (dev_priv->chipset >= 0xa3 || dev_priv->chipset == 0x98) {
			nv84_bsp_create(dev);
			nv84_vp_create(dev);
			nv98_ppp_create(dev);
		} else
		if (dev_priv->chipset >= 0x84) {
			nv50_mpeg_create(dev);
			nv84_bsp_create(dev);
			nv84_vp_create(dev);
		} else
		if (dev_priv->chipset >= 0x50) {
			nv50_mpeg_create(dev);
		} else
		if (dev_priv->card_type == NV_40 ||
		    dev_priv->chipset == 0x31 ||
		    dev_priv->chipset == 0x34 ||
		    dev_priv->chipset == 0x36) {
			nv31_mpeg_create(dev);
		}

		for (e = 0; e < NVOBJ_ENGINE_NR; e++) {
			if (dev_priv->eng[e]) {
				ret = dev_priv->eng[e]->init(dev, e);
				if (ret)
					goto out_engine;
			}
		}
	}

	ret = nouveau_irq_init(dev);
	if (ret)
		goto out_engine;

	ret = nouveau_display_create(dev);
	if (ret)
		goto out_irq;

	nouveau_backlight_init(dev);
	nouveau_pm_init(dev);

	if (dev_priv->eng[NVOBJ_ENGINE_GR]) {
		ret = nouveau_card_channel_init(dev);
		if (ret)
			goto out_pm;
	}

	if (dev->mode_config.num_crtc) {
		ret = nouveau_display_init(dev);
		if (ret)
			goto out_chan;

		nouveau_fbcon_init(dev);
	}

	return 0;

out_chan:
	nouveau_card_channel_fini(dev);
out_pm:
	nouveau_pm_fini(dev);
	nouveau_backlight_exit(dev);
	nouveau_display_destroy(dev);
out_irq:
	nouveau_irq_fini(dev);
out_engine:
	if (!dev_priv->noaccel) {
		for (e = e - 1; e >= 0; e--) {
			if (!dev_priv->eng[e])
				continue;
			dev_priv->eng[e]->fini(dev, e, false);
			dev_priv->eng[e]->destroy(dev,e );
		}
	}
	nouveau_mem_gart_fini(dev);
out_ttmvram:
	nouveau_mem_vram_fini(dev);
out_bios:
	nouveau_bios_takedown(dev);
out_display_early:
	engine->display.late_takedown(dev);
out:
	vga_switcheroo_unregister_client(dev->pdev);
	vga_client_register(dev->pdev, NULL, NULL, NULL);
	return ret;
}

static void nouveau_card_takedown(struct drm_device *dev)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct nouveau_engine *engine = &dev_priv->engine;
	int e;

	if (dev->mode_config.num_crtc) {
		nouveau_fbcon_fini(dev);
		nouveau_display_fini(dev);
	}

	nouveau_card_channel_fini(dev);
	nouveau_pm_fini(dev);
	nouveau_backlight_exit(dev);
	nouveau_display_destroy(dev);

	if (!dev_priv->noaccel) {
		for (e = NVOBJ_ENGINE_NR - 1; e >= 0; e--) {
			if (dev_priv->eng[e]) {
				dev_priv->eng[e]->fini(dev, e, false);
				dev_priv->eng[e]->destroy(dev,e );
			}
		}
	}

	if (dev_priv->vga_ram) {
		nouveau_bo_unpin(dev_priv->vga_ram);
		nouveau_bo_ref(NULL, &dev_priv->vga_ram);
	}

	mutex_lock(&dev->struct_mutex);
	ttm_bo_clean_mm(&dev_priv->ttm.bdev, TTM_PL_VRAM);
	ttm_bo_clean_mm(&dev_priv->ttm.bdev, TTM_PL_TT);
	mutex_unlock(&dev->struct_mutex);
	nouveau_mem_gart_fini(dev);
	nouveau_mem_vram_fini(dev);

	nouveau_bios_takedown(dev);
	engine->display.late_takedown(dev);

	nouveau_irq_fini(dev);

	vga_switcheroo_unregister_client(dev->pdev);
	vga_client_register(dev->pdev, NULL, NULL, NULL);
}

int
nouveau_open(struct drm_device *dev, struct drm_file *file_priv)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct nouveau_fpriv *fpriv;
	int ret;

	fpriv = kzalloc(sizeof(*fpriv), GFP_KERNEL);
	if (unlikely(!fpriv))
		return -ENOMEM;

	spin_lock_init(&fpriv->lock);
	INIT_LIST_HEAD(&fpriv->channels);

	if (dev_priv->card_type == NV_50) {
		ret = nouveau_vm_new(dev, 0, (1ULL << 40), 0x0020000000ULL,
				     &fpriv->vm);
		if (ret) {
			kfree(fpriv);
			return ret;
		}
	} else
	if (dev_priv->card_type >= NV_C0) {
		ret = nouveau_vm_new(dev, 0, (1ULL << 40), 0x0008000000ULL,
				     &fpriv->vm);
		if (ret) {
			kfree(fpriv);
			return ret;
		}
	}

	file_priv->driver_priv = fpriv;
	return 0;
}

/* here a client dies, release the stuff that was allocated for its
 * file_priv */
void nouveau_preclose(struct drm_device *dev, struct drm_file *file_priv)
{
	nouveau_channel_cleanup(dev, file_priv);
}

void
nouveau_postclose(struct drm_device *dev, struct drm_file *file_priv)
{
	struct nouveau_fpriv *fpriv = nouveau_fpriv(file_priv);
	nouveau_vm_ref(NULL, &fpriv->vm, NULL);
	kfree(fpriv);
}

/* first module load, setup the mmio/fb mapping */
/* KMS: we need mmio at load time, not when the first drm client opens. */
int nouveau_firstopen(struct drm_device *dev)
{
	return 0;
}

/* if we have an OF card, copy vbios to RAMIN */
static void nouveau_OF_copy_vbios_to_ramin(struct drm_device *dev)
{
#if defined(__powerpc__)
	int size, i;
	const uint32_t *bios;
	struct device_node *dn = pci_device_to_OF_node(dev->pdev);
	if (!dn) {
		NV_INFO(dev, "Unable to get the OF node\n");
		return;
	}

	bios = of_get_property(dn, "NVDA,BMP", &size);
	if (bios) {
		for (i = 0; i < size; i += 4)
			nv_wi32(dev, i, bios[i/4]);
		NV_INFO(dev, "OF bios successfully copied (%d bytes)\n", size);
	} else {
		NV_INFO(dev, "Unable to get the OF bios\n");
	}
#endif
}

static struct apertures_struct *nouveau_get_apertures(struct drm_device *dev)
{
	struct pci_dev *pdev = dev->pdev;
	struct apertures_struct *aper = alloc_apertures(3);
	if (!aper)
		return NULL;

	aper->ranges[0].base = pci_resource_start(pdev, 1);
	aper->ranges[0].size = pci_resource_len(pdev, 1);
	aper->count = 1;

	if (pci_resource_len(pdev, 2)) {
		aper->ranges[aper->count].base = pci_resource_start(pdev, 2);
		aper->ranges[aper->count].size = pci_resource_len(pdev, 2);
		aper->count++;
	}

	if (pci_resource_len(pdev, 3)) {
		aper->ranges[aper->count].base = pci_resource_start(pdev, 3);
		aper->ranges[aper->count].size = pci_resource_len(pdev, 3);
		aper->count++;
	}

	return aper;
}

static int nouveau_remove_conflicting_drivers(struct drm_device *dev)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	bool primary = false;
	dev_priv->apertures = nouveau_get_apertures(dev);
	if (!dev_priv->apertures)
		return -ENOMEM;

#ifdef CONFIG_X86
	primary = dev->pdev->resource[PCI_ROM_RESOURCE].flags & IORESOURCE_ROM_SHADOW;
#endif

	remove_conflicting_framebuffers(dev_priv->apertures, "nouveaufb", primary);
	return 0;
}

void *
nouveau_newpriv(struct drm_device *dev)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	return dev_priv->newpriv;
}

int nouveau_load(struct drm_device *dev, unsigned long flags)
{
	struct drm_nouveau_private *dev_priv;
	uint32_t reg0 = ~0, strap;
	int ret;

	dev_priv = kzalloc(sizeof(*dev_priv), GFP_KERNEL);
	if (!dev_priv) {
		ret = -ENOMEM;
		goto err_out;
	}
	dev_priv->newpriv = dev->dev_private;
	dev->dev_private = dev_priv;
	dev_priv->dev = dev;

	dev_priv->flags = flags & NOUVEAU_FLAGS;

	NV_DEBUG(dev, "vendor: 0x%X device: 0x%X class: 0x%X\n",
		 dev->pci_vendor, dev->pci_device, dev->pdev->class);

	/* determine chipset and derive architecture from it */
	reg0 = nv_rd32(dev, NV03_PMC_BOOT_0);
	if ((reg0 & 0x0f000000) > 0) {
		dev_priv->chipset = (reg0 & 0xff00000) >> 20;
		switch (dev_priv->chipset & 0xf0) {
		case 0x10:
		case 0x20:
		case 0x30:
			dev_priv->card_type = dev_priv->chipset & 0xf0;
			break;
		case 0x40:
		case 0x60:
			dev_priv->card_type = NV_40;
			break;
		case 0x50:
		case 0x80:
		case 0x90:
		case 0xa0:
			dev_priv->card_type = NV_50;
			break;
		case 0xc0:
			dev_priv->card_type = NV_C0;
			break;
		case 0xd0:
			dev_priv->card_type = NV_D0;
			break;
		case 0xe0:
			dev_priv->card_type = NV_E0;
			break;
		default:
			break;
		}
	} else
	if ((reg0 & 0xff00fff0) == 0x20004000) {
		if (reg0 & 0x00f00000)
			dev_priv->chipset = 0x05;
		else
			dev_priv->chipset = 0x04;
		dev_priv->card_type = NV_04;
	}

	if (!dev_priv->card_type) {
		NV_ERROR(dev, "unsupported chipset 0x%08x\n", reg0);
		ret = -EINVAL;
		goto err_priv;
	}

	NV_INFO(dev, "Detected an NV%02x generation card (0x%08x)\n",
		     dev_priv->card_type, reg0);

	/* determine frequency of timing crystal */
	strap = nv_rd32(dev, 0x101000);
	if ( dev_priv->chipset < 0x17 ||
	    (dev_priv->chipset >= 0x20 && dev_priv->chipset <= 0x25))
		strap &= 0x00000040;
	else
		strap &= 0x00400040;

	switch (strap) {
	case 0x00000000: dev_priv->crystal = 13500; break;
	case 0x00000040: dev_priv->crystal = 14318; break;
	case 0x00400000: dev_priv->crystal = 27000; break;
	case 0x00400040: dev_priv->crystal = 25000; break;
	}

	NV_DEBUG(dev, "crystal freq: %dKHz\n", dev_priv->crystal);

	/* Determine whether we'll attempt acceleration or not, some
	 * cards are disabled by default here due to them being known
	 * non-functional, or never been tested due to lack of hw.
	 */
	dev_priv->noaccel = !!nouveau_noaccel;
	if (nouveau_noaccel == -1) {
		switch (dev_priv->chipset) {
		case 0xd9: /* known broken */
		case 0xe4: /* needs binary driver firmware */
		case 0xe7: /* needs binary driver firmware */
			NV_INFO(dev, "acceleration disabled by default, pass "
				     "noaccel=0 to force enable\n");
			dev_priv->noaccel = true;
			break;
		default:
			dev_priv->noaccel = false;
			break;
		}
	}

	ret = nouveau_remove_conflicting_drivers(dev);
	if (ret)
		goto err_priv;

	nouveau_OF_copy_vbios_to_ramin(dev);

	/* Special flags */
	if (dev->pci_device == 0x01a0)
		dev_priv->flags |= NV_NFORCE;
	else if (dev->pci_device == 0x01f0)
		dev_priv->flags |= NV_NFORCE2;

	/* For kernel modesetting, init card now and bring up fbcon */
	ret = nouveau_card_init(dev);
	if (ret)
		goto err_priv;

	return 0;

err_priv:
	dev->dev_private = dev_priv->newpriv;
	kfree(dev_priv);
err_out:
	return ret;
}

void nouveau_lastclose(struct drm_device *dev)
{
	vga_switcheroo_process_delayed_switch();
}

int nouveau_unload(struct drm_device *dev)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;

	nouveau_card_takedown(dev);

	dev->dev_private = dev_priv->newpriv;
	kfree(dev_priv);
	return 0;
}

/* Waits for PGRAPH to go completely idle */
bool nouveau_wait_for_idle(struct drm_device *dev)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	uint32_t mask = ~0;

	if (dev_priv->card_type == NV_40)
		mask &= ~NV40_PGRAPH_STATUS_SYNC_STALL;

	if (!nv_wait(dev, NV04_PGRAPH_STATUS, mask, 0)) {
		NV_ERROR(dev, "PGRAPH idle timed out with status 0x%08x\n",
			 nv_rd32(dev, NV04_PGRAPH_STATUS));
		return false;
	}

	return true;
}

