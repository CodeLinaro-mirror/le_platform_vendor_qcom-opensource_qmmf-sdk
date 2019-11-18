LOCAL_PATH := $(call my-dir)

QMMF_SDK_TOP_SRCDIR := $(LOCAL_PATH)/../../..

include $(QMMF_SDK_TOP_SRCDIR)/build.mk

ifneq (,$(BUILD_QMMMF))

# Build recorder audio test application binary

include $(CLEAR_VARS)

include $(QMMF_SDK_TOP_SRCDIR)/common.mk

LOCAL_SRC_FILES  := qmmf_recorder_test.cc
LOCAL_SRC_FILES  += qmmf_recorder_test_wav.cc
LOCAL_SRC_FILES  += qmmf_recorder_test_aac.cc
LOCAL_SRC_FILES  += qmmf_recorder_test_amr.cc
LOCAL_SRC_FILES  += qmmf_recorder_test_mpegh.cc

LOCAL_SHARED_LIBRARIES += libqmmf_utils libqmmf_recorder_client

LOCAL_MODULE = qmmf_recorder_atest

include $(BUILD_EXECUTABLE)

endif # BUILD_QMMMF
