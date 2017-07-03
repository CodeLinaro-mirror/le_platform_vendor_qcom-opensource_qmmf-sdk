/*
 * Copyright (c) 2017, The Linux Foundation. All rights reserved.
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

#define TAG "CameraHalJpeg"

#include "recorder/src/service/qmmf_recorder_utils.h"
#include "recorder/src/service/qmmf_camera_context.h"

#include "qmmf_camera_hal_jpeg.h"

namespace qmmf {

namespace recorder {

CameraHalJpeg::CameraHalJpeg(IPostProc* context)
    : context_(context),
      reprocess_flag_(false),
      ready_to_start_(false),
      burst_cnt_(0) {
  QMMF_VERBOSE("%s:%s: Enter", TAG, __func__);
  QMMF_VERBOSE("%s:%s: Exit (0x%p)", TAG, __func__, this);
}

CameraHalJpeg::~CameraHalJpeg() {
  QMMF_VERBOSE("%s:%s: Enter ", TAG, __func__);
  QMMF_VERBOSE("%s:%s: Exit (0x%p)", TAG, __func__, this);
}


void CameraHalJpeg::GetInputBuffer(StreamBuffer &buffer) {
  Mutex::Autolock lock(burst_queue_lock_);
  auto iter = input_buffer_.begin();
  input_buffer_done_.push_back((*iter));
  buffer = (*iter);
  input_buffer_.erase(iter);
}

void CameraHalJpeg::ReturnInputBuffer(StreamBuffer &buffer) {
  Mutex::Autolock lock(burst_queue_lock_);
  auto iter = input_buffer_done_.begin();
  for (; iter != input_buffer_done_.end(); iter++) {
    if ((*iter).handle ==  buffer.handle) {
      (*iter).stream_id = input_stream_id_;
      auto ret = context_->ReturnStreamBuffer((*iter));
      if (NO_ERROR != ret) {
        QMMF_ERROR("%s: Failed to return input buffer: %d\n", __func__, ret);
      }
      input_buffer_done_.erase(iter);
      break;
    }
  }
}

void CameraHalJpeg::ReturnAllInputBuffers() {
  Mutex::Autolock lock(burst_queue_lock_);
  auto iter = input_buffer_done_.begin();
  for (; iter != input_buffer_done_.end(); iter++) {
    (*iter).stream_id = input_stream_id_;
    auto ret = context_->ReturnStreamBuffer((*iter));
    if (NO_ERROR != ret) {
      QMMF_ERROR("%s: Failed to return input buffer: %d\n", __func__, ret);
    }
  }
  input_buffer_done_.clear();
  input_buffer_.clear();
  burst_queue_.clear();
  input_burst_queue_.clear();
}

void CameraHalJpeg::ReprocessCallback(StreamBuffer in_buff) {
  Listener_->OnFrameReady(in_buff);

  //start next frame reprocess
  if (StartProcessing() != NO_ERROR) {
    QMMF_ERROR("%s: Failed: Wrong state. Reprocess is not started.\n",
        __func__);
  }

  // last frame
  if (++burst_cnt_ == num_images_) {
    ReturnAllInputBuffers();
    burst_cnt_= 0;
    reprocess_flag_ = false;
  }
}

status_t CameraHalJpeg::Create(const int32_t stream_id,
                                 const PostProcCreateParam& input,
                                 const PostProcCreateParam& output,
                                 const uint32_t frame_rate,
                                 const uint32_t num_images,
                                 const void* static_meta,
                                 const void* context,
                                 int32_t &out_stream_id) {
  int32_t ret = NO_ERROR;
  CameraInputStreamParameters inputStreamParams;
  CameraStreamParameters streamParams;
  int32_t stream_id_p;

  if (ready_to_start_) {
    QMMF_ERROR("%s:%s: Failed: Already configured.", TAG, __func__);
    return BAD_VALUE;
  }

  if (reprocess_flag_) {
    QMMF_ERROR("%s:%s: Failed: Wrong state.", TAG, __func__);
    return BAD_VALUE;
  }

  if (NO_ERROR != ValidateInput(*const_cast<CameraMetadata*>(
                                static_cast<const CameraMetadata*>(static_meta)),
                                input, output)){
    QMMF_ERROR("%s:%s: Failed: Wrong input parameters.", TAG, __func__);
    return BAD_VALUE;
  }

  Mutex::Autolock lock(reprocess_lock_);

  input_stream_id_ = stream_id;
  num_images_ = num_images;
  burst_cnt_ = 0;

  memset(&inputStreamParams, 0, sizeof(inputStreamParams));
  inputStreamParams.format = input.format;
  inputStreamParams.width = input.width;
  inputStreamParams.height = input.height;
  inputStreamParams.get_input_buffer = [&] (StreamBuffer &buffer)
      { GetInputBuffer(buffer); };
  inputStreamParams.return_input_buffer  = [&] (StreamBuffer &buffer)
      { ReturnInputBuffer(buffer); };
  ret = context_->CreateDeviceInputStream(inputStreamParams, &stream_id_p);
  if (NO_ERROR != ret) {
    QMMF_ERROR("%s: Failed to create input reprocess stream: %d\n",
               __func__, ret);
    return ret;
  }
  assert(stream_id_p >= 0);
  reprocess_request_.streamIds.add(stream_id_p);
  memset(&streamParams, 0, sizeof(streamParams));
  streamParams.bufferCount = num_images;
  streamParams.format = output.format;
  streamParams.width = output.width;
  streamParams.height = output.height;
  streamParams.grallocFlags = GRALLOC_USAGE_SW_READ_OFTEN;
  streamParams.cb = [&](StreamBuffer buffer) { ReprocessCallback(buffer); };
  ret = context_->CreateDeviceStream(streamParams, frame_rate, &stream_id_p);
  if (NO_ERROR != ret) {
    QMMF_ERROR("%s: Failed to create output reprocess stream: %d\n",
               __func__, ret);
    return ret;
  }
  assert(stream_id_p >= 0);
  reprocess_request_.streamIds.add(stream_id_p);
  ready_to_start_ = true;
  out_stream_id = stream_id_p;

  return ret;
}

PostProcCreateParam CameraHalJpeg::GetInput(const PostProcCreateParam &out) {
  PostProcCreateParam in = out;
  in.format = HAL_PIXEL_FORMAT_YCbCr_420_888;
  return in;
}

PostProcCreateParam CameraHalJpeg::GetOutput(const PostProcCreateParam &in) {
  PostProcCreateParam out = in;
  out.format = HAL_PIXEL_FORMAT_BLOB;
  return out;
}

status_t CameraHalJpeg::GetCapabilities(PostProcCaps &caps) {
  caps.output_buff_        = 0;
  caps.min_width_          = 160;
  caps.min_height_         = 120;
  caps.max_width_          = 5104;
  caps.max_height_         = 4092;
  caps.crop_support_       = false;
  caps.scale_support_      = true;
  caps.inplace_processing_ = false;
  caps.lib_version_        = "1.0";

  caps.in_formats_.push_back(BufferFormat::kNV12);
  caps.out_formats_.push_back(BufferFormat::kBLOB);

  return NO_ERROR;
}

status_t CameraHalJpeg::Start() {
  return NO_ERROR;
}

status_t CameraHalJpeg::Stop() {
  return NO_ERROR;
}

status_t CameraHalJpeg::StartProcessing() {
  if (!ready_to_start_) {
    return BAD_VALUE;
  }

  assert(context_ != nullptr);

  Mutex::Autolock lock(burst_queue_lock_);

  reprocess_flag_ = true;

  if (!input_burst_queue_.empty()) {
    List<BurstData>::iterator it = input_burst_queue_.begin();
    input_buffer_.push_back(it->buffer);
    reprocess_request_.metadata.clear();
    reprocess_request_.metadata.append(it->result);
    input_burst_queue_.erase(it);
  } else {
    QMMF_ERROR("%s:%s: Buffer is not ready or no more buffers", TAG, __func__);
    return NO_ERROR;
  }
  int64_t last_frame_mumber;
  auto ret = context_->SubmitRequest(reprocess_request_,
                                     false,
                                     &last_frame_mumber);
  if (ret < 0) {
    QMMF_ERROR("%s:%s: Failed to submit reprocess request.", TAG, __func__);
    return BAD_VALUE;
  }
  return NO_ERROR;
}

status_t CameraHalJpeg::Delete() {
  QMMF_VERBOSE("%s:%s: Enter ", TAG, __func__);

  assert(context_ != nullptr);

  Mutex::Autolock lock(reprocess_lock_);

  ReturnAllInputBuffers();

  if (!reprocess_request_.streamIds.isEmpty()) {
    for (auto streamId : reprocess_request_.streamIds) {
      if (NO_ERROR != context_->DeleteDeviceStream(streamId, false)) {
        QMMF_ERROR("%s: Failed to delete non-zsl snapshot stream",
                   __func__);
        return BAD_VALUE;
      }
    }
    reprocess_request_.streamIds.clear();
  }
  reprocess_flag_ = false;
  ready_to_start_ = false;
  return NO_ERROR;
}

status_t CameraHalJpeg::Configure(const std::string config_json_data) {
  return NO_ERROR;
}

status_t CameraHalJpeg::Process(
    const std::vector<StreamBuffer> &in_buffers,
    const std::vector<StreamBuffer> &out_buffers) {

  for (auto buf : in_buffers) {
    AddBuff(buf);
  }
  return NO_ERROR;
}

void CameraHalJpeg::AddBuff(StreamBuffer in_buff) {
  {
    Mutex::Autolock lock(burst_queue_lock_);
    bool append = true;
    if (!burst_queue_.empty()) {
      List<BurstData>::iterator it = burst_queue_.begin();
      List<BurstData>::iterator end = burst_queue_.end();
      while (it != end) {
        if (it->timestamp == in_buff.timestamp) {
          it->buffer = in_buff;
          input_burst_queue_.push_back(*it);
          burst_queue_.erase(it);
          append = false;
          break;
        }
        it++;
      }
    }

    if (append) {
      //Result is missing append to queue directly
      BurstData new_entry;
      new_entry.buffer = in_buff;
      new_entry.timestamp = in_buff.timestamp;
      new_entry.result.clear();
      burst_queue_.push_back(new_entry);
    }
  }

  if (input_burst_queue_.size() >= num_images_) {
    if (StartProcessing() != NO_ERROR) {
      QMMF_ERROR("%s: Failed: Wrong state. Reprocess is not started.\n",
          __func__);
    }
  }
}

status_t CameraHalJpeg::ReturnBuff(StreamBuffer &buffer) {
  QMMF_VERBOSE("%s:%s: StreamBuffer(0x%p) ts: %lld", TAG,
       __func__, buffer.handle, buffer.timestamp);
  return context_->ReturnStreamBuffer(buffer);
}

void CameraHalJpeg::AddResult(const void* result_in) {
  int64_t timestamp;
  const CaptureResult &result = *const_cast<CaptureResult*>(
                                static_cast<const CaptureResult*>(result_in));

  if (result.metadata.exists(ANDROID_SENSOR_TIMESTAMP)) {
    timestamp = result.metadata.find(ANDROID_SENSOR_TIMESTAMP).data.i64[0];
  } else {
    QMMF_ERROR("%s:%s Sensor timestamp tag missing in result!\n",
        TAG, __func__);
    return;
  }

  {
    Mutex::Autolock lock(burst_queue_lock_);
    bool append = true;
    if (!burst_queue_.empty()) {
      List<BurstData>::iterator it = burst_queue_.begin();
      List<BurstData>::iterator end = burst_queue_.end();
      while (it != end) {
        if (it->timestamp == timestamp) {
          it->result.append(result.metadata);
          input_burst_queue_.push_back(*it);
          burst_queue_.erase(it);
          append = false;
          break;
        }
        it++;
      }
    }

    if (append) {
      //Buffer is missing append to queue directly
      BurstData new_entry;
      new_entry.result.append(result.metadata);
      new_entry.timestamp = timestamp;
      memset(&new_entry.buffer, 0, sizeof(new_entry.buffer));
      burst_queue_.push_back(new_entry);
    }
  }

  if (input_burst_queue_.size() >= num_images_) {
    if (StartProcessing() != NO_ERROR) {
      QMMF_ERROR("%s: Failed: Wrong state. Reprocess is not started.\n",
          __func__);
    }
  }
}

status_t CameraHalJpeg::ValidateInput(const CameraMetadata& static_meta,
                                        const PostProcCreateParam& input,
                                        const PostProcCreateParam& output) {
  camera_metadata_ro_entry_t entry;
  int32_t in_format, num_output_formats;

  if (static_meta.exists(ANDROID_SCALER_AVAILABLE_INPUT_OUTPUT_FORMATS_MAP)) {
    entry = static_meta.find(ANDROID_SCALER_AVAILABLE_INPUT_OUTPUT_FORMATS_MAP);
    for (uint32_t i = 0 ; i < entry.count; i++) {
      in_format = entry.data.i32[i++];
      num_output_formats = entry.data.i32[i++];
      if (in_format != input.format) {
        i +=  (num_output_formats - 1);
        continue;
      }
      for (int32_t f = 0; f < num_output_formats; f++) {
        i += f;
        if (output.format == entry.data.i32[i])
          return NO_ERROR;
      }
    }
  } else {
    QMMF_ERROR("%s: Failed ANDROID_SCALER_AVAILABLE_INPUT_OUTPUT_FORMATS_MAP\n",
        __func__);
  }

  QMMF_ERROR("%s: Failed: input format: 0x%x out format 0x%x\n",  __func__,
      input.format, output.format);

  return BAD_VALUE;
}

}; // namespace recoder

}; // namespace qmmf
