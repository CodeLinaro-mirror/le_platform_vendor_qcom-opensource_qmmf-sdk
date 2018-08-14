LOCAL_PATH := $(call my-dir)

QMMF_SDK_TOP_SRCDIR := $(LOCAL_PATH)/../../../../../..

include $(QMMF_SDK_TOP_SRCDIR)/build.mk

ifneq (,$(BUILD_QMMMF))

# Build qmmf camera hal reprocess library
# libqmmf_jpeg.so

include $(CLEAR_VARS)

include $(QMMF_SDK_TOP_SRCDIR)/common.mk

ifneq ($(DISABLE_PP_JPEG),1)
LOCAL_SRC_FILES := qmmf_jpeg.cc
endif

LOCAL_SHARED_LIBRARIES += libcamera_client libjsoncpp_vendor libqmmf_utils

ifneq ($(DISABLE_PP_JPEG),1)
LOCAL_SHARED_LIBRARIES += libqmmf_common_jpeg_encoder
endif

LOCAL_MODULE = libqmmf_jpeg

include $(BUILD_SHARED_LIBRARY)

endif # BUILD_QMMMF
