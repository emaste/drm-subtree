/*
 * Copyright 2012 Red Hat Inc.
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

#include <subdev/device.h>

int
nv50_identify(struct nouveau_device *device)
{
	switch (device->chipset) {
	case 0x50:
		break;
	case 0x84:
		break;
	case 0x86:
		break;
	case 0x92:
		break;
	case 0x94:
		break;
	case 0x96:
		break;
	case 0x98:
		break;
	case 0xa0:
		break;
	case 0xaa:
		break;
	case 0xac:
		break;
	case 0xa3:
		break;
	case 0xa5:
		break;
	case 0xa8:
		break;
	case 0xaf:
		break;
	default:
		nv_fatal(device, "unknown Tesla chipset\n");
		return -EINVAL;
	}

	return 0;
}
