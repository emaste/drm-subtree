/*
 * Copyright 2009 Advanced Micro Devices, Inc.
 * Copyright 2009 Red Hat Inc.
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
 * THE COPYRIGHT HOLDER(S) AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 */

#include "drmP.h"
#include "drm.h"
#include "radeon_drm.h"
#include "radeon.h"

#include "r600d.h"
#include "r600_blit_shaders.h"

#define DI_PT_RECTLIST        0x11
#define DI_INDEX_SIZE_16_BIT  0x0
#define DI_SRC_SEL_AUTO_INDEX 0x2

#define FMT_8                 0x1
#define FMT_5_6_5             0x8
#define FMT_8_8_8_8           0x1a
#define COLOR_8               0x1
#define COLOR_5_6_5           0x8
#define COLOR_8_8_8_8         0x1a

#define RECT_UNIT_H           32
#define RECT_UNIT_W           (RADEON_GPU_PAGE_SIZE / 4 / RECT_UNIT_H)
#define MAX_RECT_DIM          8192

/* emits 21 on rv770+, 23 on r600 */
static void
set_render_target(struct radeon_device *rdev, int format,
		  int w, int h, u64 gpu_addr)
{
	u32 cb_color_info;
	int pitch, slice;

	h = ALIGN(h, 8);
	if (h < 8)
		h = 8;

	cb_color_info = CB_FORMAT(format) |
		CB_SOURCE_FORMAT(CB_SF_EXPORT_NORM) |
		CB_ARRAY_MODE(ARRAY_1D_TILED_THIN1);
	pitch = (w / 8) - 1;
	slice = ((w * h) / 64) - 1;

	radeon_ring_write(rdev, PACKET3(PACKET3_SET_CONTEXT_REG, 1));
	radeon_ring_write(rdev, (CB_COLOR0_BASE - PACKET3_SET_CONTEXT_REG_OFFSET) >> 2);
	radeon_ring_write(rdev, gpu_addr >> 8);

	if (rdev->family > CHIP_R600 && rdev->family < CHIP_RV770) {
		radeon_ring_write(rdev, PACKET3(PACKET3_SURFACE_BASE_UPDATE, 0));
		radeon_ring_write(rdev, 2 << 0);
	}

	radeon_ring_write(rdev, PACKET3(PACKET3_SET_CONTEXT_REG, 1));
	radeon_ring_write(rdev, (CB_COLOR0_SIZE - PACKET3_SET_CONTEXT_REG_OFFSET) >> 2);
	radeon_ring_write(rdev, (pitch << 0) | (slice << 10));

	radeon_ring_write(rdev, PACKET3(PACKET3_SET_CONTEXT_REG, 1));
	radeon_ring_write(rdev, (CB_COLOR0_VIEW - PACKET3_SET_CONTEXT_REG_OFFSET) >> 2);
	radeon_ring_write(rdev, 0);

	radeon_ring_write(rdev, PACKET3(PACKET3_SET_CONTEXT_REG, 1));
	radeon_ring_write(rdev, (CB_COLOR0_INFO - PACKET3_SET_CONTEXT_REG_OFFSET) >> 2);
	radeon_ring_write(rdev, cb_color_info);

	radeon_ring_write(rdev, PACKET3(PACKET3_SET_CONTEXT_REG, 1));
	radeon_ring_write(rdev, (CB_COLOR0_TILE - PACKET3_SET_CONTEXT_REG_OFFSET) >> 2);
	radeon_ring_write(rdev, 0);

	radeon_ring_write(rdev, PACKET3(PACKET3_SET_CONTEXT_REG, 1));
	radeon_ring_write(rdev, (CB_COLOR0_FRAG - PACKET3_SET_CONTEXT_REG_OFFSET) >> 2);
	radeon_ring_write(rdev, 0);

	radeon_ring_write(rdev, PACKET3(PACKET3_SET_CONTEXT_REG, 1));
	radeon_ring_write(rdev, (CB_COLOR0_MASK - PACKET3_SET_CONTEXT_REG_OFFSET) >> 2);
	radeon_ring_write(rdev, 0);
}

/* emits 5dw */
static void
cp_set_surface_sync(struct radeon_device *rdev,
		    u32 sync_type, u32 size,
		    u64 mc_addr)
{
	u32 cp_coher_size;

	if (size == 0xffffffff)
		cp_coher_size = 0xffffffff;
	else
		cp_coher_size = ((size + 255) >> 8);

	radeon_ring_write(rdev, PACKET3(PACKET3_SURFACE_SYNC, 3));
	radeon_ring_write(rdev, sync_type);
	radeon_ring_write(rdev, cp_coher_size);
	radeon_ring_write(rdev, mc_addr >> 8);
	radeon_ring_write(rdev, 10); /* poll interval */
}

/* emits 21dw + 1 surface sync = 26dw */
static void
set_shaders(struct radeon_device *rdev)
{
	u64 gpu_addr;
	u32 sq_pgm_resources;

	/* setup shader regs */
	sq_pgm_resources = (1 << 0);

	/* VS */
	gpu_addr = rdev->r600_blit.shader_gpu_addr + rdev->r600_blit.vs_offset;
	radeon_ring_write(rdev, PACKET3(PACKET3_SET_CONTEXT_REG, 1));
	radeon_ring_write(rdev, (SQ_PGM_START_VS - PACKET3_SET_CONTEXT_REG_OFFSET) >> 2);
	radeon_ring_write(rdev, gpu_addr >> 8);

	radeon_ring_write(rdev, PACKET3(PACKET3_SET_CONTEXT_REG, 1));
	radeon_ring_write(rdev, (SQ_PGM_RESOURCES_VS - PACKET3_SET_CONTEXT_REG_OFFSET) >> 2);
	radeon_ring_write(rdev, sq_pgm_resources);

	radeon_ring_write(rdev, PACKET3(PACKET3_SET_CONTEXT_REG, 1));
	radeon_ring_write(rdev, (SQ_PGM_CF_OFFSET_VS - PACKET3_SET_CONTEXT_REG_OFFSET) >> 2);
	radeon_ring_write(rdev, 0);

	/* PS */
	gpu_addr = rdev->r600_blit.shader_gpu_addr + rdev->r600_blit.ps_offset;
	radeon_ring_write(rdev, PACKET3(PACKET3_SET_CONTEXT_REG, 1));
	radeon_ring_write(rdev, (SQ_PGM_START_PS - PACKET3_SET_CONTEXT_REG_OFFSET) >> 2);
	radeon_ring_write(rdev, gpu_addr >> 8);

	radeon_ring_write(rdev, PACKET3(PACKET3_SET_CONTEXT_REG, 1));
	radeon_ring_write(rdev, (SQ_PGM_RESOURCES_PS - PACKET3_SET_CONTEXT_REG_OFFSET) >> 2);
	radeon_ring_write(rdev, sq_pgm_resources | (1 << 28));

	radeon_ring_write(rdev, PACKET3(PACKET3_SET_CONTEXT_REG, 1));
	radeon_ring_write(rdev, (SQ_PGM_EXPORTS_PS - PACKET3_SET_CONTEXT_REG_OFFSET) >> 2);
	radeon_ring_write(rdev, 2);

	radeon_ring_write(rdev, PACKET3(PACKET3_SET_CONTEXT_REG, 1));
	radeon_ring_write(rdev, (SQ_PGM_CF_OFFSET_PS - PACKET3_SET_CONTEXT_REG_OFFSET) >> 2);
	radeon_ring_write(rdev, 0);

	gpu_addr = rdev->r600_blit.shader_gpu_addr + rdev->r600_blit.vs_offset;
	cp_set_surface_sync(rdev, PACKET3_SH_ACTION_ENA, 512, gpu_addr);
}

/* emits 9 + 1 sync (5) = 14*/
static void
set_vtx_resource(struct radeon_device *rdev, u64 gpu_addr)
{
	u32 sq_vtx_constant_word2;

	sq_vtx_constant_word2 = SQ_VTXC_BASE_ADDR_HI(upper_32_bits(gpu_addr) & 0xff) |
		SQ_VTXC_STRIDE(16);
#ifdef __BIG_ENDIAN
	sq_vtx_constant_word2 |=  SQ_VTXC_ENDIAN_SWAP(SQ_ENDIAN_8IN32);
#endif

	radeon_ring_write(rdev, PACKET3(PACKET3_SET_RESOURCE, 7));
	radeon_ring_write(rdev, 0x460);
	radeon_ring_write(rdev, gpu_addr & 0xffffffff);
	radeon_ring_write(rdev, 48 - 1);
	radeon_ring_write(rdev, sq_vtx_constant_word2);
	radeon_ring_write(rdev, 1 << 0);
	radeon_ring_write(rdev, 0);
	radeon_ring_write(rdev, 0);
	radeon_ring_write(rdev, SQ_TEX_VTX_VALID_BUFFER << 30);

	if ((rdev->family == CHIP_RV610) ||
	    (rdev->family == CHIP_RV620) ||
	    (rdev->family == CHIP_RS780) ||
	    (rdev->family == CHIP_RS880) ||
	    (rdev->family == CHIP_RV710))
		cp_set_surface_sync(rdev,
				    PACKET3_TC_ACTION_ENA, 48, gpu_addr);
	else
		cp_set_surface_sync(rdev,
				    PACKET3_VC_ACTION_ENA, 48, gpu_addr);
}

/* emits 9 */
static void
set_tex_resource(struct radeon_device *rdev,
		 int format, int w, int h, int pitch,
		 u64 gpu_addr)
{
	uint32_t sq_tex_resource_word0, sq_tex_resource_word1, sq_tex_resource_word4;

	if (h < 1)
		h = 1;

	sq_tex_resource_word0 = S_038000_DIM(V_038000_SQ_TEX_DIM_2D) |
		S_038000_TILE_MODE(V_038000_ARRAY_1D_TILED_THIN1);
	sq_tex_resource_word0 |= S_038000_PITCH((pitch >> 3) - 1) |
		S_038000_TEX_WIDTH(w - 1);

	sq_tex_resource_word1 = S_038004_DATA_FORMAT(format);
	sq_tex_resource_word1 |= S_038004_TEX_HEIGHT(h - 1);

	sq_tex_resource_word4 = S_038010_REQUEST_SIZE(1) |
		S_038010_DST_SEL_X(SQ_SEL_X) |
		S_038010_DST_SEL_Y(SQ_SEL_Y) |
		S_038010_DST_SEL_Z(SQ_SEL_Z) |
		S_038010_DST_SEL_W(SQ_SEL_W);

	radeon_ring_write(rdev, PACKET3(PACKET3_SET_RESOURCE, 7));
	radeon_ring_write(rdev, 0);
	radeon_ring_write(rdev, sq_tex_resource_word0);
	radeon_ring_write(rdev, sq_tex_resource_word1);
	radeon_ring_write(rdev, gpu_addr >> 8);
	radeon_ring_write(rdev, gpu_addr >> 8);
	radeon_ring_write(rdev, sq_tex_resource_word4);
	radeon_ring_write(rdev, 0);
	radeon_ring_write(rdev, SQ_TEX_VTX_VALID_TEXTURE << 30);
}

/* emits 12 */
static void
set_scissors(struct radeon_device *rdev, int x1, int y1,
	     int x2, int y2)
{
	radeon_ring_write(rdev, PACKET3(PACKET3_SET_CONTEXT_REG, 2));
	radeon_ring_write(rdev, (PA_SC_SCREEN_SCISSOR_TL - PACKET3_SET_CONTEXT_REG_OFFSET) >> 2);
	radeon_ring_write(rdev, (x1 << 0) | (y1 << 16));
	radeon_ring_write(rdev, (x2 << 0) | (y2 << 16));

	radeon_ring_write(rdev, PACKET3(PACKET3_SET_CONTEXT_REG, 2));
	radeon_ring_write(rdev, (PA_SC_GENERIC_SCISSOR_TL - PACKET3_SET_CONTEXT_REG_OFFSET) >> 2);
	radeon_ring_write(rdev, (x1 << 0) | (y1 << 16) | (1 << 31));
	radeon_ring_write(rdev, (x2 << 0) | (y2 << 16));

	radeon_ring_write(rdev, PACKET3(PACKET3_SET_CONTEXT_REG, 2));
	radeon_ring_write(rdev, (PA_SC_WINDOW_SCISSOR_TL - PACKET3_SET_CONTEXT_REG_OFFSET) >> 2);
	radeon_ring_write(rdev, (x1 << 0) | (y1 << 16) | (1 << 31));
	radeon_ring_write(rdev, (x2 << 0) | (y2 << 16));
}

/* emits 10 */
static void
draw_auto(struct radeon_device *rdev)
{
	radeon_ring_write(rdev, PACKET3(PACKET3_SET_CONFIG_REG, 1));
	radeon_ring_write(rdev, (VGT_PRIMITIVE_TYPE - PACKET3_SET_CONFIG_REG_OFFSET) >> 2);
	radeon_ring_write(rdev, DI_PT_RECTLIST);

	radeon_ring_write(rdev, PACKET3(PACKET3_INDEX_TYPE, 0));
	radeon_ring_write(rdev,
#ifdef __BIG_ENDIAN
			  (2 << 2) |
#endif
			  DI_INDEX_SIZE_16_BIT);

	radeon_ring_write(rdev, PACKET3(PACKET3_NUM_INSTANCES, 0));
	radeon_ring_write(rdev, 1);

	radeon_ring_write(rdev, PACKET3(PACKET3_DRAW_INDEX_AUTO, 1));
	radeon_ring_write(rdev, 3);
	radeon_ring_write(rdev, DI_SRC_SEL_AUTO_INDEX);

}

/* emits 14 */
static void
set_default_state(struct radeon_device *rdev)
{
	u32 sq_config, sq_gpr_resource_mgmt_1, sq_gpr_resource_mgmt_2;
	u32 sq_thread_resource_mgmt, sq_stack_resource_mgmt_1, sq_stack_resource_mgmt_2;
	int num_ps_gprs, num_vs_gprs, num_temp_gprs, num_gs_gprs, num_es_gprs;
	int num_ps_threads, num_vs_threads, num_gs_threads, num_es_threads;
	int num_ps_stack_entries, num_vs_stack_entries, num_gs_stack_entries, num_es_stack_entries;
	u64 gpu_addr;
	int dwords;

	switch (rdev->family) {
	case CHIP_R600:
		num_ps_gprs = 192;
		num_vs_gprs = 56;
		num_temp_gprs = 4;
		num_gs_gprs = 0;
		num_es_gprs = 0;
		num_ps_threads = 136;
		num_vs_threads = 48;
		num_gs_threads = 4;
		num_es_threads = 4;
		num_ps_stack_entries = 128;
		num_vs_stack_entries = 128;
		num_gs_stack_entries = 0;
		num_es_stack_entries = 0;
		break;
	case CHIP_RV630:
	case CHIP_RV635:
		num_ps_gprs = 84;
		num_vs_gprs = 36;
		num_temp_gprs = 4;
		num_gs_gprs = 0;
		num_es_gprs = 0;
		num_ps_threads = 144;
		num_vs_threads = 40;
		num_gs_threads = 4;
		num_es_threads = 4;
		num_ps_stack_entries = 40;
		num_vs_stack_entries = 40;
		num_gs_stack_entries = 32;
		num_es_stack_entries = 16;
		break;
	case CHIP_RV610:
	case CHIP_RV620:
	case CHIP_RS780:
	case CHIP_RS880:
	default:
		num_ps_gprs = 84;
		num_vs_gprs = 36;
		num_temp_gprs = 4;
		num_gs_gprs = 0;
		num_es_gprs = 0;
		num_ps_threads = 136;
		num_vs_threads = 48;
		num_gs_threads = 4;
		num_es_threads = 4;
		num_ps_stack_entries = 40;
		num_vs_stack_entries = 40;
		num_gs_stack_entries = 32;
		num_es_stack_entries = 16;
		break;
	case CHIP_RV670:
		num_ps_gprs = 144;
		num_vs_gprs = 40;
		num_temp_gprs = 4;
		num_gs_gprs = 0;
		num_es_gprs = 0;
		num_ps_threads = 136;
		num_vs_threads = 48;
		num_gs_threads = 4;
		num_es_threads = 4;
		num_ps_stack_entries = 40;
		num_vs_stack_entries = 40;
		num_gs_stack_entries = 32;
		num_es_stack_entries = 16;
		break;
	case CHIP_RV770:
		num_ps_gprs = 192;
		num_vs_gprs = 56;
		num_temp_gprs = 4;
		num_gs_gprs = 0;
		num_es_gprs = 0;
		num_ps_threads = 188;
		num_vs_threads = 60;
		num_gs_threads = 0;
		num_es_threads = 0;
		num_ps_stack_entries = 256;
		num_vs_stack_entries = 256;
		num_gs_stack_entries = 0;
		num_es_stack_entries = 0;
		break;
	case CHIP_RV730:
	case CHIP_RV740:
		num_ps_gprs = 84;
		num_vs_gprs = 36;
		num_temp_gprs = 4;
		num_gs_gprs = 0;
		num_es_gprs = 0;
		num_ps_threads = 188;
		num_vs_threads = 60;
		num_gs_threads = 0;
		num_es_threads = 0;
		num_ps_stack_entries = 128;
		num_vs_stack_entries = 128;
		num_gs_stack_entries = 0;
		num_es_stack_entries = 0;
		break;
	case CHIP_RV710:
		num_ps_gprs = 192;
		num_vs_gprs = 56;
		num_temp_gprs = 4;
		num_gs_gprs = 0;
		num_es_gprs = 0;
		num_ps_threads = 144;
		num_vs_threads = 48;
		num_gs_threads = 0;
		num_es_threads = 0;
		num_ps_stack_entries = 128;
		num_vs_stack_entries = 128;
		num_gs_stack_entries = 0;
		num_es_stack_entries = 0;
		break;
	}

	if ((rdev->family == CHIP_RV610) ||
	    (rdev->family == CHIP_RV620) ||
	    (rdev->family == CHIP_RS780) ||
	    (rdev->family == CHIP_RS880) ||
	    (rdev->family == CHIP_RV710))
		sq_config = 0;
	else
		sq_config = VC_ENABLE;

	sq_config |= (DX9_CONSTS |
		      ALU_INST_PREFER_VECTOR |
		      PS_PRIO(0) |
		      VS_PRIO(1) |
		      GS_PRIO(2) |
		      ES_PRIO(3));

	sq_gpr_resource_mgmt_1 = (NUM_PS_GPRS(num_ps_gprs) |
				  NUM_VS_GPRS(num_vs_gprs) |
				  NUM_CLAUSE_TEMP_GPRS(num_temp_gprs));
	sq_gpr_resource_mgmt_2 = (NUM_GS_GPRS(num_gs_gprs) |
				  NUM_ES_GPRS(num_es_gprs));
	sq_thread_resource_mgmt = (NUM_PS_THREADS(num_ps_threads) |
				   NUM_VS_THREADS(num_vs_threads) |
				   NUM_GS_THREADS(num_gs_threads) |
				   NUM_ES_THREADS(num_es_threads));
	sq_stack_resource_mgmt_1 = (NUM_PS_STACK_ENTRIES(num_ps_stack_entries) |
				    NUM_VS_STACK_ENTRIES(num_vs_stack_entries));
	sq_stack_resource_mgmt_2 = (NUM_GS_STACK_ENTRIES(num_gs_stack_entries) |
				    NUM_ES_STACK_ENTRIES(num_es_stack_entries));

	/* emit an IB pointing at default state */
	dwords = ALIGN(rdev->r600_blit.state_len, 0x10);
	gpu_addr = rdev->r600_blit.shader_gpu_addr + rdev->r600_blit.state_offset;
	radeon_ring_write(rdev, PACKET3(PACKET3_INDIRECT_BUFFER, 2));
	radeon_ring_write(rdev,
#ifdef __BIG_ENDIAN
			  (2 << 0) |
#endif
			  (gpu_addr & 0xFFFFFFFC));
	radeon_ring_write(rdev, upper_32_bits(gpu_addr) & 0xFF);
	radeon_ring_write(rdev, dwords);

	/* SQ config */
	radeon_ring_write(rdev, PACKET3(PACKET3_SET_CONFIG_REG, 6));
	radeon_ring_write(rdev, (SQ_CONFIG - PACKET3_SET_CONFIG_REG_OFFSET) >> 2);
	radeon_ring_write(rdev, sq_config);
	radeon_ring_write(rdev, sq_gpr_resource_mgmt_1);
	radeon_ring_write(rdev, sq_gpr_resource_mgmt_2);
	radeon_ring_write(rdev, sq_thread_resource_mgmt);
	radeon_ring_write(rdev, sq_stack_resource_mgmt_1);
	radeon_ring_write(rdev, sq_stack_resource_mgmt_2);
}

static uint32_t i2f(uint32_t input)
{
	u32 result, i, exponent, fraction;

	if ((input & 0x3fff) == 0)
		result = 0; /* 0 is a special case */
	else {
		exponent = 140; /* exponent biased by 127; */
		fraction = (input & 0x3fff) << 10; /* cheat and only
						      handle numbers below 2^^15 */
		for (i = 0; i < 14; i++) {
			if (fraction & 0x800000)
				break;
			else {
				fraction = fraction << 1; /* keep
							     shifting left until top bit = 1 */
				exponent = exponent - 1;
			}
		}
		result = exponent << 23 | (fraction & 0x7fffff); /* mask
								    off top bit; assumed 1 */
	}
	return result;
}

int r600_blit_init(struct radeon_device *rdev)
{
	u32 obj_size;
	int i, r, dwords;
	void *ptr;
	u32 packet2s[16];
	int num_packet2s = 0;

	/* pin copy shader into vram if already initialized */
	if (rdev->r600_blit.shader_obj)
		goto done;

	mutex_init(&rdev->r600_blit.mutex);
	rdev->r600_blit.state_offset = 0;

	if (rdev->family >= CHIP_RV770)
		rdev->r600_blit.state_len = r7xx_default_size;
	else
		rdev->r600_blit.state_len = r6xx_default_size;

	dwords = rdev->r600_blit.state_len;
	while (dwords & 0xf) {
		packet2s[num_packet2s++] = cpu_to_le32(PACKET2(0));
		dwords++;
	}

	obj_size = dwords * 4;
	obj_size = ALIGN(obj_size, 256);

	rdev->r600_blit.vs_offset = obj_size;
	obj_size += r6xx_vs_size * 4;
	obj_size = ALIGN(obj_size, 256);

	rdev->r600_blit.ps_offset = obj_size;
	obj_size += r6xx_ps_size * 4;
	obj_size = ALIGN(obj_size, 256);

	r = radeon_bo_create(rdev, obj_size, PAGE_SIZE, true, RADEON_GEM_DOMAIN_VRAM,
				&rdev->r600_blit.shader_obj);
	if (r) {
		DRM_ERROR("r600 failed to allocate shader\n");
		return r;
	}

	DRM_DEBUG("r6xx blit allocated bo %08x vs %08x ps %08x\n",
		  obj_size,
		  rdev->r600_blit.vs_offset, rdev->r600_blit.ps_offset);

	r = radeon_bo_reserve(rdev->r600_blit.shader_obj, false);
	if (unlikely(r != 0))
		return r;
	r = radeon_bo_kmap(rdev->r600_blit.shader_obj, &ptr);
	if (r) {
		DRM_ERROR("failed to map blit object %d\n", r);
		return r;
	}
	if (rdev->family >= CHIP_RV770)
		memcpy_toio(ptr + rdev->r600_blit.state_offset,
			    r7xx_default_state, rdev->r600_blit.state_len * 4);
	else
		memcpy_toio(ptr + rdev->r600_blit.state_offset,
			    r6xx_default_state, rdev->r600_blit.state_len * 4);
	if (num_packet2s)
		memcpy_toio(ptr + rdev->r600_blit.state_offset + (rdev->r600_blit.state_len * 4),
			    packet2s, num_packet2s * 4);
	for (i = 0; i < r6xx_vs_size; i++)
		*(u32 *)((unsigned long)ptr + rdev->r600_blit.vs_offset + i * 4) = cpu_to_le32(r6xx_vs[i]);
	for (i = 0; i < r6xx_ps_size; i++)
		*(u32 *)((unsigned long)ptr + rdev->r600_blit.ps_offset + i * 4) = cpu_to_le32(r6xx_ps[i]);
	radeon_bo_kunmap(rdev->r600_blit.shader_obj);
	radeon_bo_unreserve(rdev->r600_blit.shader_obj);

done:
	r = radeon_bo_reserve(rdev->r600_blit.shader_obj, false);
	if (unlikely(r != 0))
		return r;
	r = radeon_bo_pin(rdev->r600_blit.shader_obj, RADEON_GEM_DOMAIN_VRAM,
			  &rdev->r600_blit.shader_gpu_addr);
	radeon_bo_unreserve(rdev->r600_blit.shader_obj);
	if (r) {
		dev_err(rdev->dev, "(%d) pin blit object failed\n", r);
		return r;
	}
	radeon_ttm_set_active_vram_size(rdev, rdev->mc.real_vram_size);
	return 0;
}

void r600_blit_fini(struct radeon_device *rdev)
{
	int r;

	radeon_ttm_set_active_vram_size(rdev, rdev->mc.visible_vram_size);
	if (rdev->r600_blit.shader_obj == NULL)
		return;
	/* If we can't reserve the bo, unref should be enough to destroy
	 * it when it becomes idle.
	 */
	r = radeon_bo_reserve(rdev->r600_blit.shader_obj, false);
	if (!r) {
		radeon_bo_unpin(rdev->r600_blit.shader_obj);
		radeon_bo_unreserve(rdev->r600_blit.shader_obj);
	}
	radeon_bo_unref(&rdev->r600_blit.shader_obj);
}

static int r600_vb_ib_get(struct radeon_device *rdev)
{
	int r;
	r = radeon_ib_get(rdev, &rdev->r600_blit.vb_ib);
	if (r) {
		DRM_ERROR("failed to get IB for vertex buffer\n");
		return r;
	}

	rdev->r600_blit.vb_total = 64*1024;
	rdev->r600_blit.vb_used = 0;
	return 0;
}

static void r600_vb_ib_put(struct radeon_device *rdev)
{
	radeon_fence_emit(rdev, rdev->r600_blit.vb_ib->fence);
	radeon_ib_free(rdev, &rdev->r600_blit.vb_ib);
}

/* FIXME: the function is very similar to evergreen_blit_create_rect, except
   that it different predefined constants; consider commonizing */
static unsigned r600_blit_create_rect(unsigned num_pages, int *width, int *height)
{
	unsigned max_pages;
	unsigned pages = num_pages;
	int w, h;

	if (num_pages == 0) {
		/* not supposed to be called with no pages, but just in case */
		h = 0;
		w = 0;
		pages = 0;
		WARN_ON(1);
	} else {
		int rect_order = 2;
		h = RECT_UNIT_H;
		while (num_pages / rect_order) {
			h *= 2;
			rect_order *= 4;
			if (h >= MAX_RECT_DIM) {
				h = MAX_RECT_DIM;
				break;
			}
		}
		max_pages = (MAX_RECT_DIM * h) / (RECT_UNIT_W * RECT_UNIT_H);
		if (pages > max_pages)
			pages = max_pages;
		w = (pages * RECT_UNIT_W * RECT_UNIT_H) / h;
		w = (w / RECT_UNIT_W) * RECT_UNIT_W;
		pages = (w * h) / (RECT_UNIT_W * RECT_UNIT_H);
		BUG_ON(pages == 0);
	}


	DRM_DEBUG("blit_rectangle: h=%d, w=%d, pages=%d\n", h, w, pages);

	/* return width and height only of the caller wants it */
	if (height)
		*height = h;
	if (width)
		*width = w;

	return pages;
}


int r600_blit_prepare_copy(struct radeon_device *rdev, unsigned num_pages)
{
	int r;
	int ring_size;
	/* loops of emits 64 + fence emit possible */
	int dwords_per_loop = 76, num_loops = 0;

	r = r600_vb_ib_get(rdev);
	if (r)
		return r;

	/* set_render_target emits 2 extra dwords on rv6xx */
	if (rdev->family > CHIP_R600 && rdev->family < CHIP_RV770)
		dwords_per_loop += 2;

	/* num loops */
	while (num_pages) {
		num_pages -= r600_blit_create_rect(num_pages, NULL, NULL);
		num_loops++;
	}

	/* calculate number of loops correctly */
	ring_size = num_loops * dwords_per_loop;
	/* set default  + shaders */
	ring_size += 40; /* shaders + def state */
	ring_size += 10; /* fence emit for VB IB */
	ring_size += 5; /* done copy */
	ring_size += 10; /* fence emit for done copy */
	r = radeon_ring_lock(rdev, ring_size);
	if (r)
		return r;

	set_default_state(rdev); /* 14 */
	set_shaders(rdev); /* 26 */
	return 0;
}

void r600_blit_done_copy(struct radeon_device *rdev, struct radeon_fence *fence)
{
	int r;

	if (rdev->r600_blit.vb_ib)
		r600_vb_ib_put(rdev);

	if (fence)
		r = radeon_fence_emit(rdev, fence);

	radeon_ring_unlock_commit(rdev);
}

void r600_kms_blit_copy(struct radeon_device *rdev,
			u64 src_gpu_addr, u64 dst_gpu_addr,
			unsigned num_pages)
{
	u64 vb_gpu_addr;
	u32 *vb;

	DRM_DEBUG("emitting copy %16llx %16llx %d %d\n", src_gpu_addr, dst_gpu_addr,
		  num_pages, rdev->r600_blit.vb_used);
	vb = (u32 *)(rdev->r600_blit.vb_ib->ptr + rdev->r600_blit.vb_used);

	while (num_pages) {
		int w, h;
		unsigned size_in_bytes;
		unsigned pages_per_loop = r600_blit_create_rect(num_pages, &w, &h);

		size_in_bytes = pages_per_loop * RADEON_GPU_PAGE_SIZE;
		DRM_DEBUG("rectangle w=%d h=%d\n", w, h);

		if ((rdev->r600_blit.vb_used + 48) > rdev->r600_blit.vb_total) {
			WARN_ON(1);
		}

		vb[0] = 0;
		vb[1] = 0;
		vb[2] = 0;
		vb[3] = 0;

		vb[4] = 0;
		vb[5] = i2f(h);
		vb[6] = 0;
		vb[7] = i2f(h);

		vb[8] = i2f(w);
		vb[9] = i2f(h);
		vb[10] = i2f(w);
		vb[11] = i2f(h);

		/* src 9 */
		set_tex_resource(rdev, FMT_8_8_8_8, w, h, w, src_gpu_addr);

		/* 5 */
		cp_set_surface_sync(rdev,
				    PACKET3_TC_ACTION_ENA, size_in_bytes, src_gpu_addr);

		/* dst 23 */
		set_render_target(rdev, COLOR_8_8_8_8, w, h, dst_gpu_addr);

		/* scissors 12  */
		set_scissors(rdev, 0, 0, w, h);

		/* Vertex buffer setup 14 */
		vb_gpu_addr = rdev->r600_blit.vb_ib->gpu_addr + rdev->r600_blit.vb_used;
		set_vtx_resource(rdev, vb_gpu_addr);

		/* draw 10 */
		draw_auto(rdev);

		/* 5 */
		cp_set_surface_sync(rdev,
				    PACKET3_CB_ACTION_ENA | PACKET3_CB0_DEST_BASE_ENA,
				    size_in_bytes, dst_gpu_addr);

		/* 78 ring dwords per loop */
		vb += 12;
		rdev->r600_blit.vb_used += 4*12;
		src_gpu_addr += size_in_bytes;
		dst_gpu_addr += size_in_bytes;
		num_pages -= pages_per_loop;
	}
}
