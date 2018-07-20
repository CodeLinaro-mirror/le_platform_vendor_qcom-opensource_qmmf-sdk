/*
* Copyright (c) 2018, The Linux Foundation. All rights reserved.
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
#include <linux/msm_ion.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <condition_variable>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include <cutils/properties.h>
#include <fastcv/fastcv.h>
#include <media/msm_media_info.h>
#ifndef TARGET_USES_GBM
#include <qcom/display/gralloc_priv.h>
#else
#include "common/utils/qmmf_common_utils.h"
#endif
#include <utils/Log.h>

#include "player/test/demuxer/qmmf_demuxer_intf.h"
#include "player/test/demuxer/qmmf_demuxer_mediadata_def.h"
#include "player/test/demuxer/qmmf_demuxer_sourceport.h"
#include "qmmf-sdk/qmmf_avcodec.h"
#include "qmmf-sdk/qmmf_player_params.h"

namespace screennailapp {

enum class TrackTypes {
  kAudioVideo,
  kAudioOnly,
  kVideoOnly,
  kInvalid,
};

typedef struct ion_allocation_data IonHandleData;
typedef std::function<int32_t(const qmmf::BufferDescriptor& buffer,
                              uint32_t width, uint32_t height)> CaptureCb;

typedef struct meta_info_t {
  int tid;
  std::string filename;
} meta_info;

class ScreenNail;

class VideoDecode {
 public:
  VideoDecode(const std::string& filename, const CaptureCb& cb,
              const int32_t ion_device);
  ~VideoDecode();

  // Function VideoDecode::Init()
  // - Initialises the video Decoder:
  //  1. Create AVcodec object.
  //  2. Configure codec to decode video, fill the params like width, height.
  //  3. For the input port and output port, do the follwoing:
  //    a. Allocate Ion buffers(on the app side) on the Input port and output
  //    port
  //    b. Register the buffers with the AVCodec.
  //    c. Call the Allocate buffer of AVcodec to create OMX buffer headers
  //    needed
  //       by OMX Client.
  //    d. Add the allocated buffers for the ports to their respective buffer
  //       lists.
  //  4. If any error occurs in step 3, we release the buffers allocated in step
  //     3.a.
  int32_t Init();

  // Function VideoDecode::InitDemux()
  // - Initialises the demuxer:
  //  1. Create a Demuxer Port
  //  2. Fill the params
  //  3. Create data source
  //  4. Read Video and Audio Track data
  int32_t InitDemux();

  // Function VideoDecode::StartDecoding()
  // - Starts the Video Decoding by calling start codec function of AVCodec
  int32_t StartDecode();

  // Function VideoDecode::StopDecoding()
  // - Stops the Video Decoding by calling stop codec function of AVCodec
  int32_t StopDecode();

  // Function VideoDecode::SetStopDecode(...)
  // - Sets the stop decode variable to either true/false
  inline void SetStopDecode(const bool value) { stop_decode_ = value; }

  // Function VideoDecode::GetStopDecodeStatus(...)
  // - Gets the value from the stop decode variable
  inline bool GetStopDecodeStatus() const { return stop_decode_; }

 private:
  class InputCodecSourceImpl : public ::qmmf::avcodec::ICodecSource {
   public:
    InputCodecSourceImpl(VideoDecode* const vid);
    ~InputCodecSourceImpl();

    int32_t GetBuffer(qmmf::BufferDescriptor& stream_buffer,
                      void* client_data) override;
    int32_t ReturnBuffer(qmmf::BufferDescriptor& stream_buffer,
                         void* client_data) override;
    int32_t NotifyPortEvent(::qmmf::avcodec::PortEventType event_type,
                            void* event_data) override;

    void AddBufferList(const std::vector<qmmf::BufferDescriptor>& list);

   private:
    VideoDecode* input_video_decode_;
    bool is_first_frame_;
    std::mutex wait_for_frame_lock_;
    std::mutex input_free_buffer_vector_lock_;
    std::mutex input_occupy_buffer_map_lock_;
    std::condition_variable wait_for_frame_;
    std::vector<qmmf::BufferDescriptor> input_free_buffer_vector_;
    std::map<void*, qmmf::BufferDescriptor> input_occupy_buffer_map_;
  };  // Class InputCodecSourceImpl

  class OutputCodecSourceImpl : public ::qmmf::avcodec::ICodecSource {
   public:
    OutputCodecSourceImpl(const std::string& filename, VideoDecode* const vid,
                          const CaptureCb& cb, const uint64_t frame_number);
    ~OutputCodecSourceImpl();

    int32_t GetBuffer(qmmf::BufferDescriptor& codec_buffer,
                      void* client_data) override;
    int32_t ReturnBuffer(qmmf::BufferDescriptor& codec_buffer,
                         void* client_data) override;
    int32_t NotifyPortEvent(::qmmf::avcodec::PortEventType event_type,
                            void* event_data) override;

    void AddBufferList(const std::vector<qmmf::BufferDescriptor>& list);

   private:
    uint32_t frame_number_;
    uint32_t frame_counter_;
    CaptureCb cb_;
    VideoDecode* output_video_decode_;
    std::mutex wait_for_frame_lock_;
    std::mutex output_free_buffer_vector_lock_;
    std::mutex output_occupy_buffer_map_lock_;
    std::condition_variable wait_for_frame_;
    std::vector<qmmf::BufferDescriptor> output_free_buffer_vector_;
    std::map<int32_t, qmmf::BufferDescriptor> output_occupy_buffer_map_;
  };  // Class OutputCodecSourceImpl

  friend class OutputCodecSourceImpl;
  // Demuxer APIs
  int32_t FillCodecParams();
  int32_t CreateDataSource();
  int32_t ReadMediaInfo();
  int32_t ReadVideoTrackMediaInfo(const uint32 track_id,
                                  const FileSourceMnMediaType mn_type);

  // Function VideoDecode::Read(...)
  // - Writes the demuxed data into the buffer passed as input paramter
  int32_t Read(qmmf::BufferDescriptor* stream_buffer, const bool stat);

  // Function VideoDecode::AllocateBuffer(...)
  // - Allocates ION buffers for the respective ports(Input or Output port)
  //  1. Get the buffer requirements (size and count) from AVCodec.
  //  2. Allocate ION buffers using the ioctl calls.
  //  3. Push the buffers into the buffer list for each of the port.
  int32_t AllocateBuffer(const uint32_t index);

  // Function VideoDecode::ReleaseBuffer(...)
  // - Releases the allocated ion buffers
  //  1. Unmaps the buffer from the buffer list
  //  2. Frees the Ion allocation
  void ReleaseBuffer(const uint32_t index);

  MM_TRACK_INFOTYPE track_info_;
  CMM_MediaSourcePort* stream_port_;
  CMM_MediaDemuxInt* demux_;
  TrackTypes track_type_;

  bool stop_decode_;
  CaptureCb cb_;
  std::string input_file_;
  qmmf::avcodec::IAVCodec* vidc_avcodec_;
  int32_t ion_device_;
  uint64_t frame_number_;
  qmmf::avcodec::CodecParam create_param_;
  std::shared_ptr<InputCodecSourceImpl> input_codec_src_;
  std::shared_ptr<OutputCodecSourceImpl> output_codec_src_;
  std::vector<qmmf::BufferDescriptor> input_buffer_list_;
  std::vector<qmmf::BufferDescriptor> output_buffer_list_;

  // The maps are a mapping between the Ion FD and the Ion handle used
  // to allocate the buffers for input and output port respectively.
  std::map<int, struct ion_handle_data> input_ion_handle_data_;
  std::map<int, struct ion_handle_data> output_ion_handle_data_;
};

// Class JpegEncode
class JpegEncode {
 public:
  JpegEncode(const std::string& filename, const int32_t ion_device);
  ~JpegEncode();

  // Function JpegEncode::Init(...):
  // - Initialises the Jpegencoder:
  //  1. Create AVcodec object.
  //  2. Configure codec to encode jpeg, fill the params like width, height.
  //  3. For the input port and output port, do the follwoing:
  //    a. Allocate Ion buffer(on the app side) on the Input port and output
  //    port
  //    b. Register the buffer with the AVCodec.
  //    c. Call the Allocate buffer of JpegEncoder to register the input and
  //       output port's ICodecSource.
  //    d. Add the allocated buffers of the ports to their respective buffer
  //       lists.
  //  4. If any error occurs in step 3, we release the buffers allocated in step
  //     3.a.
  int32_t Init(const qmmf::BufferDescriptor& buffer);

  // Function JpegEncode:StartEncoding():
  // - Starts the Jpeg Encoding by calling start codec function of AVCodec
  int32_t StartEncode();

  inline void SetEncodeInit(const bool value) { init_ = value; }

  inline bool GetEncodeInitStatus() { return init_; }

 private:
  class InputCodecSourceImpl : public ::qmmf::avcodec::ICodecSource {
   public:
    InputCodecSourceImpl(const qmmf::BufferDescriptor& yuv_buf,
                         JpegEncode* const jpg);
    ~InputCodecSourceImpl();

    int32_t GetBuffer(qmmf::BufferDescriptor& stream_buffer,
                      void* client_data) override;
    int32_t ReturnBuffer(qmmf::BufferDescriptor& stream_buffer,
                         void* client_data) override;
    int32_t NotifyPortEvent(::qmmf::avcodec::PortEventType event_type,
                            void* event_data) override;

   private:
    JpegEncode* ip_jpeg_;
  };  // Class InputCodecSourceImpl

  class OutputCodecSourceImpl : public ::qmmf::avcodec::ICodecSource {
   public:
    OutputCodecSourceImpl(std::string file_name, JpegEncode* const jpg);
    ~OutputCodecSourceImpl();

    int32_t GetBuffer(qmmf::BufferDescriptor& codec_buffer,
                      void* client_data) override;
    int32_t ReturnBuffer(qmmf::BufferDescriptor& codec_buffer,
                         void* client_data) override;
    int32_t NotifyPortEvent(::qmmf::avcodec::PortEventType event_type,
                            void* event_data) override;

    // Function JpegEncode::OutputCodecSourceImpl::DumpJpeg(...)
    // - The function dumps the encoded jpeg input buffer into a jpg file
    // - Uses linux open, write system calls.
    void DumpJpeg(const qmmf::BufferDescriptor& codec_buffer);

   private:
    JpegEncode* op_jpeg_;
    std::ofstream jpg_file_;
  };  // Class OutputCodecSourceImpl

  // Function JpegEncode:AllocateBuffer(...):
  // - Allocates ION buffer for the respective ports(Input or Output port)
  //  1. Get the buffer requirements (size) from AVCodec (We allocate only one
  //     buffer on each of the ports)
  //  2. Allocate ION buffer using the ioctl calls.
  //  3. Push the buffer into the buffer list for each of the port.
  //  Note: For input port, we use native handle since the jpeg encoder uses
  //  native handle on input side.
  int32_t AllocateBuffer(const uint32_t index);

  // Function JpegEncode::ReleaseBuffer(...)
  // - Releases the allocated ion buffer
  //  1. Unmaps the buffer from the buffer list
  //  2. Frees the Ion allocation
  void ReleaseBuffer();

  bool init_;
  uint32_t width_;
  uint32_t height_;
  int32_t ion_device_;
  std::string output_file_;
  qmmf::avcodec::IAVCodec* jpeg_avcodec_;
  qmmf::avcodec::CodecParam create_param_;
  std::shared_ptr<InputCodecSourceImpl> input_codec_src_;
  std::shared_ptr<OutputCodecSourceImpl> output_codec_src_;

  // The maps are a mapping between the Ion FD and the Ion handle used
  // to allocate the buffers for input and output port respectively.
  std::map<int, struct ion_handle_data> input_ion_handle_data_;
  std::map<int, struct ion_handle_data> output_ion_handle_data_;
  std::vector<qmmf::BufferDescriptor> input_buffer_list_;
  std::vector<qmmf::BufferDescriptor> output_buffer_list_;
};

class ScreenNail {
 public:
  ScreenNail(std::string filename);
  ~ScreenNail();

  // Function ScreenNail::Init(...)
  // - Initialises the demuxer, video decoder and start decoding
  int32_t Init(meta_info* info);

  // Function ScreenNail::CheckStatus(...)
  // - Checks the status of jpeg encode start and video decode stop and
  //   start the jpeg encoding and stops the video decoding
  void CheckStatus(int tid);

  // Function ScreenNail::Capture(...)
  // - Initiates the generation of Jpeg when the buffer of the required frame
  //   number is found
  int32_t Capture(const qmmf::BufferDescriptor& buffer, uint32_t input_width,
                 uint32_t input_height);

  std::shared_ptr<VideoDecode> vidc_;
  std::shared_ptr<JpegEncode> jpeg_;
  int32_t ion_device_;

 private:
  // Function ScreenNail::InitRescalar(...)
  // - Initialises the fastcv rescalar.
  // - Sets the fastcv operation mode to Low power mode by default.(Can be
  //   configured via setprop)
  int32_t InitRescalar();

  // Function ScreenNail::Rescale(...)
  // - Rescales the input yuv buffer ands dumps in the data to output buffer
  //  1. Rescales the y component first using fcvScaleu8_v2(...) API.
  //  2. Then, rescales the uv component using fcvScaleDownMNInterleaveu8(...)
  //     API.
  int32_t Rescale(const qmmf::BufferDescriptor& in_buffer,
                  qmmf::BufferDescriptor* out_buffer);

  // Function ScreenNail::ColorConvert(...)
  // - Color converts the input yuv buffer to output rgba using
  //   fcvColorYCbCr420PseudoPlanarToRGBA8888u8(...) API
  void ColorConvert(const qmmf::BufferDescriptor& in_buffer,
                    qmmf::BufferDescriptor* out_buffer);

  // Function ScreenNail::DumpYUV(...)
  // - The function dumps the decoded and rescale yuv input buffer into a yuv
  //   file
  // - Uses c++ ofstream open, write calls.
  void DumpYUV(const qmmf::BufferDescriptor& codec_buffer);

  // Function ScreenNail::DumpRGBA(...)
  // - The function dumps the RGBA color converted  input buffer into a rgba
  //   file
  // - Uses c++ ofstream open, write calls.
  void DumpRGBA(const qmmf::BufferDescriptor& codec_buffer);

  uint32_t flag_;
  qmmf::BufferDescriptor rescale_buffer_;
  std::ofstream yuv_file_;
  std::ofstream rgba_file_;
  uint32_t input_width_;
  uint32_t input_height_;
  uint32_t rescale_width_;
  uint32_t rescale_height_;
};  // class ScreenNail
};  // namespace screennailapp