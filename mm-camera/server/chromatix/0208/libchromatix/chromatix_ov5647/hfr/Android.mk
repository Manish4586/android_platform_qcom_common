OV5647_CHROMATIX_PREVIEW_PATH := $(call my-dir)

# ---------------------------------------------------------------------------
#                Make the shared library (libov5647)
# ---------------------------------------------------------------------------

include $(CLEAR_VARS)
LOCAL_PATH := $(OV5647_CHROMATIX_PREVIEW_PATH)
LOCAL_MODULE_TAGS := optional debug

LOCAL_CFLAGS:= \
        -DAMSS_VERSION=$(AMSS_VERSION) \
        $(mmcamera_debug_defines) \
        $(mmcamera_debug_cflags) \
        -include camera_defs_i.h

LOCAL_C_INCLUDES := $(LOCAL_PATH)/../../../
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../../../../../
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../../../../../include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../../../../../hardware/sensor
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../../../../../hardware/flash
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../../../../../hardware/flash/xenon
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../../../../../isp3a
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../../../../../../common
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../../../../../../../../../../hardware/qcom/camera
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../../../../../../../mm-still/jpeg/inc
#LOCAL_C_INCLUDES += chromatix_ov5647_preview.h

LOCAL_SRC_FILES:= chromatix_ov5647_video_hfr.c

LOCAL_MODULE           := libchromatix_ov5647_video_hfr
LOCAL_PRELINK_MODULE   := false
LOCAL_SHARED_LIBRARIES := libcutils
include $(LOCAL_PATH)/../../../../../../local_additional_dependency.mk

ifeq ($(MM_DEBUG),true)
LOCAL_SHARED_LIBRARIES += liblog
endif

LOCAL_MODULE_OWNER := qcom 
LOCAL_32_BIT_ONLY := true
LOCAL_PROPRIETARY_MODULE := true

include $(BUILD_SHARED_LIBRARY)
