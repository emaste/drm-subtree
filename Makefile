#
# Makefile for the drm device driver.  This driver provides support for the
# Direct Rendering Infrastructure (DRI) in XFree86 4.1.0 and higher.

ccflags-y := -Iinclude/drm

drm-y       :=	drm_auth.o drm_bufs.o drm_cache.o \
		drm_context.o drm_dma.o \
		drm_fops.o drm_gem.o drm_ioctl.o drm_irq.o \
		drm_lock.o drm_memory.o drm_drv.o drm_vm.o \
		drm_agpsupport.o drm_scatter.o drm_pci.o \
		drm_platform.o drm_sysfs.o drm_hashtab.o drm_mm.o \
		drm_crtc.o drm_modes.o drm_edid.o \
		drm_info.o drm_debugfs.o drm_encoder_slave.o \
		drm_trace_points.o drm_global.o drm_prime.o \
		drm_rect.o drm_vma_manager.o drm_flip_work.o \
		drm_modeset_lock.o drm_atomic.o

drm-$(CONFIG_COMPAT) += drm_ioc32.o
drm-$(CONFIG_DRM_GEM_CMA_HELPER) += drm_gem_cma_helper.o
drm-$(CONFIG_PCI) += ati_pcigart.o
drm-$(CONFIG_DRM_PANEL) += drm_panel.o
drm-$(CONFIG_OF) += drm_of.o

drm_kms_helper-y := drm_crtc_helper.o drm_dp_helper.o drm_probe_helper.o \
		drm_plane_helper.o drm_dp_mst_topology.o drm_atomic_helper.o
drm_kms_helper-$(CONFIG_DRM_LOAD_EDID_FIRMWARE) += drm_edid_load.o
drm_kms_helper-$(CONFIG_DRM_KMS_FB_HELPER) += drm_fb_helper.o
drm_kms_helper-$(CONFIG_DRM_KMS_CMA_HELPER) += drm_fb_cma_helper.o

obj-$(CONFIG_DRM_KMS_HELPER) += drm_kms_helper.o

CFLAGS_drm_trace_points.o := -I$(src)

obj-$(CONFIG_DRM)	+= drm.o
obj-$(CONFIG_DRM_MIPI_DSI) += drm_mipi_dsi.o
obj-$(CONFIG_DRM_TTM)	+= ttm/
obj-$(CONFIG_DRM_TDFX)	+= tdfx/
obj-$(CONFIG_DRM_R128)	+= r128/
obj-$(CONFIG_DRM_RADEON)+= radeon/
obj-$(CONFIG_DRM_MGA)	+= mga/
obj-$(CONFIG_DRM_I810)	+= i810/
obj-$(CONFIG_DRM_I915)  += i915/
obj-$(CONFIG_DRM_MGAG200) += mgag200/
obj-$(CONFIG_DRM_CIRRUS_QEMU) += cirrus/
obj-$(CONFIG_DRM_SIS)   += sis/
obj-$(CONFIG_DRM_SAVAGE)+= savage/
obj-$(CONFIG_DRM_VMWGFX)+= vmwgfx/
obj-$(CONFIG_DRM_VIA)	+=via/
obj-$(CONFIG_DRM_NOUVEAU) +=nouveau/
obj-$(CONFIG_DRM_EXYNOS) +=exynos/
obj-$(CONFIG_DRM_ROCKCHIP) +=rockchip/
obj-$(CONFIG_DRM_GMA500) += gma500/
obj-$(CONFIG_DRM_UDL) += udl/
obj-$(CONFIG_DRM_AST) += ast/
obj-$(CONFIG_DRM_ARMADA) += armada/
obj-$(CONFIG_DRM_RCAR_DU) += rcar-du/
obj-$(CONFIG_DRM_SHMOBILE) +=shmobile/
obj-$(CONFIG_DRM_OMAP)	+= omapdrm/
obj-$(CONFIG_DRM_TILCDC)	+= tilcdc/
obj-$(CONFIG_DRM_QXL) += qxl/
obj-$(CONFIG_DRM_BOCHS) += bochs/
obj-$(CONFIG_DRM_MSM) += msm/
obj-$(CONFIG_DRM_TEGRA) += tegra/
obj-$(CONFIG_DRM_STI) += sti/
obj-$(CONFIG_DRM_IMX) += imx/
obj-y			+= i2c/
obj-y			+= panel/
obj-y			+= bridge/
obj-$(CONFIG_HSA_AMD) += amd/amdkfd/
