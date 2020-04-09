LOCAL_PATH := $(call my-dir)

QMMF_SDK_TOP_SRCDIR := $(LOCAL_PATH)/../../..

include $(QMMF_SDK_TOP_SRCDIR)/build.mk

ifneq (,$(BUILD_QMMMF))

# Build recorder gtest application binary

include $(CLEAR_VARS)

include $(QMMF_SDK_TOP_SRCDIR)/common.mk
LOCAL_CFLAGS += -DUSE_SKIA=1
LOCAL_CFLAGS += -DUSE_CAIRO=0

LOCAL_C_INCLUDES += $(TOP)/system/media/camera/include
LOCAL_C_INCLUDES += $(TOP)/system/core/base/include
LOCAL_C_INCLUDES += $(CAMERA_HAL_PATH)/QCamera2/HAL3
LOCAL_C_INCLUDES += $(TOP)/external/skia/include/core/

LOCAL_SRC_FILES := qmmf_recorder_gtest_common.cc
LOCAL_SRC_FILES += qmmf_recorder_base_gtest.cc
LOCAL_SRC_FILES += qmmf_recorder_image_gtest.cc
LOCAL_SRC_FILES += qmmf_recorder_video_gtest.cc
LOCAL_SRC_FILES += qmmf_recorder_video_snapshot_gtest.cc

LOCAL_SHARED_LIBRARIES += libqmmf_recorder_client libqmmf_av_queue libqmmf_memory_interface
LOCAL_SHARED_LIBRARIES += libcamera_client libskia

LOCAL_SHARED_LIBRARIES += $(LIB_JSONCPP)

LOCAL_MODULE = qmmf_recorder_gtest

ifeq ($(LOCAL_VENDOR_MODULE),true)
LOCAL_VENDOR_MODULE := false
endif

include $(BUILD_NATIVE_TEST)

include $(CLEAR_VARS)

LOCAL_MODULE := imx577_sensor_mode_config.json

LOCAL_MODULE_CLASS  := ETC

LOCAL_SRC_FILES     := $(LOCAL_MODULE)

LOCAL_MODULE_PATH   := $(TARGET_OUT_DATA)/misc/qmmf

include $(BUILD_PREBUILT)

endif
