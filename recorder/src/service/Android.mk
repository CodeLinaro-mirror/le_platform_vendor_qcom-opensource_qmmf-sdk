LOCAL_PATH := $(call my-dir)

QMMF_SDK_TOP_SRCDIR := $(LOCAL_PATH)/../../..

INTERFACE_INCLUDES := $(LOCAL_PATH)/post-process

include $(QMMF_SDK_TOP_SRCDIR)/build.mk

ifneq (,$(BUILD_QMMMF))

# Build qmmf recorder service library
# libqmmf_recorder_service.so

include $(CLEAR_VARS)

include $(QMMF_SDK_TOP_SRCDIR)/common.mk

LOCAL_C_INCLUDES += $(CAMERA_HAL_PATH)/QCamera2/HAL3
LOCAL_C_INCLUDES += $(TARGET_OUT_HEADERS)/mm-core/omxcore
LOCAL_C_INCLUDES += $(MEDIA_HAL_PATH)
ifeq ($(TARGET_USES_GRALLOC1),true)
LOCAL_C_INCLUDES += $(DISPLAY_HAL_PATH)
endif
# reprocess-related includes
LOCAL_C_INCLUDES += $(CAMERA_HAL_PATH)/QCamera2/stack/common
LOCAL_C_INCLUDES += $(CAMERA_HAL_PATH)/mm-image-codec/qomx_core
LOCAL_C_INCLUDES += $(CAMERA_HAL_PATH)/mm-image-codec/qexif
LOCAL_C_INCLUDES += $(QMMF_SDK_TOP_SRCDIR)/recorder/src/service/post-process

LOCAL_SRC_FILES := qmmf_recorder_service.cc
LOCAL_SRC_FILES += qmmf_recorder_impl.cc
LOCAL_SRC_FILES += qmmf_recorder_ion.cc
LOCAL_SRC_FILES += qmmf_remote_cb.cc
LOCAL_SRC_FILES += qmmf_camera_source.cc
LOCAL_SRC_FILES += qmmf_camera_frc.cc
LOCAL_SRC_FILES += qmmf_camera_context.cc
LOCAL_SRC_FILES += qmmf_fake_camera.cc
LOCAL_SRC_FILES += qmmf_encoder_core.cc
LOCAL_SRC_FILES += qmmf_audio_pulse_client.cc
LOCAL_SRC_FILES += qmmf_audio_source.cc
LOCAL_SRC_FILES += qmmf_audio_raw_track_source.cc
LOCAL_SRC_FILES += qmmf_audio_encoded_track_source.cc
LOCAL_SRC_FILES += qmmf_audio_encoder_core.cc
ifneq ($(DISABLE_PP_JPEG),1)
LOCAL_SRC_FILES += qmmf_camera_jpeg.cc
endif

LOCAL_SRC_FILES += qmmf_camera_reprocess_impl.cc
LOCAL_SRC_FILES += post-process/factory/qmmf_postproc_factory.cc
LOCAL_SRC_FILES += post-process/node/qmmf_postproc_node.cc
LOCAL_SRC_FILES += post-process/memory/qmmf_postproc_memory_pool.cc
LOCAL_SRC_FILES += post-process/pipe/qmmf_postproc_pipe.cc
LOCAL_SRC_FILES += post-process/common/qmmf_postproc_thread.cc
LOCAL_SRC_FILES += qmmf_camera_rescaler.cc

LOCAL_SHARED_LIBRARIES += libqmmf_utils libqmmf_postproc_algo libqmmf_jpeg
LOCAL_SHARED_LIBRARIES += libqmmf_camera_hal_reproc libqmmf_postproc_test
LOCAL_SHARED_LIBRARIES += libqmmf_recorder_client libqmmf_camera_adaptor
LOCAL_SHARED_LIBRARIES += libqmmf_codec_adaptor libqmmf_audio_client
LOCAL_SHARED_LIBRARIES += libqmmf_overlay
LOCAL_SHARED_LIBRARIES += libqmmf_exif_generator
LOCAL_SHARED_LIBRARIES += libqmmf_memory_interface
ifneq ($(DISABLE_DISPLAY),1)
LOCAL_SHARED_LIBRARIES += libqmmf_display_client
endif
LOCAL_SHARED_LIBRARIES += libcamera_client libbinder libhardware
LOCAL_SHARED_LIBRARIES += libqmmf_postproc_frame_skip
LOCAL_SHARED_LIBRARIES += libqmmf_common_resizer_fastcv
LOCAL_SHARED_LIBRARIES += libfastcvopt
LOCAL_SHARED_LIBRARIES += libqmmf_common_resizer_c2d
LOCAL_SHARED_LIBRARIES += libqmmf_common_resizer_neon
ifneq ($(DISABLE_PP_JPEG),1)
LOCAL_SHARED_LIBRARIES += libqmmf_common_jpeg_encoder
endif

LOCAL_SHARED_LIBRARIES += $(LIB_JSONCPP)

LOCAL_MODULE = libqmmf_recorder_service

include $(BUILD_SHARED_LIBRARY)

include $(LOCAL_PATH)/post-process/modules/Android.mk

endif # BUILD_QMMMF
