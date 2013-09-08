#ifndef __NV31_MPEG_H__
#define __NV31_MPEG_H__

#include <engine/mpeg.h>

struct nv31_mpeg_priv {
	struct nouveau_mpeg base;
	atomic_t refcount;
};

struct nv31_mpeg_chan {
	struct nouveau_object base;
};

#endif
