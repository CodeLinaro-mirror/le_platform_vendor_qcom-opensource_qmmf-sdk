LOCAL_PATH := $(call my-dir)
QMMF_SDK_TOP_SRCDIR := $(LOCAL_PATH)/..

QMMF_TOPO_MAN_INCLUDE_PATH := $(QMMF_SDK_TOP_SRCDIR)/include/qmmf-plugin

include $(QMMF_SDK_TOP_SRCDIR)/build.mk

ifneq (,$(BUILD_QMMMF))

# Build qmmf test outplace algorithm library

include $(CLEAR_VARS)

include $(QMMF_SDK_TOP_SRCDIR)/common.mk

LOCAL_C_INCLUDES += $(QMMF_TOPO_MAN_INCLUDE_PATH)

LOCAL_SRC_FILES := sample-plugins/qmmf_test_algo_outplace.cc

LOCAL_MODULE = libqmmf_test_algo_outplace

include $(BUILD_SHARED_LIBRARY)

# Build qmmf test inplace algorithm library

include $(CLEAR_VARS)

include $(QMMF_SDK_TOP_SRCDIR)/common.mk

LOCAL_C_INCLUDES += $(QMMF_TOPO_MAN_INCLUDE_PATH)

LOCAL_SRC_FILES := sample-plugins/qmmf_test_algo_inplace.cc

LOCAL_MODULE = libqmmf_test_algo_inplace

include $(BUILD_SHARED_LIBRARY)

# Build qmmf test outplace algorithm with history library

include $(CLEAR_VARS)

include $(QMMF_SDK_TOP_SRCDIR)/common.mk

LOCAL_C_INCLUDES += $(QMMF_TOPO_MAN_INCLUDE_PATH)

LOCAL_SRC_FILES := sample-plugins/qmmf_test_algo_outplace_history.cc

LOCAL_MODULE = libqmmf_test_algo_outplace_history

include $(BUILD_SHARED_LIBRARY)

# Build qmmf test inplace algorithm with history library

include $(CLEAR_VARS)

include $(QMMF_SDK_TOP_SRCDIR)/common.mk

LOCAL_C_INCLUDES += $(QMMF_TOPO_MAN_INCLUDE_PATH)

LOCAL_SRC_FILES := sample-plugins/qmmf_test_algo_inplace_history.cc

LOCAL_MODULE = libqmmf_test_algo_inplace_history

include $(BUILD_SHARED_LIBRARY)

# Build qmmf test resizer library

include $(CLEAR_VARS)

include $(QMMF_SDK_TOP_SRCDIR)/common.mk

LOCAL_C_INCLUDES += $(QMMF_TOPO_MAN_INCLUDE_PATH)

LOCAL_SRC_FILES := sample-plugins/qmmf_test_algo_resizer.cc

LOCAL_MODULE = libqmmf_test_algo_resizer

include $(BUILD_SHARED_LIBRARY)

# Build qmmf utils gtest application binary

include $(CLEAR_VARS)

include $(QMMF_SDK_TOP_SRCDIR)/common.mk

LOCAL_C_INCLUDES += $(QMMF_TOPO_MAN_INCLUDE_PATH)

LOCAL_SRC_FILES := gtest/qmmf_utils_gtest.cc

LOCAL_MODULE = qmmf_utils_gtest

ifeq ($(LOCAL_VENDOR_MODULE),true)
LOCAL_VENDOR_MODULE := false
endif

include $(BUILD_NATIVE_TEST)

# Build qmmf algorithm interface gtest application binary

include $(CLEAR_VARS)

include $(QMMF_SDK_TOP_SRCDIR)/common.mk

LOCAL_C_INCLUDES += $(QMMF_TOPO_MAN_INCLUDE_PATH)

LOCAL_SRC_FILES := gtest/qmmf_algo_interface_gtest.cc
LOCAL_SRC_FILES += gtest/buffer_handler.cc
LOCAL_SRC_FILES += gtest/qmmf_algo_gtest_configuration_buffer.cc
LOCAL_SRC_FILES += gtest/qmmf_algo_gtest_test_content.cc
LOCAL_SRC_FILES += gtest/qmmf_algo_gtest_test_suite.cc
LOCAL_SRC_FILES += gtest/qmmf_algo_gtest_configuration.cc
LOCAL_SRC_FILES += gtest/ion_buffer.cc
LOCAL_SRC_FILES += gtest/heap_buffer.cc
LOCAL_SRC_FILES += gtest/heap_tracker.cc

LOCAL_SHARED_LIBRARIES += $(LIB_JSONCPP)

LOCAL_MODULE = qmmf_algo_interface_gtest

ifeq ($(LOCAL_VENDOR_MODULE),true)
LOCAL_VENDOR_MODULE := false
endif

include $(BUILD_NATIVE_TEST)

include $(CLEAR_VARS)
LOCAL_MODULE := qmmf_algo_gtest_bayer_lcac.json
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_MODULE_PATH := $(TARGET_OUT_DATA)/misc/qmmf/
LOCAL_SRC_FILES := test_cases/$(LOCAL_MODULE)
LOCAL_MODULE_OWNER := qti
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := qmmf_algo_gtest_binning_correction.json
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_MODULE_PATH := $(TARGET_OUT_DATA)/misc/qmmf/
LOCAL_SRC_FILES := test_cases/$(LOCAL_MODULE)
LOCAL_MODULE_OWNER := qti
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := qmmf_algo_gtest_bit_blit.json
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_MODULE_PATH := $(TARGET_OUT_DATA)/misc/qmmf/
LOCAL_SRC_FILES := test_cases/$(LOCAL_MODULE)
LOCAL_MODULE_OWNER := qti
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := qmmf_algo_gtest_edge_smooth.json
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_MODULE_PATH := $(TARGET_OUT_DATA)/misc/qmmf/
LOCAL_SRC_FILES := test_cases/$(LOCAL_MODULE)
LOCAL_MODULE_OWNER := qti
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := qmmf_algo_gtest_inplace_history.json
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_MODULE_PATH := $(TARGET_OUT_DATA)/misc/qmmf/
LOCAL_SRC_FILES := test_cases/$(LOCAL_MODULE)
LOCAL_MODULE_OWNER := qti
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := qmmf_algo_gtest_inplace.json
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_MODULE_PATH := $(TARGET_OUT_DATA)/misc/qmmf/
LOCAL_SRC_FILES := test_cases/$(LOCAL_MODULE)
LOCAL_MODULE_OWNER := qti
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := qmmf_algo_gtest_outplace_history.json
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_MODULE_PATH := $(TARGET_OUT_DATA)/misc/qmmf/
LOCAL_SRC_FILES := test_cases/$(LOCAL_MODULE)
LOCAL_MODULE_OWNER := qti
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := qmmf_algo_gtest_outplace.json
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_MODULE_PATH := $(TARGET_OUT_DATA)/misc/qmmf/
LOCAL_SRC_FILES := test_cases/$(LOCAL_MODULE)
LOCAL_MODULE_OWNER := qti
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := qmmf_algo_gtest_privacy_mask.json
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_MODULE_PATH := $(TARGET_OUT_DATA)/misc/qmmf/
LOCAL_SRC_FILES := test_cases/$(LOCAL_MODULE)
LOCAL_MODULE_OWNER := qti
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := qmmf_algo_gtest_res_conv_sub.json
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_MODULE_PATH := $(TARGET_OUT_DATA)/misc/qmmf/
LOCAL_SRC_FILES := test_cases/$(LOCAL_MODULE)
LOCAL_MODULE_OWNER := qti
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := qmmf_algo_gtest_svhdr.json
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_MODULE_PATH := $(TARGET_OUT_DATA)/misc/qmmf/
LOCAL_SRC_FILES := test_cases/$(LOCAL_MODULE)
LOCAL_MODULE_OWNER := qti
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := qmmf_algo_gtest_yuv_cac.json
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_MODULE_PATH := $(TARGET_OUT_DATA)/misc/qmmf/
LOCAL_SRC_FILES := test_cases/$(LOCAL_MODULE)
LOCAL_MODULE_OWNER := qti
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := bayer_lcac_calibration.json
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_MODULE_PATH := $(TARGET_OUT_DATA)/misc/qmmf/
LOCAL_SRC_FILES := test_cases/calibration/$(LOCAL_MODULE)
LOCAL_MODULE_OWNER := qti
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := binning_correction_calibration.json
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_MODULE_PATH := $(TARGET_OUT_DATA)/misc/qmmf/
LOCAL_SRC_FILES := test_cases/calibration/$(LOCAL_MODULE)
LOCAL_MODULE_OWNER := qti
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := bit_blit_configuration.json
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_MODULE_PATH := $(TARGET_OUT_DATA)/misc/qmmf/
LOCAL_SRC_FILES := test_cases/calibration/$(LOCAL_MODULE)
LOCAL_MODULE_OWNER := qti
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := privacy_mask_configuration.json
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_MODULE_PATH := $(TARGET_OUT_DATA)/misc/qmmf/
LOCAL_SRC_FILES := test_cases/calibration/$(LOCAL_MODULE)
LOCAL_MODULE_OWNER := qti
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := res_conv_sub_configuration.json
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_MODULE_PATH := $(TARGET_OUT_DATA)/misc/qmmf/
LOCAL_SRC_FILES := test_cases/calibration/$(LOCAL_MODULE)
LOCAL_MODULE_OWNER := qti
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := yuv_cac_calibration_nv21.json
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_MODULE_PATH := $(TARGET_OUT_DATA)/misc/qmmf/
LOCAL_SRC_FILES := test_cases/calibration/$(LOCAL_MODULE)
LOCAL_MODULE_OWNER := qti
include $(BUILD_PREBUILT)

endif # BUILD_QMMMF
