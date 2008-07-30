#
# Makefile for the drm device driver.  This driver provides support for the
# Direct Rendering Infrastructure (DRI) in XFree86 4.1.0 and higher.

ccflags-y := -Iinclude/drm

drm-y       :=	drm_auth.o drm_bufs.o drm_cache.o \
		drm_context.o drm_dma.o drm_drawable.o \
		drm_drv.o drm_fops.o drm_gem.o drm_ioctl.o drm_irq.o \
		drm_lock.o drm_memory.o drm_proc.o drm_stub.o drm_vm.o \
		drm_agpsupport.o drm_scatter.o ati_pcigart.o drm_pci.o \
		drm_sysfs.o drm_hashtab.o drm_sman.o drm_mm.o

drm-$(CONFIG_COMPAT) += drm_ioc32.o

obj-$(CONFIG_DRM)	+= drm.o
obj-$(CONFIG_DRM_TDFX)	+= tdfx/
obj-$(CONFIG_DRM_R128)	+= r128/
obj-$(CONFIG_DRM_RADEON)+= radeon/
obj-$(CONFIG_DRM_MGA)	+= mga/
obj-$(CONFIG_DRM_I810)	+= i810/
obj-$(CONFIG_DRM_I830)	+= i830/
obj-$(CONFIG_DRM_I915)  += i915/
obj-$(CONFIG_DRM_SIS)   += sis/
obj-$(CONFIG_DRM_SAVAGE)+= savage/
obj-$(CONFIG_DRM_VIA)	+=via/

