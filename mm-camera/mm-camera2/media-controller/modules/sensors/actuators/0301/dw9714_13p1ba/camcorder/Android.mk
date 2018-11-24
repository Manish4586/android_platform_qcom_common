LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE_TAGS := optional

LOCAL_CFLAGS:= -DAMSS_VERSION=$(AMSS_VERSION) \
        $(mmcamera_debug_defines) \
        $(mmcamera_debug_cflags)

MCTL_PATH := $(LOCAL_PATH)/../../../../../../media-controller/

LOCAL_C_INCLUDES := $(LOCAL_PATH)/../../
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../../../../../module/
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../../../../../../../../../common/
LOCAL_C_INCLUDES += $(MCTL_PATH)/mct/tools/
LOCAL_C_INCLUDES += $(MCTL_PATH)/modules/sensors/actuators/
LOCAL_C_INCLUDES += dw9714_13p1ba_camcorder.h

LOCAL_SRC_FILES:= dw9714_13p1ba_camcorder.c

LOCAL_MODULE           := libactuator_dw9714_13p1ba_camcorder
LOCAL_SHARED_LIBRARIES := libcutils
include $(LOCAL_PATH)/../../../../../../../../local_additional_dependency.mk

ifeq ($(MM_DEBUG),true)
LOCAL_SHARED_LIBRARIES += liblog
endif

LOCAL_MODULE_OWNER := qcom 
LOCAL_32_BIT_ONLY := true
LOCAL_PROPRIETARY_MODULE := true

include $(BUILD_SHARED_LIBRARY)
