LOCAL_PATH := $(call my-dir)

QMMF_SDK_TOP_SRCDIR := $(LOCAL_PATH)/..

include $(QMMF_SDK_TOP_SRCDIR)/build.mk

ifneq (,$(BUILD_QMMMF))

# Build qmmf-server binary

include $(CLEAR_VARS)

include $(QMMF_SDK_TOP_SRCDIR)/common.mk

LOCAL_C_INCLUDES += $(TOP)/system/media/camera/include
LOCAL_C_INCLUDES += $(TARGET_OUT_HEADERS)/mm-core/omxcore
LOCAL_C_INCLUDES += $(MEDIA_HAL_PATH)
LOCAL_C_INCLUDES += $(TARGET_OUT_HEADERS)/qcom/display
LOCAL_C_INCLUDES += $(TARGET_OUT_HEADERS)/qcom/display/sdm
LOCAL_C_INCLUDES += $(QMMF_SDK_TOP_SRCDIR)/recorder/src/service/post-process

ifeq ($(TARGET_USES_GRALLOC1),true)
LOCAL_C_INCLUDES += $(DISPLAY_HAL_PATH)
endif

LOCAL_SRC_FILES := qmmf_server_main.cc

LOCAL_SHARED_LIBRARIES += libqmmf_memory_interface

ifneq ($(DISABLE_DISPLAY),1)
LOCAL_SHARED_LIBRARIES += libqmmf_display_service
endif

ifneq ($(DISABLE_AUDIO_SERVICE),1)
LOCAL_SHARED_LIBRARIES += libqmmf_audio_service
endif

LOCAL_SHARED_LIBRARIES += libqmmf_recorder_service

LOCAL_SHARED_LIBRARIES += libbinder

LOCAL_MODULE = qmmf-server

include $(BUILD_EXECUTABLE)

endif # BUILD_QMMMF
