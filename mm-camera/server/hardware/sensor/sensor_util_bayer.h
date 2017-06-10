/*============================================================================

   Copyright (c) 2011-2012 Qualcomm Technologies, Inc.  All Rights Reserved.
   Qualcomm Technologies Proprietary and Confidential.

============================================================================*/
#ifndef SENSOR_UTIL_BAYER_H
#define SENSOR_UTIL_BAYER_H

#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "camera_dbg.h"
#include "sensor.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

int8_t sensor_util_bayer_config(void *sctrl);
int8_t sensor_util_bayer_get_output_csi_info(void *sctrl);
int8_t sensor_util_bayer_power_up(void *sctrl);
int8_t sensor_util_get_eeprom_data1(void *sctrl);
int8_t sensor_util_bayer_get_dim_info(void *sctrl, void *dim_info);
int8_t sensor_util_bayer_get_preview_fps_range(void *sctrl, void *range);
int8_t sensor_util_bayer_set_frame_rate(void *sctrl, uint16_t fps);
uint32_t sensor_util_bayer_get_max_fps(void *sctrl, uint8_t mode);
int8_t sensor_util_bayer_set_op_mode(void *sctrl, uint8_t mode);
int8_t sensor_util_bayer_get_mode_aec_info(void *sctrl, void *);
int8_t sensor_util_bayer_get_cur_fps(void *sctrl, void *fps_data);
int8_t sensor_get_bayer_lens_info(void *sctrl, void *info);
int8_t sensor_util_bayer_set_exposure_gain(void *sctrl, void *);
int32_t sensor_util_bayer_write_exp_gain(void *sctrl, uint16_t gain, uint32_t line);
int8_t sensor_util_bayer_set_snapshot_exposure_gain(void *sctrl, void *);
int8_t sensor_util_bayer_get_max_supported_hfr_mode(void *sctrl, void *);
int8_t sensor_util_bayer_set_start_stream(void *sctrl);
int8_t sensor_util_bayer_set_stop_stream(void *sctrl);
int8_t sensor_util_bayer_get_csi_params(void *sctrl, void *data);
int8_t sensor_util_bayer_config_setting(void *sctrl, void *oem_setting);
int8_t sensor_util_bayer_get_camif_cfg(void *sctrl, void *data);
int8_t sensor_util_bayer_get_output_cfg(void *sctrl, void *data);
int8_t sensor_util_bayer_get_digital_gain(void *sctrl, void *data);
int8_t sensor_util_bayer_get_cur_res(void *sctrl, uint16_t op_mode, void *data);

#endif
