/*
 *
 * Copyright 2008 (c) Intel Corporation
 *   Jesse Barnes <jbarnes@virtuousgeek.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL TUNGSTEN GRAPHICS AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "drmP.h"
#include "drm.h"
#include "i915_drm.h"
#include "i915_drv.h"

static bool i915_pipe_enabled(struct drm_device *dev, enum pipe pipe)
{
	struct drm_i915_private *dev_priv = dev->dev_private;

	if (pipe == PIPE_A)
		return (I915_READ(DPLL_A) & DPLL_VCO_ENABLE);
	else
		return (I915_READ(DPLL_B) & DPLL_VCO_ENABLE);
}

static void i915_save_palette(struct drm_device *dev, enum pipe pipe)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	unsigned long reg = (pipe == PIPE_A ? PALETTE_A : PALETTE_B);
	u32 *array;
	int i;

	if (!i915_pipe_enabled(dev, pipe))
		return;

	if (pipe == PIPE_A)
		array = dev_priv->save_palette_a;
	else
		array = dev_priv->save_palette_b;

	for(i = 0; i < 256; i++)
		array[i] = I915_READ(reg + (i << 2));
}

static void i915_restore_palette(struct drm_device *dev, enum pipe pipe)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	unsigned long reg = (pipe == PIPE_A ? PALETTE_A : PALETTE_B);
	u32 *array;
	int i;

	if (!i915_pipe_enabled(dev, pipe))
		return;

	if (pipe == PIPE_A)
		array = dev_priv->save_palette_a;
	else
		array = dev_priv->save_palette_b;

	for(i = 0; i < 256; i++)
		I915_WRITE(reg + (i << 2), array[i]);
}

static u8 i915_read_indexed(struct drm_device *dev, u16 index_port, u16 data_port, u8 reg)
{
	struct drm_i915_private *dev_priv = dev->dev_private;

	I915_WRITE8(index_port, reg);
	return I915_READ8(data_port);
}

static u8 i915_read_ar(struct drm_device *dev, u16 st01, u8 reg, u16 palette_enable)
{
	struct drm_i915_private *dev_priv = dev->dev_private;

	I915_READ8(st01);
	I915_WRITE8(VGA_AR_INDEX, palette_enable | reg);
	return I915_READ8(VGA_AR_DATA_READ);
}

static void i915_write_ar(struct drm_device *dev, u16 st01, u8 reg, u8 val, u16 palette_enable)
{
	struct drm_i915_private *dev_priv = dev->dev_private;

	I915_READ8(st01);
	I915_WRITE8(VGA_AR_INDEX, palette_enable | reg);
	I915_WRITE8(VGA_AR_DATA_WRITE, val);
}

static void i915_write_indexed(struct drm_device *dev, u16 index_port, u16 data_port, u8 reg, u8 val)
{
	struct drm_i915_private *dev_priv = dev->dev_private;

	I915_WRITE8(index_port, reg);
	I915_WRITE8(data_port, val);
}

static void i915_save_vga(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	int i;
	u16 cr_index, cr_data, st01;

	/* VGA color palette registers */
	dev_priv->saveDACMASK = I915_READ8(VGA_DACMASK);

	/* MSR bits */
	dev_priv->saveMSR = I915_READ8(VGA_MSR_READ);
	if (dev_priv->saveMSR & VGA_MSR_CGA_MODE) {
		cr_index = VGA_CR_INDEX_CGA;
		cr_data = VGA_CR_DATA_CGA;
		st01 = VGA_ST01_CGA;
	} else {
		cr_index = VGA_CR_INDEX_MDA;
		cr_data = VGA_CR_DATA_MDA;
		st01 = VGA_ST01_MDA;
	}

	/* CRT controller regs */
	i915_write_indexed(dev, cr_index, cr_data, 0x11,
			   i915_read_indexed(dev, cr_index, cr_data, 0x11) &
			   (~0x80));
	for (i = 0; i <= 0x24; i++)
		dev_priv->saveCR[i] =
			i915_read_indexed(dev, cr_index, cr_data, i);
	/* Make sure we don't turn off CR group 0 writes */
	dev_priv->saveCR[0x11] &= ~0x80;

	/* Attribute controller registers */
	I915_READ8(st01);
	dev_priv->saveAR_INDEX = I915_READ8(VGA_AR_INDEX);
	for (i = 0; i <= 0x14; i++)
		dev_priv->saveAR[i] = i915_read_ar(dev, st01, i, 0);
	I915_READ8(st01);
	I915_WRITE8(VGA_AR_INDEX, dev_priv->saveAR_INDEX);
	I915_READ8(st01);

	/* Graphics controller registers */
	for (i = 0; i < 9; i++)
		dev_priv->saveGR[i] =
			i915_read_indexed(dev, VGA_GR_INDEX, VGA_GR_DATA, i);

	dev_priv->saveGR[0x10] =
		i915_read_indexed(dev, VGA_GR_INDEX, VGA_GR_DATA, 0x10);
	dev_priv->saveGR[0x11] =
		i915_read_indexed(dev, VGA_GR_INDEX, VGA_GR_DATA, 0x11);
	dev_priv->saveGR[0x18] =
		i915_read_indexed(dev, VGA_GR_INDEX, VGA_GR_DATA, 0x18);

	/* Sequencer registers */
	for (i = 0; i < 8; i++)
		dev_priv->saveSR[i] =
			i915_read_indexed(dev, VGA_SR_INDEX, VGA_SR_DATA, i);
}

static void i915_restore_vga(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	int i;
	u16 cr_index, cr_data, st01;

	/* MSR bits */
	I915_WRITE8(VGA_MSR_WRITE, dev_priv->saveMSR);
	if (dev_priv->saveMSR & VGA_MSR_CGA_MODE) {
		cr_index = VGA_CR_INDEX_CGA;
		cr_data = VGA_CR_DATA_CGA;
		st01 = VGA_ST01_CGA;
	} else {
		cr_index = VGA_CR_INDEX_MDA;
		cr_data = VGA_CR_DATA_MDA;
		st01 = VGA_ST01_MDA;
	}

	/* Sequencer registers, don't write SR07 */
	for (i = 0; i < 7; i++)
		i915_write_indexed(dev, VGA_SR_INDEX, VGA_SR_DATA, i,
				   dev_priv->saveSR[i]);

	/* CRT controller regs */
	/* Enable CR group 0 writes */
	i915_write_indexed(dev, cr_index, cr_data, 0x11, dev_priv->saveCR[0x11]);
	for (i = 0; i <= 0x24; i++)
		i915_write_indexed(dev, cr_index, cr_data, i, dev_priv->saveCR[i]);

	/* Graphics controller regs */
	for (i = 0; i < 9; i++)
		i915_write_indexed(dev, VGA_GR_INDEX, VGA_GR_DATA, i,
				   dev_priv->saveGR[i]);

	i915_write_indexed(dev, VGA_GR_INDEX, VGA_GR_DATA, 0x10,
			   dev_priv->saveGR[0x10]);
	i915_write_indexed(dev, VGA_GR_INDEX, VGA_GR_DATA, 0x11,
			   dev_priv->saveGR[0x11]);
	i915_write_indexed(dev, VGA_GR_INDEX, VGA_GR_DATA, 0x18,
			   dev_priv->saveGR[0x18]);

	/* Attribute controller registers */
	I915_READ8(st01); /* switch back to index mode */
	for (i = 0; i <= 0x14; i++)
		i915_write_ar(dev, st01, i, dev_priv->saveAR[i], 0);
	I915_READ8(st01); /* switch back to index mode */
	I915_WRITE8(VGA_AR_INDEX, dev_priv->saveAR_INDEX | 0x20);
	I915_READ8(st01);

	/* VGA color palette registers */
	I915_WRITE8(VGA_DACMASK, dev_priv->saveDACMASK);
}

static void i915_save_modeset_reg(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;

	if (drm_core_check_feature(dev, DRIVER_MODESET))
		return;

	/* Pipe & plane A info */
	dev_priv->savePIPEACONF = I915_READ(PIPEACONF);
	dev_priv->savePIPEASRC = I915_READ(PIPEASRC);
	dev_priv->saveFPA0 = I915_READ(FPA0);
	dev_priv->saveFPA1 = I915_READ(FPA1);
	dev_priv->saveDPLL_A = I915_READ(DPLL_A);
	if (IS_I965G(dev))
		dev_priv->saveDPLL_A_MD = I915_READ(DPLL_A_MD);
	dev_priv->saveHTOTAL_A = I915_READ(HTOTAL_A);
	dev_priv->saveHBLANK_A = I915_READ(HBLANK_A);
	dev_priv->saveHSYNC_A = I915_READ(HSYNC_A);
	dev_priv->saveVTOTAL_A = I915_READ(VTOTAL_A);
	dev_priv->saveVBLANK_A = I915_READ(VBLANK_A);
	dev_priv->saveVSYNC_A = I915_READ(VSYNC_A);
	dev_priv->saveBCLRPAT_A = I915_READ(BCLRPAT_A);

	dev_priv->saveDSPACNTR = I915_READ(DSPACNTR);
	dev_priv->saveDSPASTRIDE = I915_READ(DSPASTRIDE);
	dev_priv->saveDSPASIZE = I915_READ(DSPASIZE);
	dev_priv->saveDSPAPOS = I915_READ(DSPAPOS);
	dev_priv->saveDSPAADDR = I915_READ(DSPAADDR);
	if (IS_I965G(dev)) {
		dev_priv->saveDSPASURF = I915_READ(DSPASURF);
		dev_priv->saveDSPATILEOFF = I915_READ(DSPATILEOFF);
	}
	i915_save_palette(dev, PIPE_A);
	dev_priv->savePIPEASTAT = I915_READ(PIPEASTAT);

	/* Pipe & plane B info */
	dev_priv->savePIPEBCONF = I915_READ(PIPEBCONF);
	dev_priv->savePIPEBSRC = I915_READ(PIPEBSRC);
	dev_priv->saveFPB0 = I915_READ(FPB0);
	dev_priv->saveFPB1 = I915_READ(FPB1);
	dev_priv->saveDPLL_B = I915_READ(DPLL_B);
	if (IS_I965G(dev))
		dev_priv->saveDPLL_B_MD = I915_READ(DPLL_B_MD);
	dev_priv->saveHTOTAL_B = I915_READ(HTOTAL_B);
	dev_priv->saveHBLANK_B = I915_READ(HBLANK_B);
	dev_priv->saveHSYNC_B = I915_READ(HSYNC_B);
	dev_priv->saveVTOTAL_B = I915_READ(VTOTAL_B);
	dev_priv->saveVBLANK_B = I915_READ(VBLANK_B);
	dev_priv->saveVSYNC_B = I915_READ(VSYNC_B);
	dev_priv->saveBCLRPAT_A = I915_READ(BCLRPAT_A);

	dev_priv->saveDSPBCNTR = I915_READ(DSPBCNTR);
	dev_priv->saveDSPBSTRIDE = I915_READ(DSPBSTRIDE);
	dev_priv->saveDSPBSIZE = I915_READ(DSPBSIZE);
	dev_priv->saveDSPBPOS = I915_READ(DSPBPOS);
	dev_priv->saveDSPBADDR = I915_READ(DSPBADDR);
	if (IS_I965GM(dev) || IS_GM45(dev)) {
		dev_priv->saveDSPBSURF = I915_READ(DSPBSURF);
		dev_priv->saveDSPBTILEOFF = I915_READ(DSPBTILEOFF);
	}
	i915_save_palette(dev, PIPE_B);
	dev_priv->savePIPEBSTAT = I915_READ(PIPEBSTAT);
	return;
}

static void i915_restore_modeset_reg(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;

	if (drm_core_check_feature(dev, DRIVER_MODESET))
		return;

	/* Pipe & plane A info */
	/* Prime the clock */
	if (dev_priv->saveDPLL_A & DPLL_VCO_ENABLE) {
		I915_WRITE(DPLL_A, dev_priv->saveDPLL_A &
			   ~DPLL_VCO_ENABLE);
		DRM_UDELAY(150);
	}
	I915_WRITE(FPA0, dev_priv->saveFPA0);
	I915_WRITE(FPA1, dev_priv->saveFPA1);
	/* Actually enable it */
	I915_WRITE(DPLL_A, dev_priv->saveDPLL_A);
	DRM_UDELAY(150);
	if (IS_I965G(dev))
		I915_WRITE(DPLL_A_MD, dev_priv->saveDPLL_A_MD);
	DRM_UDELAY(150);

	/* Restore mode */
	I915_WRITE(HTOTAL_A, dev_priv->saveHTOTAL_A);
	I915_WRITE(HBLANK_A, dev_priv->saveHBLANK_A);
	I915_WRITE(HSYNC_A, dev_priv->saveHSYNC_A);
	I915_WRITE(VTOTAL_A, dev_priv->saveVTOTAL_A);
	I915_WRITE(VBLANK_A, dev_priv->saveVBLANK_A);
	I915_WRITE(VSYNC_A, dev_priv->saveVSYNC_A);
	I915_WRITE(BCLRPAT_A, dev_priv->saveBCLRPAT_A);

	/* Restore plane info */
	I915_WRITE(DSPASIZE, dev_priv->saveDSPASIZE);
	I915_WRITE(DSPAPOS, dev_priv->saveDSPAPOS);
	I915_WRITE(PIPEASRC, dev_priv->savePIPEASRC);
	I915_WRITE(DSPAADDR, dev_priv->saveDSPAADDR);
	I915_WRITE(DSPASTRIDE, dev_priv->saveDSPASTRIDE);
	if (IS_I965G(dev)) {
		I915_WRITE(DSPASURF, dev_priv->saveDSPASURF);
		I915_WRITE(DSPATILEOFF, dev_priv->saveDSPATILEOFF);
	}

	I915_WRITE(PIPEACONF, dev_priv->savePIPEACONF);

	i915_restore_palette(dev, PIPE_A);
	/* Enable the plane */
	I915_WRITE(DSPACNTR, dev_priv->saveDSPACNTR);
	I915_WRITE(DSPAADDR, I915_READ(DSPAADDR));

	/* Pipe & plane B info */
	if (dev_priv->saveDPLL_B & DPLL_VCO_ENABLE) {
		I915_WRITE(DPLL_B, dev_priv->saveDPLL_B &
			   ~DPLL_VCO_ENABLE);
		DRM_UDELAY(150);
	}
	I915_WRITE(FPB0, dev_priv->saveFPB0);
	I915_WRITE(FPB1, dev_priv->saveFPB1);
	/* Actually enable it */
	I915_WRITE(DPLL_B, dev_priv->saveDPLL_B);
	DRM_UDELAY(150);
	if (IS_I965G(dev))
		I915_WRITE(DPLL_B_MD, dev_priv->saveDPLL_B_MD);
	DRM_UDELAY(150);

	/* Restore mode */
	I915_WRITE(HTOTAL_B, dev_priv->saveHTOTAL_B);
	I915_WRITE(HBLANK_B, dev_priv->saveHBLANK_B);
	I915_WRITE(HSYNC_B, dev_priv->saveHSYNC_B);
	I915_WRITE(VTOTAL_B, dev_priv->saveVTOTAL_B);
	I915_WRITE(VBLANK_B, dev_priv->saveVBLANK_B);
	I915_WRITE(VSYNC_B, dev_priv->saveVSYNC_B);
	I915_WRITE(BCLRPAT_B, dev_priv->saveBCLRPAT_B);

	/* Restore plane info */
	I915_WRITE(DSPBSIZE, dev_priv->saveDSPBSIZE);
	I915_WRITE(DSPBPOS, dev_priv->saveDSPBPOS);
	I915_WRITE(PIPEBSRC, dev_priv->savePIPEBSRC);
	I915_WRITE(DSPBADDR, dev_priv->saveDSPBADDR);
	I915_WRITE(DSPBSTRIDE, dev_priv->saveDSPBSTRIDE);
	if (IS_I965G(dev)) {
		I915_WRITE(DSPBSURF, dev_priv->saveDSPBSURF);
		I915_WRITE(DSPBTILEOFF, dev_priv->saveDSPBTILEOFF);
	}

	I915_WRITE(PIPEBCONF, dev_priv->savePIPEBCONF);

	i915_restore_palette(dev, PIPE_B);
	/* Enable the plane */
	I915_WRITE(DSPBCNTR, dev_priv->saveDSPBCNTR);
	I915_WRITE(DSPBADDR, I915_READ(DSPBADDR));

	return;
}

void i915_save_display(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;

	/* Display arbitration control */
	dev_priv->saveDSPARB = I915_READ(DSPARB);

	/* This is only meaningful in non-KMS mode */
	/* Don't save them in KMS mode */
	i915_save_modeset_reg(dev);

	/* Cursor state */
	dev_priv->saveCURACNTR = I915_READ(CURACNTR);
	dev_priv->saveCURAPOS = I915_READ(CURAPOS);
	dev_priv->saveCURABASE = I915_READ(CURABASE);
	dev_priv->saveCURBCNTR = I915_READ(CURBCNTR);
	dev_priv->saveCURBPOS = I915_READ(CURBPOS);
	dev_priv->saveCURBBASE = I915_READ(CURBBASE);
	if (!IS_I9XX(dev))
		dev_priv->saveCURSIZE = I915_READ(CURSIZE);

	/* CRT state */
	dev_priv->saveADPA = I915_READ(ADPA);

	/* LVDS state */
	dev_priv->savePP_CONTROL = I915_READ(PP_CONTROL);
	dev_priv->savePFIT_PGM_RATIOS = I915_READ(PFIT_PGM_RATIOS);
	dev_priv->saveBLC_PWM_CTL = I915_READ(BLC_PWM_CTL);
	if (IS_I965G(dev))
		dev_priv->saveBLC_PWM_CTL2 = I915_READ(BLC_PWM_CTL2);
	if (IS_MOBILE(dev) && !IS_I830(dev))
		dev_priv->saveLVDS = I915_READ(LVDS);
	if (!IS_I830(dev) && !IS_845G(dev))
		dev_priv->savePFIT_CONTROL = I915_READ(PFIT_CONTROL);
	dev_priv->savePP_ON_DELAYS = I915_READ(PP_ON_DELAYS);
	dev_priv->savePP_OFF_DELAYS = I915_READ(PP_OFF_DELAYS);
	dev_priv->savePP_DIVISOR = I915_READ(PP_DIVISOR);

	/* Display Port state */
	if (SUPPORTS_INTEGRATED_DP(dev)) {
		dev_priv->saveDP_B = I915_READ(DP_B);
		dev_priv->saveDP_C = I915_READ(DP_C);
		dev_priv->saveDP_D = I915_READ(DP_D);
		dev_priv->savePIPEA_GMCH_DATA_M = I915_READ(PIPEA_GMCH_DATA_M);
		dev_priv->savePIPEB_GMCH_DATA_M = I915_READ(PIPEB_GMCH_DATA_M);
		dev_priv->savePIPEA_GMCH_DATA_N = I915_READ(PIPEA_GMCH_DATA_N);
		dev_priv->savePIPEB_GMCH_DATA_N = I915_READ(PIPEB_GMCH_DATA_N);
		dev_priv->savePIPEA_DP_LINK_M = I915_READ(PIPEA_DP_LINK_M);
		dev_priv->savePIPEB_DP_LINK_M = I915_READ(PIPEB_DP_LINK_M);
		dev_priv->savePIPEA_DP_LINK_N = I915_READ(PIPEA_DP_LINK_N);
		dev_priv->savePIPEB_DP_LINK_N = I915_READ(PIPEB_DP_LINK_N);
	}
	/* FIXME: save TV & SDVO state */

	/* FBC state */
	if (IS_GM45(dev)) {
		dev_priv->saveDPFC_CB_BASE = I915_READ(DPFC_CB_BASE);
	} else {
		dev_priv->saveFBC_CFB_BASE = I915_READ(FBC_CFB_BASE);
		dev_priv->saveFBC_LL_BASE = I915_READ(FBC_LL_BASE);
		dev_priv->saveFBC_CONTROL2 = I915_READ(FBC_CONTROL2);
		dev_priv->saveFBC_CONTROL = I915_READ(FBC_CONTROL);
	}

	/* VGA state */
	dev_priv->saveVGA0 = I915_READ(VGA0);
	dev_priv->saveVGA1 = I915_READ(VGA1);
	dev_priv->saveVGA_PD = I915_READ(VGA_PD);
	dev_priv->saveVGACNTRL = I915_READ(VGACNTRL);

	i915_save_vga(dev);
}

void i915_restore_display(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;

	/* Display arbitration */
	I915_WRITE(DSPARB, dev_priv->saveDSPARB);

	/* Display port ratios (must be done before clock is set) */
	if (SUPPORTS_INTEGRATED_DP(dev)) {
		I915_WRITE(PIPEA_GMCH_DATA_M, dev_priv->savePIPEA_GMCH_DATA_M);
		I915_WRITE(PIPEB_GMCH_DATA_M, dev_priv->savePIPEB_GMCH_DATA_M);
		I915_WRITE(PIPEA_GMCH_DATA_N, dev_priv->savePIPEA_GMCH_DATA_N);
		I915_WRITE(PIPEB_GMCH_DATA_N, dev_priv->savePIPEB_GMCH_DATA_N);
		I915_WRITE(PIPEA_DP_LINK_M, dev_priv->savePIPEA_DP_LINK_M);
		I915_WRITE(PIPEB_DP_LINK_M, dev_priv->savePIPEB_DP_LINK_M);
		I915_WRITE(PIPEA_DP_LINK_N, dev_priv->savePIPEA_DP_LINK_N);
		I915_WRITE(PIPEB_DP_LINK_N, dev_priv->savePIPEB_DP_LINK_N);
	}

	/* This is only meaningful in non-KMS mode */
	/* Don't restore them in KMS mode */
	i915_restore_modeset_reg(dev);

	/* Cursor state */
	I915_WRITE(CURAPOS, dev_priv->saveCURAPOS);
	I915_WRITE(CURACNTR, dev_priv->saveCURACNTR);
	I915_WRITE(CURABASE, dev_priv->saveCURABASE);
	I915_WRITE(CURBPOS, dev_priv->saveCURBPOS);
	I915_WRITE(CURBCNTR, dev_priv->saveCURBCNTR);
	I915_WRITE(CURBBASE, dev_priv->saveCURBBASE);
	if (!IS_I9XX(dev))
		I915_WRITE(CURSIZE, dev_priv->saveCURSIZE);

	/* CRT state */
	I915_WRITE(ADPA, dev_priv->saveADPA);

	/* LVDS state */
	if (IS_I965G(dev))
		I915_WRITE(BLC_PWM_CTL2, dev_priv->saveBLC_PWM_CTL2);
	if (IS_MOBILE(dev) && !IS_I830(dev))
		I915_WRITE(LVDS, dev_priv->saveLVDS);
	if (!IS_I830(dev) && !IS_845G(dev))
		I915_WRITE(PFIT_CONTROL, dev_priv->savePFIT_CONTROL);

	I915_WRITE(PFIT_PGM_RATIOS, dev_priv->savePFIT_PGM_RATIOS);
	I915_WRITE(BLC_PWM_CTL, dev_priv->saveBLC_PWM_CTL);
	I915_WRITE(PP_ON_DELAYS, dev_priv->savePP_ON_DELAYS);
	I915_WRITE(PP_OFF_DELAYS, dev_priv->savePP_OFF_DELAYS);
	I915_WRITE(PP_DIVISOR, dev_priv->savePP_DIVISOR);
	I915_WRITE(PP_CONTROL, dev_priv->savePP_CONTROL);

	/* Display Port state */
	if (SUPPORTS_INTEGRATED_DP(dev)) {
		I915_WRITE(DP_B, dev_priv->saveDP_B);
		I915_WRITE(DP_C, dev_priv->saveDP_C);
		I915_WRITE(DP_D, dev_priv->saveDP_D);
	}
	/* FIXME: restore TV & SDVO state */

	/* FBC info */
	if (IS_GM45(dev)) {
		g4x_disable_fbc(dev);
		I915_WRITE(DPFC_CB_BASE, dev_priv->saveDPFC_CB_BASE);
	} else {
		i8xx_disable_fbc(dev);
		I915_WRITE(FBC_CFB_BASE, dev_priv->saveFBC_CFB_BASE);
		I915_WRITE(FBC_LL_BASE, dev_priv->saveFBC_LL_BASE);
		I915_WRITE(FBC_CONTROL2, dev_priv->saveFBC_CONTROL2);
		I915_WRITE(FBC_CONTROL, dev_priv->saveFBC_CONTROL);
	}

	/* VGA state */
	I915_WRITE(VGACNTRL, dev_priv->saveVGACNTRL);
	I915_WRITE(VGA0, dev_priv->saveVGA0);
	I915_WRITE(VGA1, dev_priv->saveVGA1);
	I915_WRITE(VGA_PD, dev_priv->saveVGA_PD);
	DRM_UDELAY(150);

	i915_restore_vga(dev);
}

int i915_save_state(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	int i;

	pci_read_config_byte(dev->pdev, LBB, &dev_priv->saveLBB);

	/* Render Standby */
	if (IS_I965G(dev) && IS_MOBILE(dev))
		dev_priv->saveRENDERSTANDBY = I915_READ(MCHBAR_RENDER_STANDBY);

	/* Hardware status page */
	dev_priv->saveHWS = I915_READ(HWS_PGA);

	i915_save_display(dev);

	/* Interrupt state */
	dev_priv->saveIER = I915_READ(IER);
	dev_priv->saveIMR = I915_READ(IMR);

	/* Clock gating state */
	dev_priv->saveD_STATE = I915_READ(D_STATE);
	dev_priv->saveDSPCLK_GATE_D = I915_READ(DSPCLK_GATE_D); /* Not sure about this */

	/* Cache mode state */
	dev_priv->saveCACHE_MODE_0 = I915_READ(CACHE_MODE_0);

	/* Memory Arbitration state */
	dev_priv->saveMI_ARB_STATE = I915_READ(MI_ARB_STATE);

	/* Scratch space */
	for (i = 0; i < 16; i++) {
		dev_priv->saveSWF0[i] = I915_READ(SWF00 + (i << 2));
		dev_priv->saveSWF1[i] = I915_READ(SWF10 + (i << 2));
	}
	for (i = 0; i < 3; i++)
		dev_priv->saveSWF2[i] = I915_READ(SWF30 + (i << 2));

	/* Fences */
	if (IS_I965G(dev)) {
		for (i = 0; i < 16; i++)
			dev_priv->saveFENCE[i] = I915_READ64(FENCE_REG_965_0 + (i * 8));
	} else {
		for (i = 0; i < 8; i++)
			dev_priv->saveFENCE[i] = I915_READ(FENCE_REG_830_0 + (i * 4));

		if (IS_I945G(dev) || IS_I945GM(dev) || IS_G33(dev))
			for (i = 0; i < 8; i++)
				dev_priv->saveFENCE[i+8] = I915_READ(FENCE_REG_945_8 + (i * 4));
	}

	return 0;
}

int i915_restore_state(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	int i;

	pci_write_config_byte(dev->pdev, LBB, dev_priv->saveLBB);

	/* Render Standby */
	if (IS_I965G(dev) && IS_MOBILE(dev))
		I915_WRITE(MCHBAR_RENDER_STANDBY, dev_priv->saveRENDERSTANDBY);

	/* Hardware status page */
	I915_WRITE(HWS_PGA, dev_priv->saveHWS);

	/* Fences */
	if (IS_I965G(dev)) {
		for (i = 0; i < 16; i++)
			I915_WRITE64(FENCE_REG_965_0 + (i * 8), dev_priv->saveFENCE[i]);
	} else {
		for (i = 0; i < 8; i++)
			I915_WRITE(FENCE_REG_830_0 + (i * 4), dev_priv->saveFENCE[i]);
		if (IS_I945G(dev) || IS_I945GM(dev) || IS_G33(dev))
			for (i = 0; i < 8; i++)
				I915_WRITE(FENCE_REG_945_8 + (i * 4), dev_priv->saveFENCE[i+8]);
	}

	i915_restore_display(dev);

	/* Interrupt state */
	I915_WRITE (IER, dev_priv->saveIER);
	I915_WRITE (IMR,  dev_priv->saveIMR);

	/* Clock gating state */
	I915_WRITE (D_STATE, dev_priv->saveD_STATE);
	I915_WRITE (DSPCLK_GATE_D, dev_priv->saveDSPCLK_GATE_D);

	/* Cache mode state */
	I915_WRITE (CACHE_MODE_0, dev_priv->saveCACHE_MODE_0 | 0xffff0000);

	/* Memory arbitration state */
	I915_WRITE (MI_ARB_STATE, dev_priv->saveMI_ARB_STATE | 0xffff0000);

	for (i = 0; i < 16; i++) {
		I915_WRITE(SWF00 + (i << 2), dev_priv->saveSWF0[i]);
		I915_WRITE(SWF10 + (i << 2), dev_priv->saveSWF1[i]);
	}
	for (i = 0; i < 3; i++)
		I915_WRITE(SWF30 + (i << 2), dev_priv->saveSWF2[i]);

	return 0;
}

