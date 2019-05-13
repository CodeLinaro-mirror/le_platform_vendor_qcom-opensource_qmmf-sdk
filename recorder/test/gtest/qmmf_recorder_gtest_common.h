/*
* Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are
* met:
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above
*       copyright notice, this list of conditions and the following
*       disclaimer in the documentation and/or other materials provided
*       with the distribution.
*     * Neither the name of The Linux Foundation nor the names of its
*       contributors may be used to endorse or promote products derived
*       from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
* WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
* ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#pragma once

#include <fcntl.h>
#include <dirent.h>
#include <functional>
#include <gtest/gtest.h>
#include <vector>
#include <map>
#include <mutex>
#include <sys/time.h>
#include <chrono>
#include <condition_variable>
#include <cutils/properties.h>
#include <random>
#include <fstream>
#include <json/json.h>
//#include <system/graphics.h>

#include <qmmf-sdk/qmmf_queue.h>
#include <qmmf-sdk/qmmf_display.h>
#include <qmmf-sdk/qmmf_display_params.h>
#include <qmmf-sdk/qmmf_recorder.h>
#include <qmmf-sdk/qmmf_recorder_params.h>
#include <qmmf-sdk/qmmf_recorder_extra_param_tags.h>
#include "common/utils/qmmf_log.h"
#include "qmmf_memory_interface.h"

#define DUMP_META_PATH "/data/misc/qmmf/param.dump"
#define OVERLAY_TEST_FILE "/data/misc/qmmf/overlay_test.rgba"

#ifdef USE_SURFACEFLINGER
#include <sys/mman.h>
#include <android/native_window.h>
#endif

#if USE_SKIA
#include <SkCanvas.h>
#include <SkString.h>
#elif USE_CAIRO
#include <cairo/cairo.h>
#endif

#ifdef QCAMERA3_TAG_LOCAL_COPY
#include <camera/VendorTagDescriptor.h>
#endif

#ifdef USE_SURFACEFLINGER
#include <ui/DisplayInfo.h>
#include <gui/Surface.h>
#include <gui/SurfaceComposerClient.h>
#include <gui/ISurfaceComposer.h>
#endif

#ifdef QCAMERA3_TAG_LOCAL_COPY
#include "common/utils/qmmf_common_utils.h"
#else
#include <QCamera3VendorTags.h>
#endif  // QCAMERA3_TAG_LOCAL_COPY

//#define DEBUG
#define TEST_INFO(fmt, args...)  ALOGD(fmt, ##args)
#define TEST_ERROR(fmt, args...) ALOGE(fmt, ##args)
#define TEST_WARN(fmt, args...) ALOGW(fmt, ##args)
#ifdef DEBUG
#define TEST_DBG  TEST_INFO
#else
#define TEST_DBG(...) ((void)0)
#endif

using namespace qmmf;
using namespace recorder;
using namespace overlay;
using namespace android;
using ::qmmf::display::DisplayEventType;
using ::qmmf::display::DisplayType;
using ::qmmf::display::Display;
using ::qmmf::display::DisplayCb;
using ::qmmf::display::SurfaceBuffer;
using ::qmmf::display::SurfaceParam;
using ::qmmf::display::SurfaceConfig;
using ::qmmf::display::SurfaceBlending;
using ::qmmf::display::SurfaceFormat;

static const uint32_t kZslWidth      = 1920;
static const uint32_t kZslHeight     = 1080;
static const uint32_t kZslQDepth     = 10;

#if USE_SKIA
static const uint32_t kColorRed        = 0xFFFF0000;
static const uint32_t kColorDarkGray   = 0x202020FF;
static const uint32_t kColorYellow     = 0xFFFF00FF;
static const uint32_t kColorBlue       = 0x0000CCFF;
static const uint32_t kColorWhilte     = 0xFFFFFFFF;
static const uint32_t kColorOrange     = 0xFF8000FF;
static const uint32_t kColorLightGreen = 0x33CC00FF;
static const uint32_t kColorLightBlue  = 0x189BF2FF;
#elif USE_CAIRO
static const uint32_t kColorRed        = 0xFF0000FF;
static const uint32_t kColorDarkGray   = 0x202020FF;
static const uint32_t kColorYellow     = 0xFFFF00FF;
static const uint32_t kColorBlue       = 0x0000CCFF;
static const uint32_t kColorWhilte     = 0xFFFFFFFF;
static const uint32_t kColorOrange     = 0xFF8000FF;
static const uint32_t kColorLightGreen = 0x33CC00FF;
static const uint32_t kColorLightBlue  = 0x189BF2FF;
#endif


#define TEXT_SIZE                 40
#define DATETIME_PIXEL_SIZE       30
#define DATETIME_TEXT_BUF_WIDTH   192
#define DATETIME_TEXT_BUF_HEIGHT  108
#define FHD_1080p_STREAM_WIDTH    1920
#define FHD_1080p_STREAM_HEIGHT   1080
#define MAX_EXP_TABLE_KNEES       50
#define MAX_DUMP_SIZE             4294967295 //4GB

static const uint32_t kBitRate4k30    = 45000000;
static const uint32_t kBitRate1440p30 = 25000000;
static const uint32_t kBitRate1440p60 = 45000000;
static const uint32_t kBitRate960p90  = 45000000;
static const uint32_t kBitRate480p    = 4000000;
static const uint32_t kBitRate100Mbps = 100000000;
static const uint32_t kBitRate10Mbps  = 10000000;

template<class T>
struct Rect {
  T left;
  T top;
  T width;
  T height;
};

struct FaceInfo {
  uint32_t fd_stream_height;
  uint32_t fd_stream_width;
  std::vector<Rect<uint32_t>> face_rect;
};

#define DEFAULT_YUV_DUMP_FREQ       "200"
#define DEFAULT_ITERATIONS          "50"
#define DEFAULT_BURST_COUNT         "30"
#define IMAGE_QUALITY               "95"

// Default recording duration is 2 minutes i.e. 2 * 60 seconds
#define DEFAULT_RECORD_DURATION     "120"

// Prop to enable YUV data dumping from YUV track
#define PROP_DUMP_YUV_FRAMES        "persist.qmmf.rec.gtest.dumpyuv"
// Prop to enable encoded bitstream data dumping
#define PROP_DUMP_BITSTREAM         "persist.qmmf.rec.gtest.dumpstrm"
// Prop to enable JPEG (BLOB) dumping
#define PROP_DUMP_JPEG              "persist.qmmf.rec.gtest.dumpjpeg"
// Prop to enable RAW Snapshot dumping
#define PROP_DUMP_RAW               "persist.qmmf.rec.gtest.dumpraw"
// Prop to set frequency of YUV data dumping
#define PROP_DUMP_YUV_FREQ          "persist.qmmf.rec.gtest.dumpfreq"
// Prop to set no of iterations
#define PROP_N_ITERATIONS           "persist.qmmf.rec.gtest.iter"
// Prop to set camera id
#define PROP_CAMERA_ID              "persist.qmmf.rec.gtest.cameraid"
// Prop to set recording duration in seconds
#define PROP_RECORD_DURATION        "persist.qmmf.rec.gtest.recdur"
// Prop to enable JPEG thumbnail dumping
#define PROP_DUMP_THUMBNAIL         "persist.qmmf.rec.gtest.thumb"
// Prop to set Burst snapshot count
#define PROP_BURST_N_IMAGES         "persist.qmmf.rec.gtest.burstcnt"

// Prop to set Track Resolutions and FPS
#define PROP_TRACK1_WIDTH           "persist.qmmf.rec.gtest.t1.w"
#define PROP_TRACK1_HEIGHT          "persist.qmmf.rec.gtest.t1.h"
#define PROP_TRACK1_FPS             "persist.qmmf.rec.gtest.t1.fps"
#define PROP_TRACK2_WIDTH           "persist.qmmf.rec.gtest.t2.w"
#define PROP_TRACK2_HEIGHT          "persist.qmmf.rec.gtest.t2.h"
#define PROP_TRACK2_FPS             "persist.qmmf.rec.gtest.t2.fps"
// Prop to update Camera Parameters: SHDR and TNR
#define PROP_CAM_PARAMS1            "persist.qmmf.rec.gtest.cam.par1"
#define PROP_CAM_PARAMS2            "persist.qmmf.rec.gtest.cam.par2"
// Prop to determine whether to create or delete session
#define PROP_TRACK1_DELETE          "persist.qmmf.rec.gtest.t1.del"
#define PROP_SESSION2_CREATE        "persist.qmmf.rec.gtest.s2.creat"
// Prop to set JPEG Quality
#define PROP_JPEG_QUALITY           "persist.qmmf.rec.gtest.jpegq"
// Prop to set CDS sensitivity threshold
#define PROP_CDS_THRESHOLD          "persist.qmmf.rec.gtest.cdsth"
// Prop to enable default EIS margins
#define PROP_DEFAULT_EIS_MARGINS    "persist.qmmf.rec.gtest.eis.dflt"
// Prop to enable/disable display usage
#define PROP_TOGGLE_DISPLAY_USAGE   "persist.qmmf.rec.gtest.display"
// Prop to enable/disable overlay usage
#define PROP_TOGGLE_OVERLAY_USAGE   "persist.qmmf.rec.gtest.overlay"
// Prop to enable/disable ubwc support in qmmf
#define PROP_UBWC_STREAM_ENABLE     "persist.qmmf.ubwcstream.enable"
// Prop to enable debugging frames
#define PROP_FRAME_DEBUG            "persist.qmmf.rec.gtest.frm.dbg"
// Prop to set force sensor mode config file
#define PROP_SENSOR_CONFIG_FILE     "persist.qmmf.sensor.mode.file"
// Prop to measure SOF latency
#define PROP_MEASURE_SOF_LATENCY    "persist.qmmf.rec.gtest.sof.ts"

#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif

/*
* frame timestamps are not always accurate to 1/fps sec
* due to interrupt latencies
* variance of 5% is considered
*/
#define FRAME_TIMESTAMP_VARIANCE (0.05f)

#ifdef QCAMERA3_TAG_LOCAL_COPY
enum ISOModes : int64_t {
  kISOModeAuto = 0,
  kISOModeDeblur,
  kISOMode100,
  kISOMode200,
  kISOMode400,
  kISOMode800,
  kISOMode1600,
  kISOMode3200,
  kISOModeEnd
};

enum AWbModes : uint8_t {
  kAWBModeOff,
  kAWBModeAuto,
  kAWBModeIncandescent,
  kAWBModeFluorescent,
  kAWBModeWarmFluorescent,
  kAWBModeDaylight,
  kAWBModeCloudyDaylight,
  kAWBModeTwilight,
  kAWBModeShade,
  kAWBModeEnd
};

static const int32_t MWBColorTemperatures[] = {0, 2300, 2800, 3200, 4000,
                                               4500, 5500, 6000, 6500};

#endif

typedef struct StreamDumpInfo {
  VideoFormat   format;
  uint32_t      session_id;
  uint32_t      track_id;
  uint32_t      width;
  uint32_t      height;
} StreamDumpInfo;

typedef struct SplitFileInfo {
  struct StreamDumpInfo streaminfo;
  time_t                timestamp;
  uint32_t              part_number;
  BufferDescriptor*     header;
  int32_t               file_fd;
} SplitFileInfo;

struct RGBAValues {
  double red;
  double green;
  double blue;
  double alpha;
};

typedef struct TriggerParams {
  float start;
  float end;
  int32_t fog_p;
} TriggerParams;

typedef struct FogSceneDetectionParams {
  TriggerParams dnr_trigger[3];  // [0]: flat_scene, [1]: fog_scene, [2]:
                                 // normal_scene, range: 0.0 - 8.0 EV
  TriggerParams lux_trigger[3];  // [0]: daylight, [1]: normal light, [2]: low
                                 // light, range: 0.0 - 1000.0 lux index
  TriggerParams cct_trigger[4];  // [0]: low CCT, [1]: indoor/outdoor CCT, [2]:
                                 // outdoor/fog CCT, [3]: high CCT
} FOG_SCENE_DETECTION_PARAMS;

typedef struct DeFogTable {
  uint8_t enable;
  int32_t algo_type;
  int32_t algo_decision_mode;
  float strength;
  float strength_range[2];
  int32_t convergence_speed;
  int32_t convergence_speed_range[2];
  float lp_color_comp_gain;
  float lp_color_comp_gain_range[2];
  uint8_t abc_en;
  uint8_t acc_en;
  uint8_t afsd_en;
  uint8_t afsd_2a_en;
  int32_t defog_dark_thres;
  int32_t defog_dark_thres_range[2];
  int32_t defog_bright_thres;
  int32_t defog_bright_thres_range[2];
  float abc_gain;
  float abc_gain_range[2];
  float acc_max_dark_str;
  float acc_max_dark_str_range[2];
  float acc_max_bright_str;
  float acc_max_bright_str_range[2];
  int32_t dark_limit;
  int32_t dark_limit_range[2];
  int32_t bright_limit;
  int32_t bright_limit_range[2];
  int32_t dark_preserve;
  int32_t dark_preserve_range[2];
  int32_t bright_preserve;
  int32_t bright_preserve_range[2];
  float dnr_trigparam_start_range[2];
  float dnr_trigparam_end_range[2];
  int dnr_trigparam_fog_range[2];
  float lux_trigparam_start_range[2];
  float lux_trigparam_end_range[2];
  int lux_trigparam_fog_range[2];
  float cct_trigparam_start_range[2];
  float cct_trigparam_end_range[2];
  int cct_trigparam_fog_range[2];
  FOG_SCENE_DETECTION_PARAMS trig_params;

  DeFogTable() {
    enable = 1;
    algo_type = 0;
    algo_decision_mode = 0;
    strength = 1;
    memset(strength_range, 0, sizeof(strength_range));
    convergence_speed = 10;
    memset(convergence_speed_range, 0, sizeof(convergence_speed_range));
    lp_color_comp_gain = 1.0;
    memset(lp_color_comp_gain_range, 0.0, sizeof(lp_color_comp_gain_range));
    abc_en = 1;
    acc_en = 1;
    afsd_en = 1;
    afsd_2a_en = 1;
    defog_dark_thres = 10;
    memset(defog_dark_thres_range, 0, sizeof(defog_dark_thres_range));
    defog_bright_thres = 40;
    memset(defog_bright_thres_range, 0, sizeof(defog_bright_thres_range));
    abc_gain = 2.0;
    memset(abc_gain_range, 0.0, sizeof(abc_gain_range));
    acc_max_dark_str = 2.0;
    memset(acc_max_dark_str_range, 0.0, sizeof(acc_max_dark_str_range));
    acc_max_bright_str = 0.5;
    memset(acc_max_bright_str_range, 0.0, sizeof(acc_max_bright_str_range));
    dark_limit = 255;
    memset(dark_limit_range, 0, sizeof(dark_limit_range));
    bright_limit = 0;
    memset(bright_limit_range, 0, sizeof(bright_limit_range));
    dark_preserve = 10;
    memset(dark_preserve_range, 0, sizeof(dark_preserve_range));
    bright_preserve = 50;
    memset(bright_preserve_range, 0, sizeof(bright_preserve_range));

    memset(dnr_trigparam_start_range, 0.0, sizeof(dnr_trigparam_start_range));
    memset(dnr_trigparam_end_range, 0.0, sizeof(dnr_trigparam_end_range));
    memset(dnr_trigparam_fog_range, 0, sizeof(dnr_trigparam_fog_range));
    memset(lux_trigparam_start_range, 0.0, sizeof(lux_trigparam_start_range));
    memset(lux_trigparam_end_range, 0.0, sizeof(lux_trigparam_end_range));
    memset(lux_trigparam_fog_range, 0, sizeof(lux_trigparam_fog_range));
    memset(cct_trigparam_start_range, 0.0, sizeof(cct_trigparam_start_range));
    memset(cct_trigparam_end_range, 0.0, sizeof(cct_trigparam_end_range));
    memset(cct_trigparam_fog_range, 0, sizeof(cct_trigparam_fog_range));
  }
} DeFogTable;

typedef struct ExposureTable {
  uint8_t is_valid;
  float sensitivity_correction_factor;
  int32_t knee_count;
  float gain_knee_entries[MAX_EXP_TABLE_KNEES];
  int64_t exp_time_knee_entries[MAX_EXP_TABLE_KNEES];
  int32_t increment_priority_knee_entries[MAX_EXP_TABLE_KNEES];
  float exp_index_knee_entries[MAX_EXP_TABLE_KNEES];
  float thres_anti_banding_min_exp_time_pct;

  ExposureTable() {
    is_valid = 0;
    sensitivity_correction_factor = 0.0;
    knee_count = 0;
    memset(gain_knee_entries, 0.0, sizeof(gain_knee_entries));
    memset(exp_time_knee_entries, 0, sizeof(exp_time_knee_entries));
    memset(increment_priority_knee_entries, 0, sizeof(increment_priority_knee_entries));
    memset(exp_index_knee_entries, 0.0, sizeof(exp_index_knee_entries));
    thres_anti_banding_min_exp_time_pct = 0.0;
  }
} ExposureTable;

#ifdef USE_SURFACEFLINGER
class SFDisplaySink
{
 public:
  SFDisplaySink(uint32_t width, uint32_t height);

  ~SFDisplaySink();

  void HandlePreviewBuffer(BufferDescriptor &buffer,
      CameraBufferMetaData &meta_data);

 private:
  int32_t CreatePreviewSurface(uint32_t width, uint32_t height);

  void DestroyPreviewSurface();

  sp<SurfaceComposerClient> surface_client_;
  sp<Surface>               preview_surface_;
  sp<SurfaceControl>        surface_control_;
};
#endif

class DumpBitStream {
 public:
  DumpBitStream() : is_enabled_(false) {};

  ~DumpBitStream() {split_file_info_.clear();}

  bool IsEnabled() {return is_enabled_;}

  bool IsUsed() {return (is_enabled_ && split_file_info_.size());}

  void Enable(const bool enable) {is_enabled_ = enable;}

  status_t SetUp(const StreamDumpInfo& dumpinfo);

  status_t Dump(const std::vector<BufferDescriptor>& buffers,
    const uint32_t &session_id, const uint32_t &track_id);

  int32_t GetFileFd(const uint32_t &session_id, const uint32_t &track_id) {
    uint8_t key_by_session_track_id = GenerateKey(session_id, track_id);
    EXPECT_TRUE(split_file_info_.count(key_by_session_track_id));
    return split_file_info_[key_by_session_track_id].file_fd;
  }

  void Close(const uint32_t &session_id, const uint32_t &track_id);

  void CloseAll();
 private:
  bool is_enabled_;

  std::map<uint8_t, SplitFileInfo> split_file_info_;

  status_t SplitFile(const uint8_t file_index);

  uint8_t GenerateKey(const uint32_t &session_id, const uint32_t &track_id) {
    return static_cast<uint8_t>(session_id << 4 | track_id);
  }

  std::string GetFileName(const SplitFileInfo& file_info);

  uint64_t GetFileSize(const int32_t file_fd) {
    off_t fsize = lseek(file_fd, 0, SEEK_END);
    EXPECT_TRUE(fsize >= 0);
    return static_cast<uint64_t>(fsize);
  }
};

class FrameTrace {
 public:
  FrameTrace(bool enable)
     : enabled_(enable), session_id_(0), track_id_(0),  track_fps_(0),
       previous_timestamp_(0), total_frames_(0), total_dropped_frames_(0) {};
  ~FrameTrace() {}

  void SetUp(uint32_t session_id, uint32_t track_id, float fps);

  void Reset();

  void BufferAvailableCb(BufferDescriptor buffer);

 private:
  bool       enabled_;

  uint32_t   session_id_;
  uint32_t   track_id_;
  float      track_fps_;

  uint64_t   previous_timestamp_;

  uint32_t   total_frames_;
  uint32_t   total_dropped_frames_;

  std::mutex lock_;

  static constexpr float kTimestampVariance = 0.05f; // 5% frame rate variance.
};

class GtestCommon : public ::testing::Test {
 public:
  GtestCommon() : recorder_(), face_bbox_active_(false), camera_error_(false) {}

  ~GtestCommon() {}

 protected:
  const ::testing::TestInfo* test_info_;

  void SetUp() override;

  void TearDown() override;

  int32_t Init();

  int32_t DeInit();

  void InitSupportedVHDRModes();
  bool IsVHDRSupported();
  void InitSupportedNRModes();
  bool IsNRSupported();

  void ClearSessions();

  void RecorderCallbackHandler(EventType event_type, void *event_data,
                               size_t event_data_size);

  void SessionCallbackHandler(EventType event_type,
                              void *event_data,
                              size_t event_data_size);

  void CameraResultCallbackHandler(uint32_t camera_id,
                                   const CameraMetadata &result);

  void VideoTrackRGBDataCb(uint32_t session_id, uint32_t track_id,
                           std::vector<BufferDescriptor> buffers,
                           std::vector<MetaData> meta_buffers);

  void VideoTrackYUVDataCb(uint32_t session_id, uint32_t track_id,
                           std::vector<BufferDescriptor> buffers,
                           std::vector<MetaData> meta_buffers);

  void VideoTrackEncDataCb(uint32_t session_id, uint32_t track_id,
                              std::vector<BufferDescriptor> &buffers,
                              std::vector<MetaData> &meta_buffers);

  void VideoTrackEventCb(uint32_t track_id, EventType event_type,
                         void *event_data, size_t event_data_size);

  void SnapshotCb(uint32_t camera_id, uint32_t image_sequence_count,
                  BufferDescriptor buffer, MetaData meta_data);

  status_t QueueVideoFrame(VideoFormat format_type,
                           const uint8_t *buffer, size_t size,
                           int64_t timestamp, AVQueue *que);

  void VideoCachedDataCb(uint32_t session_id, uint32_t track_id,
                         std::vector<BufferDescriptor> buffers,
                         std::vector<MetaData> meta_buffers,
                         VideoFormat format_type,
                         AVQueue *que);

  void ResultCallbackHandlerMatchCameraMeta(uint32_t camera_id,
                                       const CameraMetadata &result);

  void VideoTrackDataCbMatchCameraMeta(uint32_t session_id, uint32_t track_id,
                                       std::vector<BufferDescriptor> buffers,
                                       std::vector<MetaData> meta_buffers);

  status_t DumpQueue(AVQueue *queue, int32_t file_fd);

  status_t DumpThumbnail(BufferDescriptor buffer,
                         const CameraBufferMetaData& meta_data,
                         uint32_t image_sequence_count,
                         uint64_t tv_ms);

  status_t SetCameraFocalLength(const float focal_length);

  void RemoveSpaces(std::string &str);

  void TokenizeString(std::string const &str, const char delim,
                      std::vector<std::string> &out);

  status_t ListFilesFromDir(std::string dir_path,
                            std::string name_starts_with,
                            std::string extension,
                            std::vector<std::string> &files_list);

  status_t PopulateDeFogTables(std::vector<DeFogTable> &defog_tables);

  status_t PopulateExpTables(std::vector<ExposureTable> &exp_tables);

  int32_t FindSensorModeIndex(const std::string& name_of_file,
                              const std:: string& mode_index);

  status_t ReadAndParseJsonFile(const std::string &input_file,
                                Json::Value &value);

  template <typename TItem>
  void GetValue(const Json::Value &v, const std::string &field_name,
                TItem &item);

#ifdef CAM_ARCH_V2
  bool VendorTagSupported(const String8& name, const String8& section,
                          uint32_t* tag_id);

  bool VendorTagExistsInMeta(const CameraMetadata& meta, const String8& name,
                             const String8& section, uint32_t* tag_id);

  void CreatePrivacyMaskOverlay(const uint32_t& video_track_id,
                                const int32_t& width, const int32_t& height,
                                uint32_t* mask_id);

  void DestroyPrivacyMaskOverlay (const uint32_t& video_track_id,
                                  const uint32_t& mask_id);
#endif

  Recorder              recorder_;
  uint32_t              camera_id_;
  uint32_t              iteration_count_;
  std::vector<uint32_t> camera_ids_;
  CameraStartParam      camera_start_params_;
  RecorderCb            recorder_status_cb_;
  std::map <uint32_t , std::vector<uint32_t> > sessions_;
  std::map<uint32_t,uint32_t> track_frame_count_map_;
  static const std::string    kQmmfFolderPath;

  void ParseFaceInfo(const android::CameraMetadata &res,
                     struct FaceInfo &info);

  void ApplyFaceOveralyOnStream(struct FaceInfo &info);

  static bool ValidateResFromStreamConfigs(const CameraMetadata& meta,
                                            const uint32_t width,
                                            const uint32_t height);

  static bool GetMinResFromStreamConfigs(const CameraMetadata& meta,
                                          uint32_t &width,
                                          uint32_t &height);

  static bool ValidateResFromProcessedSizes(const CameraMetadata& meta,
                                            const uint32_t width,
                                            const uint32_t height);

  static bool ValidateResFromJpegSizes(const CameraMetadata& meta,
                                        const uint32_t width,
                                        const uint32_t height);

  static bool ValidateResFromRawSizes(const CameraMetadata& meta,
                                      const uint32_t width,
                                      const uint32_t height);

  static bool GetMaxSupportedCameraRes(const CameraMetadata& meta,
                                      uint32_t &width, uint32_t &height,
                                const int32_t format = HAL_PIXEL_FORMAT_RAW10);

  static bool GetMinSupportedCameraRes(const CameraMetadata& meta,
                                        uint32_t &width,
                                        uint32_t &height);

  status_t DrawOverlay(void *data, int32_t width, int32_t height);

  void ExtractColorValues(uint32_t hex_color, RGBAValues* color);

  void ClearSurface();

  status_t FillCropMetadata(CameraMetadata& meta, int32_t sensor_mode_w,
                            int32_t sensor_mode_h, int32_t crop_x,
                            int32_t crop_y, int32_t crop_w, int32_t crop_h);

  SessionCb CreateSessionStatusCb() {
    SessionCb session_status_cb;
    session_status_cb.event_cb =
        [this] (EventType event_type, void *event_data,
                size_t event_data_size) -> void {
        SessionCallbackHandler(event_type,
        event_data, event_data_size); };
    return session_status_cb;
  }

#ifndef DISABLE_DISPLAY
  void DisplayCallbackHandler(DisplayEventType event_type, void *event_data,
                              size_t event_data_size);

  void DisplayVSyncHandler(int64_t time_stamp);

  status_t StartDisplay(DisplayType display_type,
                     uint32_t src_width, uint32_t src_height,
                     uint32_t dst_width, uint32_t dst_height);

  status_t StopDisplay(DisplayType display_type);

  status_t PushFrameToDisplay(BufferDescriptor &buffer,
                              CameraBufferMetaData &meta_data);

  int32_t DequeueGfxSurfaceBuffer();

  int32_t QueueGfxSurfaceBuffer();
#endif

  std::vector<uint32_t> face_bbox_id_;
  bool face_bbox_active_;
  uint32_t face_track_id_;
  struct FaceInfo face_info_;
  std::mutex face_overlay_lock_;
#if USE_SKIA
  SkCanvas*            canvas_;
#elif USE_CAIRO
  cairo_surface_t*     cr_surface_;
  cairo_t*             cr_context_;
#endif

  typedef std::vector<uint8_t> nr_modes_;
  typedef std::vector<int32_t> vhdr_modes_;
  CameraMetadata       static_info_;
  nr_modes_            supported_nr_modes_;
  vhdr_modes_          supported_hdr_modes_;

  typedef std::tuple<BufferDescriptor, CameraMetadata, uint32_t, uint32_t>
      BufferMetaDataTuple;
  std::map <uint32_t, BufferMetaDataTuple > buffer_metadata_map_;
  std::mutex buffer_metadata_lock_;

  DumpBitStream         dump_bitstream_;
  bool                  is_dump_jpeg_enabled_;
  bool                  is_dump_raw_enabled_;
  bool                  is_dump_yuv_enabled_;
  bool                  is_dump_thumb_enabled_;
  uint32_t              dump_yuv_freq_;
  uint32_t              record_duration_;
  uint32_t              burst_image_count_;
  uint32_t              default_jpeg_quality_;
  int32_t               default_cds_threshold_;
  std::mutex            error_lock_;
  bool                  camera_error_;
  bool                  default_eis_margins_;
  bool                  is_apply_overlay_;
  bool                  is_frame_debug_enabled_;
  std::string           sensor_mode_file_name_;

#ifndef DISABLE_DISPLAY
  bool                  use_display_;
  bool                  display_started_;
  Display               *display_;
  uint32_t              surface_id_;
  SurfaceParam          surface_param_;
  SurfaceBuffer         surface_buffer_;
  SurfaceConfig         surface_config_;

  FILE                  *gfx_file;
  bool                  enable_gfx_;
  uint32_t              gfx_surface_id_;
  SurfaceParam          gfx_surface_param_;
  SurfaceBuffer         gfx_surface_buffer_;
  SurfaceConfig         gfx_surface_config_;
#endif

  bool                  ubwc_stream_enable_;
  bool                  enable_sof_latency_;

#ifdef QCAMERA3_TAG_LOCAL_COPY
  sp<VendorTagDescriptor> vendor_tag_desc_;
#endif

  struct TestEventWait {
    std::condition_variable signal_;
    std::mutex mutex_;
    bool done_;
    uint32_t cnt_;
    uint32_t wait_sec_;

    TestEventWait() : signal_(), mutex_(), done_(false), cnt_(1), wait_sec_(2) {
    }

    void Done() {
          std::unique_lock<std::mutex> lock(mutex_);
          if (!(--cnt_)) {
            done_ = true;
            signal_.notify_one();
         }
    }

    void Reset(const uint32_t cnt, const uint32_t wait_sec = 2) {
      std::unique_lock<std::mutex> lock(mutex_);
      done_ = false;
      cnt_ = cnt;
      wait_sec_ = wait_sec;
    }

    status_t Wait() {
      std::unique_lock<std::mutex> lock(mutex_);
      while (!done_) {
        auto status = signal_.wait_for(lock,
                                       std::chrono::seconds(wait_sec_ * cnt_));
        if (status != std::cv_status::no_timeout) {
          return TIMED_OUT;
        }
      }
      return NO_ERROR;
    }
  } test_wait_;
};

