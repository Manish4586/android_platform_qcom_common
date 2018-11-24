/*
* Copyright (c) 2014 Qualcomm Technologies, Inc.  All Rights Reserved.
* Qualcomm Technologies Proprietary and Confidential.
*
*/

#ifndef __WIFI_HAL_RTT_TEST_HPP__
#define __WIFI_HAL_RTT_TEST_HPP__

#include "wifi_hal.h"
#include <getopt.h>
#include "rtt.h"

namespace RTT_TEST
{
    class RttTestSuite
    {
    public:
        /* CLI cmd strings */
        static const char *RTT_CMD;
        static const char *RTT_RANGE_REQUEST;
        static const char *RTT_RANGE_CANCEL;
        static const char *RTT_GET_CAPABILITIES;

        /* Default service name */
        static const char *DEFAULT_SVC;

        /* Default service name */
        static const char *DEFAULT_SVC_INFO;

        RttTestSuite(wifi_interface_handle handle, wifi_request_id request_id);

        /* process the command line args */
        void processCmd(int argc, char **argv);

        /* execute the command line args */
        void executeCmd(int argc, char **argv, int cmdIndex);

        void setRequestId(int reqId);
        int getRequestId();

    private:
        wifi_interface_handle wifiHandle_;
        wifi_request_id id;

        /* Send a GscanSCAN command to Android HAL */
        void rttSendRangeRequest(int argc, char **argv);
        void rttSendCancelRangeRequest(int argc, char **argv);
        void rttSendGetCapabilitiesRequest(int argc, char **argv);
        int rttParseHex(unsigned char c);
        int rttParseMacAddress(const char* arg, u8* addr);
        void rttPrintCmdUsage(
            char **argv,
            const char *cmd,
            const char *sub_cmd,
            const struct option long_options[],
            int size);
    };
}

#endif
