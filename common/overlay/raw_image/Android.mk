LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

include $(QMMF_SDK_TOP_SRCDIR)/common.mk

LOCAL_MODULE := overlay_test.rgba

LOCAL_MODULE_CLASS  := ETC

LOCAL_SRC_FILES     := $(LOCAL_MODULE)

LOCAL_MODULE_PATH   := $(TARGET_OUT_DATA)/misc/qmmf

include $(BUILD_PREBUILT)
