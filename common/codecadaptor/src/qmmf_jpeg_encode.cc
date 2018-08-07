/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *     Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *     Neither the name of The Linux Foundation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.

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

#define LOG_TAG "AVCodecJpegEncode"

#include <atomic>
#include <cmath>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <dlfcn.h>
#include <fcntl.h>

#include <utils/Errors.h>
#include <utils/RefBase.h>
#include <cutils/properties.h>
#include <hardware/camera3.h>
#include <qcom/display/gralloc_priv.h>
#include <linux/msm_ion.h>
#include <media/hardware/HardwareAPI.h>

#include "common/codecadaptor/src/qmmf_avcodec_common.h"
#include "common/codecadaptor/src/qmmf_jpeg_encode.h"
#include "common/jpeg-encoder/qmmf_jpeg_encoder.h"
#include "common/utils/qmmf_log.h"

namespace qmmf {
namespace avcodec {
using std::string;
using std::vector;
using std::shared_ptr;
using namespace android;

JPEGEncoder::JPEGEncoder()
    : stop_jpeg_(false), job_result_size_(0),
      jpeg_encoder_(nullptr) {

  // Set Default Values
  jpeg_encoder_ = JpegEncoder::getInstance();
  jpeg_params_.image_quality = kDefaultJPEGQuality;
  thumbnail_width_ = kDefaultThumbnailWidth;
  thumbnail_height_ = kDefaultThumbnailHeight;
  thumbnail_quality_ = kDefaultThumbnailQuality;
  enable_thumbnail_ = false;
}

JPEGEncoder::~JPEGEncoder() {
  auto ret = jpeg_encoder_->DeInit();
  if (ret != 0) {
    QMMF_ERROR("%s: failed to inint Jpeg Encoder", __func__);
  }

  JpegEncoder::releaseInstance();
  jpeg_encoder_ = nullptr;

}

void JPEGEncoder::FreeMappedBuffers() {
  for (auto it : input_buffers_map_) {
    munmap(static_cast<void *>(it.second.vaddr),
           static_cast<size_t>(it.second.size));
  }
  input_buffers_map_.clear();
}

status_t JPEGEncoder::Encode(const snapshot_info &in_buffer, size_t &jpeg_size) {

  if (in_buffer.img_in_buf.data == nullptr ||
      in_buffer.img_out_buf.data == nullptr) {
    QMMF_ERROR("%s can't pass nullptr plane pointer", __func__);
    return BAD_VALUE;
  }

  jpeg_params_.thumbnail_data.clear();

  thumbnail_width_ = kDefaultThumbnailWidth;
  thumbnail_height_ = kDefaultThumbnailHeight;
  thumbnail_quality_ = kDefaultThumbnailQuality;
  if (enable_thumbnail_) {
    JpegEncoder::jpeg_thumbnail jpeg_thumbnail(thumbnail_width_,
                                               thumbnail_height_,
                                               thumbnail_quality_);
    jpeg_params_.thumbnail_data.push_back(jpeg_thumbnail);
  }

  jpeg_size = 0;
  jpeg_params_.img_data[0] = static_cast<uint8_t*>(in_buffer.img_in_buf.data);
  jpeg_params_.out_data[0] = static_cast<uint8_t*>(in_buffer.img_out_buf.data);
  jpeg_params_.source_info.format = in_buffer.format;
  jpeg_params_.source_info.num_planes = 1;
  jpeg_params_.source_info.plane_info[0].stride = in_buffer.stride;
  jpeg_params_.source_info.plane_info[0].scanline = in_buffer.scanline;
  jpeg_params_.source_info.plane_info[0].width = in_buffer.width;
  jpeg_params_.source_info.plane_info[0].height = in_buffer.height;
  jpeg_params_.source_info.plane_info[0].offset = 0;
  uint32_t size = in_buffer.stride * in_buffer.scanline;
  jpeg_params_.source_info.plane_info[0].size = 3 * size / 2;

  auto ret = jpeg_encoder_->Encode(jpeg_params_, jpeg_size);
  if (ret != 0) {
    QMMF_ERROR("%s: Jpeg Encode fails", __func__);
  }

  if (0 == jpeg_size) {
    QMMF_ERROR("%s: JPEG size is 0!", __func__);
  }
  return ret;
}

bool JPEGEncoder::IsInputStop() {
  std::lock_guard<std::mutex> l(stop_jpeg_mutex_);
  return stop_jpeg_;
}

void *JPEGEncoder::JpegEncodeThread(void *arg) {
  status_t ret = 0;
  JPEGEncoder *jpeg_encode = static_cast<JPEGEncoder *>(arg);
  BufferDescriptor buffer_in, buffer_out;
  snapshot_info img_buffer;
  bool thread_stop = false;

  // The purpose of this function is to Handle JPEG Encode.
  // Steps :
  // 1.Take input stream buffer.
  // 2.Map the buffer to current process.
  // 3.For mapping, caching has been enabled.
  // 4.Take the output stream buffer.
  // 5.Send the buffers for Jpeg Encode
  // 6.Stop logic

  while (thread_stop != true) {
    if (jpeg_encode->IsInputStop()) {
      thread_stop = true;
      break;
    }
    memset(&buffer_in, 0x0, sizeof(buffer_in));
    // Get a YUV Buffer from Track source
    ret = jpeg_encode->getInputBufferSource()->GetBuffer(buffer_in, nullptr);
    if (ret != 0) {
      QMMF_ERROR("%s InputSource Read failed", __func__);
      thread_stop = true;
    }

    buffer_handle_t native_handle_in;
    memset(&native_handle_in, 0x0, sizeof native_handle_in);
    native_handle_in = static_cast<buffer_handle_t>(buffer_in.data);

    // Get a Empty Buffer from Output Buffer source
    memset(&buffer_out, 0x0, sizeof(buffer_out));
    ret = jpeg_encode->getOutputBufferSource()->GetBuffer(buffer_out, nullptr);
    assert(buffer_out.data != nullptr);

    void *buf_vaaddr = nullptr;
    // We need to map input buffers.
    // Before mapping we are checking whether that buffer is already mapped.
    // If yes then get the vaddr.
    // If not then do mmap and add the mapping to map.
    auto it = jpeg_encode->input_buffers_map_.find(buffer_in.fd);
    if (it != jpeg_encode->input_buffers_map_.end()) {
      buf_vaaddr = it->second.vaddr;
    } else {
      buf_vaaddr = mmap(nullptr, buffer_in.size, PROT_READ | PROT_WRITE,
                        MAP_SHARED, native_handle_in->data[0], 0);
      if (buf_vaaddr == MAP_FAILED) {
        QMMF_ERROR("%s MAP_FAILED for input", __func__);
        break;
      }
      // Add Entry to map
      mapped_buffer_info instance = {buf_vaaddr, buffer_in.size};
      jpeg_encode->input_buffers_map_.emplace(buffer_in.fd, instance);
    }

    // Set input-output buffer dimension
    size_t jpeg_size = 0;
    memset(&img_buffer, 0x0, sizeof(img_buffer));
    img_buffer.img_in_buf.data = buf_vaaddr;
    img_buffer.img_out_buf.data = buffer_out.data;
    img_buffer.stride = VENUS_Y_STRIDE(
        COLOR_FMT_NV12, jpeg_encode->codec_params_.video_enc_param.width);
    img_buffer.scanline = VENUS_Y_SCANLINES(
        COLOR_FMT_NV12, jpeg_encode->codec_params_.video_enc_param.height);
    img_buffer.width = jpeg_encode->codec_params_.video_enc_param.width;
    img_buffer.height = jpeg_encode->codec_params_.video_enc_param.height;
    img_buffer.format = BufferFormat::kNV12;

    // Send buffer for encode
    jpeg_encode->Encode(img_buffer, jpeg_size);
    if (0 == jpeg_size) {
      QMMF_ERROR("%s: JPEG size is 0!", __func__);
    } else {
      buffer_out.timestamp = buffer_in.timestamp;
      buffer_out.size = jpeg_size;
      camera3_jpeg_blob_t header;
      header.jpeg_blob_id = CAMERA3_JPEG_BLOB_ID;
      header.jpeg_size = jpeg_size;
      // The Jpeg header must be appended at the end of the allocated buffer.
      uint32_t offset = jpeg_size - sizeof(header);
      uintptr_t data = reinterpret_cast<uintptr_t>(buffer_out.data);
      void *vaddr = reinterpret_cast<void *>(data + offset);
      memcpy(vaddr, &header, sizeof(header));
    }

    // send back the buffer to Track Source
    jpeg_encode->getInputBufferSource()->ReturnBuffer(buffer_in, nullptr);
    jpeg_encode->getOutputBufferSource()->ReturnBuffer(buffer_out, nullptr);
  }

  // Stop Logic
  if (thread_stop == true) {
    jpeg_encode->FreeMappedBuffers();
    CodecPortStatus status = CodecPortStatus::kPortStop;
    jpeg_encode->getInputBufferSource()->NotifyPortEvent(
        PortEventType::kPortStatus, static_cast<void *>(&status));
    status = CodecPortStatus::kPortIdle;
    jpeg_encode->getInputBufferSource()->NotifyPortEvent(
        PortEventType::kPortStatus, static_cast<void *>(&status));
  }
  return nullptr;
}

status_t JPEGEncoder::GetComponentName(CodecMimeType mime_type,
                                       uint32_t *num_comps,
                                       vector<string> &comp_names) {
  return 0;
}

status_t JPEGEncoder::ConfigureCodec(CodecMimeType codec_type,
                                     CodecParam &codec_param,
                                     const AVCodecCb& avcodec_cb,
                                     string comp_name) {
  codec_params_ = codec_param;
  format_type_ = CodecType::kImageEncoder;
  jpeg_quality_ = codec_params_.video_enc_param.codec_param.jpeg.quality;

  // thumbnail settings
  enable_thumbnail_ =
      codec_params_.video_enc_param.codec_param.jpeg.enable_thumbnail;
  thumbnail_width_ =
      codec_params_.video_enc_param.codec_param.jpeg.thumbnail_width;
  thumbnail_height_ =
      codec_params_.video_enc_param.codec_param.jpeg.thumbnail_height;
  thumbnail_quality_ =
      codec_params_.video_enc_param.codec_param.jpeg.thumbnail_quality;

  auto ret = jpeg_encoder_->Init(codec_param.video_enc_param.width,
                                 codec_param.video_enc_param.height);
  if (ret != 0) {
    QMMF_ERROR("%s: failed to inint Jpeg Encoder", __func__);
    return BAD_VALUE;
  }
  return NO_ERROR;
}

status_t JPEGEncoder::GetBufferRequirements(uint32_t port_type,
                                            uint32_t *buf_count,
                                            uint32_t *buf_size) {
  if (port_type == kPortIndexOutput) {
    *buf_count = kJPEGBufferCount;
    *buf_size =
        (codec_params_.video_enc_param.width *
         codec_params_.video_enc_param.height * 1.5);  // We need to fix this
  } else {
    // for input port not required.
  }
  QMMF_INFO("%s %s: buf count(%d), buf size(%d)", __func__,
            PORT_NAME(port_type), *buf_count, *buf_size);
  return NO_ERROR;
}

status_t JPEGEncoder::AllocateBuffer(uint32_t port_type, uint32_t buf_count,
                                     uint32_t buf_size,
                                     const shared_ptr<ICodecSource> &source,
                                     vector<BufferDescriptor> &buffer_list) {
  if (port_type == kPortIndexInput) {
    assert(source.get() != nullptr);
    input_source_ = source;
  } else {
    assert(source.get() != nullptr);
    output_source_ = source;
  }
  return NO_ERROR;
}

status_t JPEGEncoder::ReleaseBuffer() {
  input_source_ = nullptr;
  output_source_ = nullptr;
  return NO_ERROR;
}

status_t JPEGEncoder::SetParameters(CodecParamType param_type,
                                    void *codec_param, size_t param_size) {
  uint32_t* quality = nullptr;
  switch (param_type) {
    case CodecParamType::kJPEGQuality:
      quality = static_cast<uint32_t*>(codec_param);
      {
        std::lock_guard<std::mutex> lock(param_lock_);
        jpeg_quality_ = *quality;
      }
      break;
    default:
      QMMF_ERROR("%s Unknown param type", __func__);
      return -1;
  }
  return NO_ERROR;
}

status_t JPEGEncoder::GetParameters(const CodecParamType param_type,
                                    void *codec_param, size_t *param_size) {
  return NO_ERROR;
}

status_t JPEGEncoder::StartCodec() {
  std::lock_guard<std::mutex> l(stop_jpeg_mutex_);
  stop_jpeg_ = false;
  jpeg_thread_id_ = thread(JpegEncodeThread, this);
  return NO_ERROR;
}

status_t JPEGEncoder::StopCodec(bool do_flush) {
  std::lock_guard<std::mutex> l(stop_jpeg_mutex_);
  stop_jpeg_ = true;
  jpeg_thread_id_.join();
  return NO_ERROR;
}

status_t JPEGEncoder::PauseCodec() { return 0; }
status_t JPEGEncoder::ResumeCodec() { return 0; }
status_t JPEGEncoder::RegisterOutputBuffers(vector<BufferDescriptor> &list) {
  output_buffer_list_ = list;
  return NO_ERROR;
}

status_t JPEGEncoder::RegisterInputBuffers(vector<BufferDescriptor> &list) {
  input_buffer_list_ = list;
  return NO_ERROR;
}

status_t JPEGEncoder::Flush(uint32_t port_type) { return NO_ERROR; }

};  // namespace avcodec
};  // namespace qmmf
