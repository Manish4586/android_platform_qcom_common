/*============================================================================

  Copyright (c) 2014 Qualcomm Technologies, Inc. All Rights Reserved.
  Qualcomm Technologies Proprietary and Confidential.

============================================================================*/

#include "af_algo_tuning.h"

static af_algo_ctrl_t af_algo_data = {
#include "rohm_bu64243gwz_camcorder.h"
};

void *ROHM_BU64243GWZ_camcorder_af_algo_open_lib(void)
{
  return &af_algo_data;
}
