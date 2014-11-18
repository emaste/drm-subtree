/*
 * Copyright (c) 2014 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __MDP5_CFG_H__
#define __MDP5_CFG_H__

#include "msm_drv.h"

/*
 * mdp5_cfg
 *
 * This module configures the dynamic offsets used by mdp5.xml.h
 * (initialized in mdp5_cfg.c)
 */
extern const struct mdp5_cfg_hw *mdp5_cfg;

#define MAX_BASES		8
#define MAX_SMP_BLOCKS		44
#define MAX_CLIENTS		32

typedef DECLARE_BITMAP(mdp5_smp_state_t, MAX_SMP_BLOCKS);

#define MDP5_SUB_BLOCK_DEFINITION \
	int count; \
	uint32_t base[MAX_BASES]

struct mdp5_sub_block {
	MDP5_SUB_BLOCK_DEFINITION;
};

struct mdp5_lm_block {
	MDP5_SUB_BLOCK_DEFINITION;
	uint32_t nb_stages;		/* number of stages per blender */
};

struct mdp5_smp_block {
	int mmb_count;			/* number of SMP MMBs */
	int mmb_size;			/* MMB: size in bytes */
	mdp5_smp_state_t reserved_state;/* SMP MMBs statically allocated */
	int reserved[MAX_CLIENTS];	/* # of MMBs allocated per client */
};

struct mdp5_cfg_hw {
	char  *name;

	struct mdp5_smp_block smp;
	struct mdp5_sub_block ctl;
	struct mdp5_sub_block pipe_vig;
	struct mdp5_sub_block pipe_rgb;
	struct mdp5_sub_block pipe_dma;
	struct mdp5_lm_block  lm;
	struct mdp5_sub_block dspp;
	struct mdp5_sub_block ad;
	struct mdp5_sub_block intf;

	uint32_t max_clk;
};

/* platform config data (ie. from DT, or pdata) */
struct mdp5_cfg_platform {
	struct iommu_domain *iommu;
};

struct mdp5_cfg {
	const struct mdp5_cfg_hw *hw;
	struct mdp5_cfg_platform platform;
};

struct mdp5_kms;

const struct mdp5_cfg_hw *mdp5_cfg_get_hw_config(void *cfg_hnd);
struct mdp5_cfg *mdp5_cfg_get_config(void *cfg_hnd);
int mdp5_cfg_get_hw_rev(void *cfg_hnd);

void *mdp5_cfg_init(struct mdp5_kms *mdp5_kms, uint32_t major, uint32_t minor);
void mdp5_cfg_destroy(void *cfg_hnd);

#endif /* __MDP5_CFG_H__ */
