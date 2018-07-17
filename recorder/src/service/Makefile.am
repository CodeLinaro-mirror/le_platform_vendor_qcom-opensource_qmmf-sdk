source_files = qmmf_recorder_service.cc
source_files += qmmf_recorder_impl.cc
source_files += qmmf_recorder_ion.cc
source_files += qmmf_remote_cb.cc
source_files += qmmf_camera_source.cc
source_files += qmmf_camera_context.cc
source_files += qmmf_fake_camera.cc
source_files += qmmf_multicamera_manager.cc
source_files += qmmf_encoder_core.cc
source_files += qmmf_audio_source.cc
source_files += qmmf_audio_raw_track_source.cc
source_files += qmmf_audio_encoded_track_source.cc
source_files += qmmf_audio_encoder_core.cc
source_files += qmmf_camera_jpeg.cc

source_files += qmmf_camera_reprocess_impl.cc
source_files += post-process/factory/qmmf_postproc_factory.cc
source_files += post-process/node/qmmf_postproc_node.cc
source_files += post-process/memory/qmmf_postproc_memory_pool.cc
source_files += post-process/pipe/qmmf_postproc_pipe.cc
source_files += post-process/common/qmmf_postproc_thread.cc
source_files += qmmf_camera_rescaler.cc


# Build qmmf recorder service library
# libqmmf_recorder_service.so
lib_LTLIBRARIES = libqmmf_recorder_service.la

libqmmf_recorder_service_la_SOURCES = $(source_files)

libqmmf_recorder_service_la_CPPFLAGS = -I$(top_srcdir)/include
libqmmf_recorder_service_la_CPPFLAGS += -I$(top_srcdir)/
libqmmf_recorder_service_la_CPPFLAGS += -I$(CAMERAHAL_CFLAGS)
libqmmf_recorder_service_la_CPPFLAGS += -DHFR_THRESHOLD=30
libqmmf_recorder_service_la_CPPFLAGS += -DAEC_WAIT_TIMEOUT=500000000

libqmmf_recorder_service_la_LDFLAGS  = -llog -lbinder -lutils -lcutils
libqmmf_recorder_service_la_LDFLAGS  += -lcamera_client -lcamera_metadata
libqmmf_recorder_service_la_LDFLAGS  += -lhardware

libqmmf_recorder_service_la_LIBADD   = ../client/libqmmf_recorder_client.la
libqmmf_recorder_service_la_LIBADD   += ../../../common/utils/libqmmf_utils.la
libqmmf_recorder_service_la_LIBADD   += ../../../common/cameraadaptor/libcamera_adaptor.la
libqmmf_recorder_service_la_LIBADD   += ../../../common/codecadaptor/libcodec_adaptor.la
libqmmf_recorder_service_la_LIBADD   += ../../../common/audio/src/client/libqmmf_audio_client.la
libqmmf_recorder_service_la_LIBADD   += ../../../common/overlay/liboverlay.la
libqmmf_recorder_service_la_LIBADD   += ../../../common/exif-generator/libexif_generator.la
libqmmf_recorder_service_la_LIBADD   += ../../../common/jpeg-encoder/libqmmf_common_jpeg_encoder.la
libqmmf_recorder_service_la_LIBADD   += ../../../common/resizer-neon/libqmmf_common_resizer_neon.la
libqmmf_recorder_service_la_LIBADD   += ../../../common/resizer-c2d/libqmmf_common_resizer_c2d.la
libqmmf_recorder_service_la_LIBADD   += ../../../common/resizer-fastCV/libqmmf_common_resizer_fastcv.la
libqmmf_recorder_service_la_LIBADD   += ../../../display/src/client/libqmmf_display_client.la
libqmmf_recorder_service_la_LIBADD   += ../service/post-process/modules/camera-hal-reproc/libqmmf_camera_hal_reproc.la
libqmmf_recorder_service_la_LIBADD   += ../service/post-process/modules/jpeg-encoder/libqmmf_jpeg.la
libqmmf_recorder_service_la_LIBADD   += ../service/post-process/modules/test/libqmmf_postproc_test.la
libqmmf_recorder_service_la_LIBADD   += ../service/post-process/modules/frame-skip/libqmmf_postproc_frame_skip.la
libqmmf_recorder_service_la_LIBADD   += ../service/post-process/modules/algo/libqmmf_postproc_algo.la
