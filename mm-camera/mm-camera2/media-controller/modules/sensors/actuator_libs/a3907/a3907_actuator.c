/*============================================================================

  Copyright (c) 2014 Qualcomm Technologies, Inc. All Rights Reserved.
  Qualcomm Technologies Proprietary and Confidential.

============================================================================*/

#include "actuator_driver.h"

static actuator_driver_ctrl_t actuator_lib_ptr = {
#include "a3907_actuator.h"
};

void *actuator_driver_open_lib(void)
{
  return &actuator_lib_ptr;
}
