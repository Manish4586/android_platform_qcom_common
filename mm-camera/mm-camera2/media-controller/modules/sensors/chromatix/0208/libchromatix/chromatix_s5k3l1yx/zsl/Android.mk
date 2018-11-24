S5K3L1YX_CHROMATIX_ZSL_PATH := $(call my-dir)

# ---------------------------------------------------------------------------
#                      Make the shared library (libchromatix_s5k3l1yx_zsl)
# ---------------------------------------------------------------------------

include $(CLEAR_VARS)
LOCAL_PATH := $(S5K3L1YX_CHROMATIX_ZSL_PATH)
LOCAL_MODULE_TAGS := optional

LOCAL_CFLAGS:= \
        -DAMSS_VERSION=$(AMSS_VERSION) \
        $(mmcamera_debug_defines) \
        $(mmcamera_debug_cflags) \
        -include camera_defs_i.h

LOCAL_C_INCLUDES := $(LOCAL_PATH)/../../../
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../../../../../../../../../common/
LOCAL_C_INCLUDES += chromatix_s5k3l1yx_zsl.h

LOCAL_SRC_FILES:= chromatix_s5k3l1yx_zsl.c

LOCAL_MODULE           := libchromatix_s5k3l1yx_zsl
LOCAL_SHARED_LIBRARIES := libcutils
include $(LOCAL_PATH)/../../../../../../../../../local_additional_dependency.mk

ifeq ($(MM_DEBUG),true)
LOCAL_SHARED_LIBRARIES += liblog
endif

LOCAL_MODULE_OWNER := qcom 
LOCAL_32_BIT_ONLY := true
LOCAL_PROPRIETARY_MODULE := true

include $(BUILD_SHARED_LIBRARY)
