/*============================================================================
  @file Gestures.h

  @brief
  Gestures class definition.

  Copyright (c) 2014 Qualcomm Technologies, Inc.  All Rights Reserved.
  Qualcomm Technologies Proprietary and Confidential.
============================================================================*/
#ifndef ANDROID_GESTURES_SENSOR_H
#define ANDROID_GESTURES_SENSOR_H

#include "SAMSensor.h"
#include "sns_sam_basic_gestures_v01.h"

/*============================================================================
 * Class Gestures
=============================================================================*/

class Gestures : public SAMSensor {

public:
    Gestures(int handle);
    ~Gestures();
    int enable(int en);
/*===========================================================================
  FUNCTION:  processResp
    Process the response to the sensor1 SENSOR1_MSG_TYPE_RESP
    Parameters
        @msg_hdr : sensor1 message header
        @msg_ptr : sensor1 message data
===========================================================================*/
    void processResp(sensor1_msg_header_s *msg_hdr, void *msg_ptr);
/*===========================================================================
  FUNCTION:  processInd
    Process the response to the sensor1 SENSOR1_MSG_TYPE_IND
    Parameters
        @msg_hdr : sensor1 message header
        @msg_ptr : sensor1 message data
===========================================================================*/
    void processInd(sensor1_msg_header_s *msg_hdr, void *msg_ptr);
};
#endif
