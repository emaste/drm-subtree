/*
 * Copyright 2010 Red Hat Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors: Ben Skeggs
 */

#ifndef __NOUVEAU_MM_H__
#define __NOUVEAU_MM_H__

struct nouveau_mm_node {
	struct list_head nl_entry;
	struct list_head fl_entry;
	struct list_head rl_entry;

	u8  type;
	u32 offset;
	u32 length;
};

struct nouveau_mm {
	struct list_head nodes;
	struct list_head free;

	struct mutex mutex;

	u32 block_size;
	int heap_nodes;
};

int  nouveau_mm_init(struct nouveau_mm *, u32 offset, u32 length, u32 block);
int  nouveau_mm_fini(struct nouveau_mm *);
int  nouveau_mm_pre(struct nouveau_mm *);
int  nouveau_mm_get(struct nouveau_mm *, int type, u32 size, u32 size_nc,
		    u32 align, struct nouveau_mm_node **);
void nouveau_mm_put(struct nouveau_mm *, struct nouveau_mm_node *);

#endif
