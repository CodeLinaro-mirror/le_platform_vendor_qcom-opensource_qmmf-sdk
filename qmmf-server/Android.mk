LOCAL_PATH := $(call my-dir)

QMMF_SDK_TOP_SRCDIR := $(LOCAL_PATH)/..

include $(QMMF_SDK_TOP_SRCDIR)/build.mk

ifneq (,$(BUILD_QMMMF))

# Build qmmf-server binary

include $(CLEAR_VARS)

include $(QMMF_SDK_TOP_SRCDIR)/common.mk

LOCAL_C_INCLUDES += $(TOP)/system/media/camera/include
LOCAL_C_INCLUDES += $(TARGET_OUT_HEADERS)/mm-core/omxcore
LOCAL_C_INCLUDES += $(TOP)/hardware/qcom/media
LOCAL_C_INCLUDES += $(TARGET_OUT_HEADERS)/qcom/display
LOCAL_C_INCLUDES += $(TARGET_OUT_HEADERS)/qcom/display/sdm

ifeq ($(TARGET_USES_GRALLOC1),true)
LOCAL_C_INCLUDES += $(TOP)/hardware/qcom/display
endif

LOCAL_SRC_FILES := qmmf_server_main.cc

ifneq ($(DISABLE_SYSTEM_SERVICE),1)
LOCAL_SHARED_LIBRARIES += libqmmf_system_service
endif

ifneq ($(DISABLE_DISPLAY),1)
LOCAL_SHARED_LIBRARIES += libqmmf_display_service
endif

ifneq ($(DISABLE_AUDIO_SERVICE),1)
LOCAL_SHARED_LIBRARIES += libqmmf_audio_service
endif

LOCAL_SHARED_LIBRARIES += libqmmf_recorder_service

ifneq ($(DISABLE_PLAYER_SERVICE),1)
LOCAL_SHARED_LIBRARIES += libqmmf_player_service
endif

LOCAL_SHARED_LIBRARIES += libbinder

LOCAL_MODULE = qmmf-server

include $(BUILD_EXECUTABLE)

endif # BUILD_QMMMF
