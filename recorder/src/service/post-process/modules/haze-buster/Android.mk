LOCAL_PATH := $(call my-dir)

QMMF_SDK_TOP_SRCDIR := $(LOCAL_PATH)/../../../../../..

include $(QMMF_SDK_TOP_SRCDIR)/build.mk

ifneq (,$(BUILD_QMMMF))

# Build qmmf camera haze buster library
# libqmmf_haze_buster.so

include $(CLEAR_VARS)

include $(QMMF_SDK_TOP_SRCDIR)/common.mk

LOCAL_C_INCLUDES += $(TOP)/hardware/qcom/camera/QCamera2/HAL3

LOCAL_SRC_FILES := qmmf_haze_buster.cc
LOCAL_SRC_FILES += ../../common/qmmf_camera_alg.cc

LOCAL_SHARED_LIBRARIES += libcamera_metadata

LOCAL_MODULE = libqmmf_haze_buster

include $(BUILD_SHARED_LIBRARY)

endif # BUILD_QMMMF
