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
LOCAL_SRC_FILES += gtest/heap_tracker.cc

LOCAL_SHARED_LIBRARIES += $(LIB_JSONCPP)

LOCAL_MODULE = qmmf_algo_interface_gtest

ifeq ($(LOCAL_VENDOR_MODULE),true)
LOCAL_VENDOR_MODULE := false
endif

include $(BUILD_NATIVE_TEST)

endif # BUILD_QMMMF
