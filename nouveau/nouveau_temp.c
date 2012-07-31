/*
 * Copyright 2010 PathScale inc.
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
 * Authors: Martin Peres
 */

#include <linux/module.h>

#include "drmP.h"

#include "nouveau_drm.h"
#include "nouveau_pm.h"

#include <subdev/i2c.h>

static void
nouveau_temp_vbios_parse(struct drm_device *dev, u8 *temp)
{
	struct nouveau_drm *drm = nouveau_drm(dev);
	struct nouveau_pm *pm = nouveau_pm(dev);
	struct nouveau_pm_temp_sensor_constants *sensor = &pm->sensor_constants;
	struct nouveau_pm_threshold_temp *temps = &pm->threshold_temp;
	int i, headerlen, recordlen, entries;

	if (!temp) {
		NV_DEBUG(drm, "temperature table pointer invalid\n");
		return;
	}

	/* Set the default sensor's contants */
	sensor->offset_constant = 0;
	sensor->offset_mult = 0;
	sensor->offset_div = 1;
	sensor->slope_mult = 1;
	sensor->slope_div = 1;

	/* Set the default temperature thresholds */
	temps->critical = 110;
	temps->down_clock = 100;
	temps->fan_boost = 90;

	/* Set the default range for the pwm fan */
	pm->fan.min_duty = 30;
	pm->fan.max_duty = 100;

	/* Set the known default values to setup the temperature sensor */
	if (nv_device(drm->device)->card_type >= NV_40) {
		switch (nv_device(drm->device)->chipset) {
		case 0x43:
			sensor->offset_mult = 32060;
			sensor->offset_div = 1000;
			sensor->slope_mult = 792;
			sensor->slope_div = 1000;
			break;

		case 0x44:
		case 0x47:
		case 0x4a:
			sensor->offset_mult = 27839;
			sensor->offset_div = 1000;
			sensor->slope_mult = 780;
			sensor->slope_div = 1000;
			break;

		case 0x46:
			sensor->offset_mult = -24775;
			sensor->offset_div = 100;
			sensor->slope_mult = 467;
			sensor->slope_div = 10000;
			break;

		case 0x49:
			sensor->offset_mult = -25051;
			sensor->offset_div = 100;
			sensor->slope_mult = 458;
			sensor->slope_div = 10000;
			break;

		case 0x4b:
			sensor->offset_mult = -24088;
			sensor->offset_div = 100;
			sensor->slope_mult = 442;
			sensor->slope_div = 10000;
			break;

		case 0x50:
			sensor->offset_mult = -22749;
			sensor->offset_div = 100;
			sensor->slope_mult = 431;
			sensor->slope_div = 10000;
			break;

		case 0x67:
			sensor->offset_mult = -26149;
			sensor->offset_div = 100;
			sensor->slope_mult = 484;
			sensor->slope_div = 10000;
			break;
		}
	}

	headerlen = temp[1];
	recordlen = temp[2];
	entries = temp[3];
	temp = temp + headerlen;

	/* Read the entries from the table */
	for (i = 0; i < entries; i++) {
		s16 value = ROM16(temp[1]);

		switch (temp[0]) {
		case 0x01:
			if ((value & 0x8f) == 0)
				sensor->offset_constant = (value >> 9) & 0x7f;
			break;

		case 0x04:
			if ((value & 0xf00f) == 0xa000) /* core */
				temps->critical = (value&0x0ff0) >> 4;
			break;

		case 0x07:
			if ((value & 0xf00f) == 0xa000) /* core */
				temps->down_clock = (value&0x0ff0) >> 4;
			break;

		case 0x08:
			if ((value & 0xf00f) == 0xa000) /* core */
				temps->fan_boost = (value&0x0ff0) >> 4;
			break;

		case 0x10:
			sensor->offset_mult = value;
			break;

		case 0x11:
			sensor->offset_div = value;
			break;

		case 0x12:
			sensor->slope_mult = value;
			break;

		case 0x13:
			sensor->slope_div = value;
			break;
		case 0x22:
			pm->fan.min_duty = value & 0xff;
			pm->fan.max_duty = (value & 0xff00) >> 8;
			break;
		case 0x26:
			pm->fan.pwm_freq = value;
			break;
		}
		temp += recordlen;
	}

	nouveau_temp_safety_checks(dev);

	/* check the fan min/max settings */
	if (pm->fan.min_duty < 10)
		pm->fan.min_duty = 10;
	if (pm->fan.max_duty > 100)
		pm->fan.max_duty = 100;
	if (pm->fan.max_duty < pm->fan.min_duty)
		pm->fan.max_duty = pm->fan.min_duty;
}

static int
nv40_sensor_setup(struct drm_device *dev)
{
	struct nouveau_device *device = nouveau_dev(dev);
	struct nouveau_drm *drm = nouveau_drm(dev);
	struct nouveau_pm *pm = nouveau_pm(dev);
	struct nouveau_pm_temp_sensor_constants *sensor = &pm->sensor_constants;
	s32 offset = sensor->offset_mult / sensor->offset_div;
	s32 sensor_calibration;

	/* set up the sensors */
	sensor_calibration = 120 - offset - sensor->offset_constant;
	sensor_calibration = sensor_calibration * sensor->slope_div /
				sensor->slope_mult;

	if (nv_device(drm->device)->chipset >= 0x46)
		sensor_calibration |= 0x80000000;
	else
		sensor_calibration |= 0x10000000;

	nv_wr32(device, 0x0015b0, sensor_calibration);

	/* Wait for the sensor to update */
	msleep(5);

	/* read */
	return nv_rd32(device, 0x0015b4) & 0x1fff;
}

int
nv40_temp_get(struct drm_device *dev)
{
	struct nouveau_device *device = nouveau_dev(dev);
	struct nouveau_drm *drm = nouveau_drm(dev);
	struct nouveau_pm *pm = nouveau_pm(dev);
	struct nouveau_pm_temp_sensor_constants *sensor = &pm->sensor_constants;
	int offset = sensor->offset_mult / sensor->offset_div;
	int core_temp;

	if (nv_device(drm->device)->card_type >= NV_50) {
		core_temp = nv_rd32(device, 0x20008);
	} else {
		core_temp = nv_rd32(device, 0x0015b4) & 0x1fff;
		/* Setup the sensor if the temperature is 0 */
		if (core_temp == 0)
			core_temp = nv40_sensor_setup(dev);
	}

	core_temp = core_temp * sensor->slope_mult / sensor->slope_div;
	core_temp = core_temp + offset + sensor->offset_constant;

	return core_temp;
}

int
nv84_temp_get(struct drm_device *dev)
{
	struct nouveau_device *device = nouveau_dev(dev);
	return nv_rd32(device, 0x20400);
}

void
nouveau_temp_safety_checks(struct drm_device *dev)
{
	struct nouveau_pm *pm = nouveau_pm(dev);
	struct nouveau_pm_threshold_temp *temps = &pm->threshold_temp;

	if (temps->critical > 120)
		temps->critical = 120;
	else if (temps->critical < 80)
		temps->critical = 80;

	if (temps->down_clock > 110)
		temps->down_clock = 110;
	else if (temps->down_clock < 60)
		temps->down_clock = 60;

	if (temps->fan_boost > 100)
		temps->fan_boost = 100;
	else if (temps->fan_boost < 40)
		temps->fan_boost = 40;
}

static bool
probe_monitoring_device(struct nouveau_i2c_port *i2c,
			struct i2c_board_info *info)
{
	struct i2c_client *client;

	request_module("%s%s", I2C_MODULE_PREFIX, info->type);

	client = i2c_new_device(&i2c->adapter, info);
	if (!client)
		return false;

	if (!client->driver || client->driver->detect(client, info)) {
		i2c_unregister_device(client);
		return false;
	}

	return true;
}

static void
nouveau_temp_probe_i2c(struct drm_device *dev)
{
	struct nouveau_device *device = nouveau_dev(dev);
	struct nouveau_i2c *i2c = nouveau_i2c(device);
	struct i2c_board_info info[] = {
		{ I2C_BOARD_INFO("w83l785ts", 0x2d) },
		{ I2C_BOARD_INFO("w83781d", 0x2d) },
		{ I2C_BOARD_INFO("adt7473", 0x2e) },
		{ I2C_BOARD_INFO("f75375", 0x2e) },
		{ I2C_BOARD_INFO("lm99", 0x4c) },
		{ }
	};

	i2c->identify(i2c, NV_I2C_DEFAULT(0), "monitoring device", info,
		      probe_monitoring_device);
}

void
nouveau_temp_init(struct drm_device *dev)
{
	struct nouveau_drm *drm = nouveau_drm(dev);
	struct nvbios *bios = &drm->vbios;
	struct bit_entry P;
	u8 *temp = NULL;

	if (bios->type == NVBIOS_BIT) {
		if (bit_table(dev, 'P', &P))
			return;

		if (P.version == 1)
			temp = ROMPTR(dev, P.data[12]);
		else if (P.version == 2)
			temp = ROMPTR(dev, P.data[16]);
		else
			NV_WARN(drm, "unknown temp for BIT P %d\n", P.version);

		nouveau_temp_vbios_parse(dev, temp);
	}

	nouveau_temp_probe_i2c(dev);
}

void
nouveau_temp_fini(struct drm_device *dev)
{

}
