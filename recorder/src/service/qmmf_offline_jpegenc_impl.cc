/*
* Copyright (c) 2021 Qualcomm Innovation Center, Inc. All rights reserved.
*  
* Redistribution and use in source and binary forms, with or without
* modification, are permitted (subject to the limitations in the
* disclaimer below) provided that the following conditions are met:
*  
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*  
*     * Redistributions in binary form must reproduce the above
*       copyright notice, this list of conditions and the following
*       disclaimer in the documentation and/or other materials provided
*       with the distribution.
*  
*     * Neither the name of Qualcomm Innovation Center, Inc. nor the names of its
*       contributors may be used to endorse or promote products derived
*       from this software without specific prior written permission.
*  
* NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE
* GRANTED BY THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT
* HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
* WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
* MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
* IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
* ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
* GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
* IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
* OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#define LOG_TAG "OfflineJPEG"

#include "qmmf_offline_jpegenc_impl.h"

#include <dlfcn.h>
#include <cutils/native_handle.h>

#include "common/utils/qmmf_log.h"

namespace qmmf {

OfflineJpegEncoder::OfflineJpegEncoder() :
                    jpeg_lib_(nullptr),
                    pCameraPostProcCreate(nullptr),
                    pCameraPostProcProcess(nullptr),
                    pCameraPostProcDestroy(nullptr),
                    pproc_instance_(nullptr),
                    cb_data_(nullptr),
                    frame_number_(0),
                    exit_pending_(false) {

  QMMF_INFO("%s: Enter ", __func__);
  QMMF_INFO("%s: Exit ", __func__);
}

status_t OfflineJpegEncoder::Init(
                  const recorder::RemoteCallbackHandle& remote_cb_handle) {
  QMMF_INFO("%s: Enter ", __func__);

  assert(remote_cb_handle != nullptr);
  remote_cb_handle_ = remote_cb_handle;

  int32_t ret = NO_ERROR;
  jpeg_lib_ = dlopen(JPEG_POSTPROC_LIB, RTLD_NOW | RTLD_LOCAL);
  if (!jpeg_lib_) {
    QMMF_ERROR("%s: No postproc lib, dlopen failed with: %s.",
                 __func__, dlerror());
    return BAD_VALUE;
  }

  pCameraPostProcCreate   = (PFN_CameraPostProc_Create)
                              dlsym(jpeg_lib_, "CameraPostProc_Create");
  pCameraPostProcProcess  = (PFN_CameraPostProc_Process)
                              dlsym(jpeg_lib_, "CameraPostProc_Process");
  pCameraPostProcDestroy  = (PFN_CameraPostProc_Destroy)
                              dlsym(jpeg_lib_, "CameraPostProc_Destroy");

  if ((nullptr == pCameraPostProcCreate)       ||
      (nullptr == pCameraPostProcDestroy)      ||
      (nullptr == pCameraPostProcProcess)) {
    QMMF_ERROR("%s: dlsym failed, %p, %p, %p", __func__,
                                               pCameraPostProcCreate,
                                               pCameraPostProcProcess,
                                               pCameraPostProcDestroy);
    return BAD_VALUE;
  }

  ret = Run("PostProcThread");
  if (OK != ret) {
    QMMF_ERROR("%s: Thread creation failed!", __func__);
    return BAD_VALUE;
  }

  QMMF_INFO("%s: Exit ", __func__);

  return ret;
}

status_t OfflineJpegEncoder::DeInit() {
  QMMF_INFO("%s: Enter ", __func__);
  {
    std::lock_guard<std::mutex> lock(buffer_lock_);
    exit_pending_ = true;
    buffer_signal_.Signal();
  }

  RequestExitAndWait();

  pCameraPostProcCreate = nullptr;
  pCameraPostProcProcess = nullptr;
  pCameraPostProcDestroy = nullptr;

  if (jpeg_lib_) {
    dlclose(jpeg_lib_);
    jpeg_lib_ = nullptr;
  }

  clients_list_.clear();
  client_fd_map_.clear();

  QMMF_INFO("%s: Exit ", __func__);
  return NO_ERROR;
}

status_t OfflineJpegEncoder::RegisterClient(const uint32_t client_id) {
  QMMF_INFO("%s: Enter client_id %d", __func__, client_id);

  if (IsClientFound(client_id)) {
    QMMF_INFO("%s: Client %d already registered.", __func__, client_id);
  } else {
    clients_list_.push_back(client_id);
    QMMF_INFO("%s: Client %d registered successfully", __func__, client_id);
  }

  QMMF_INFO("%s: Exit client_id %d", __func__, client_id);

  return NO_ERROR;
}

status_t OfflineJpegEncoder::DeRegisterClient(const uint32_t client_id) {
  QMMF_INFO("%s: Enter client_id %d", __func__, client_id);

  if (!IsClientFound(client_id)) {
    QMMF_ERROR("%s: Client %d not found.", __func__, client_id);
    return BAD_VALUE;
  }

  for (uint32_t i = 0; i < clients_list_.size(); i++) {
    if (client_id == clients_list_[i]) {
      clients_list_.erase(clients_list_.begin() + i);
      QMMF_INFO("%s: Client %d removed successfully.", __func__, client_id);
    }
  }

  QMMF_INFO("%s: Exit client_id %d", __func__, client_id);

  return NO_ERROR;
}

bool OfflineJpegEncoder::IsClientFound(const uint32_t& client_id) {
  bool found = false;
  for (uint32_t i = 0; i < clients_list_.size(); i++) {
    if (client_id == clients_list_[i]) {
      found = true;
      break;
    }
  }

  return found;
}

status_t OfflineJpegEncoder::Create(const uint32_t client_id,
                                    const OfflineJpegCreateParams& params) {
  QMMF_INFO("%s: Enter client_id %d", __func__, client_id);

  if(!IsClientFound(client_id)) {
    QMMF_ERROR("%s Error: Client %d not found.", __func__, client_id);
    return BAD_VALUE;
  }

  create_params_.streamId = 0;
  create_params_.processMode = (PostProcMode)params.process_mode;

  create_params_.inBuffer.width = params.in_buffer.width;
  create_params_.inBuffer.height = params.in_buffer.height;
  create_params_.inBuffer.format = params.in_buffer.format;

  create_params_.outBuffer.width = params.out_buffer.width;
  create_params_.outBuffer.height = params.out_buffer.height;
  create_params_.outBuffer.format = params.out_buffer.format;

  create_params_.clientCb = JpegCb;
  cb_data_ = new JpegCbData;
  cb_data_->encoder = this;

  for (uint32_t i = 0; i < clients_list_.size(); i++) {
    if (client_id == clients_list_[i]) {
      cb_data_->client_id = clients_list_[i];
      break;
    }
  }

  create_params_.clientData = reinterpret_cast<void*>(cb_data_);

  pproc_instance_ = pCameraPostProcCreate(&create_params_);
  if (!pproc_instance_) {
  QMMF_ERROR("%s pproc_instance creation failed.", __func__);
    return BAD_VALUE;
  }
  QMMF_INFO("%s pproc_instance %p created successfully",
             __func__, pproc_instance_);

  QMMF_INFO("%s: Exit client_id %d", __func__, client_id);
  return NO_ERROR;
}

status_t OfflineJpegEncoder::Process(
                  const uint32_t client_id,
                  const OfflineJpegProcessParams& process_params) {
  QMMF_INFO("%s: Enter client_id %d", __func__, client_id);

  if (!pproc_instance_) {
    QMMF_ERROR("%s: No jpeg encoder instance!", __func__);
    return BAD_VALUE;
  }
  if(!IsClientFound(client_id)) {
    QMMF_ERROR("%s Error: Client %d not found.", __func__, client_id);
    return BAD_VALUE;
  }

  status_t ret = NO_ERROR;

  native_handle_t *input_nh;
  native_handle_t *output_nh;

  input_nh = native_handle_create(2, 8);
  output_nh = native_handle_create(2, 8);

  PostProcHandleParams in_handle_params, out_handle_params;
  in_handle_params.format = create_params_.inBuffer.format;
  in_handle_params.width = create_params_.inBuffer.width;
  in_handle_params.height = create_params_.inBuffer.height;

  input_nh->data[0] = process_params.in_buf_fd;
  in_handle_params.phHandle = input_nh;

  out_handle_params.format = create_params_.outBuffer.format;
  out_handle_params.width = create_params_.outBuffer.width;
  out_handle_params.height = create_params_.outBuffer.height;

  output_nh->data[0] = process_params.out_buf_fd;

  // Store client fd in map. It will be used in callback to client.
  client_fd_map_.emplace(process_params.out_buf_fd, process_params.reserved);
  out_handle_params.phHandle = output_nh;

  PostProcSessionParams* pproc_params = new PostProcSessionParams;
  if (!pproc_params) {
    QMMF_ERROR("%s: PosptProc param allocation failed", __func__);
    return NO_MEMORY;
  }

  pproc_params->streamId = 0;

  pproc_params->valid = true;

  pproc_params->frameNum = frame_number_++;

  camera_metadata_t *metadata = allocate_camera_metadata(1, 128);
  //TODO check client parameters as process_params.metadata.quality
  //If set by client fill the corresponding metadata entry.

  pproc_params->pMetadata = metadata;

  pproc_params->inHandle.push_back(in_handle_params);
  pproc_params->outHandle.push_back(out_handle_params);

  QMMF_DEBUG("%s:Handle params size input %d output %d", __func__,
            pproc_params->inHandle.size(),
            pproc_params->outHandle.size());

  {
    std::lock_guard<std::mutex> lock(buffer_lock_);
    pproc_queue_.push_back(pproc_params);
    buffer_signal_.Signal();
  }

  QMMF_INFO("%s: Exit client_id %d", __func__, client_id);
  return NO_ERROR;
}

status_t OfflineJpegEncoder::Destroy(const uint32_t client_id) {
  QMMF_INFO("%s: Enter client_id %d", __func__, client_id);

  if (!pproc_instance_) {
    QMMF_ERROR("%s: No jpeg encoder instance!", __func__);
    return BAD_VALUE;
  }

  {
    std::lock_guard<std::mutex> lock(buffer_lock_);
    exit_pending_ = true;
    buffer_signal_.Signal();
  }

  pCameraPostProcDestroy(pproc_instance_);
  pproc_instance_ = nullptr;

  if (cb_data_) {
    delete cb_data_;
    cb_data_ = nullptr;
  }

  frame_number_ = 0;

  QMMF_INFO("%s: Exit client_id %d", __func__, client_id);
  return NO_ERROR;
}

bool OfflineJpegEncoder::ThreadLoop() {
  if (ExitPending()) {
    QMMF_DEBUG("%s: Exit pending", __func__);
    return false;
  }
  PostProcSessionParams* pproc_params = nullptr;
  PostProcResultInfo status = { 0 };
  {
    std::unique_lock<std::mutex> lock(buffer_lock_);
    while (pproc_queue_.empty() && !exit_pending_) {
      QMMF_INFO("%s: Waiting for input buffer", __func__);
      buffer_signal_.Wait(lock);
      if (exit_pending_) {
        QMMF_INFO("%s: Exit request received", __func__);
        return false;
      }
    }

    pproc_params = pproc_queue_.front();
    pproc_queue_.pop_front();
  }

  if (nullptr == pproc_params) {
    QMMF_ERROR("%s: pproc_params is NULL", __func__);
    return false;
  } else {
    QMMF_INFO("%s: Submitting postproc request %p", __func__, pproc_params);
    status = pCameraPostProcProcess(pproc_instance_, pproc_params);
    if (POSTPROCSUCCESS == status.result) {
      QMMF_INFO("Postprocessing request submitted! Buf fd %d",
                 pproc_params->outHandle[0].phHandle->data[0]);
    } else {
      QMMF_ERROR("Postprocessing request failed %d", status.result);
    }
  }

  return true;
}

void OfflineJpegEncoder::ReleaseRequestData(PostProcSessionParams* params) {
  if (params) {
    int32_t in_buf_fd = params->inHandle[0].phHandle->data[0];
    int32_t out_buf_fd = params->outHandle[0].phHandle->data[0];
    native_handle_delete(
        const_cast<native_handle_t*>(params->inHandle[0].phHandle));
    native_handle_delete(
        const_cast<native_handle_t*>(params->outHandle[0].phHandle));
    close(in_buf_fd);
    close(out_buf_fd);
    delete params;
    params = nullptr;
  }
}

void OfflineJpegEncoder::NotifyJpeg(const uint32_t& client_id,
                                    const int32_t& buf_fd,
                                    const uint32_t& encoded_size,
                                    PostProcSessionParams* pproc_params) {

  // Get the corresponding client fd.
  auto it = client_fd_map_.find(buf_fd);
  QMMF_INFO("%s: Notifying client %d for buf_fd %d encoded_size %d",
            __func__,
            client_id,
            buf_fd,
            encoded_size);
  remote_cb_handle_(client_id)->NotifyOfflineJpegData(it->second,
                                                      encoded_size);
  client_fd_map_.erase(it);
  ReleaseRequestData(pproc_params);
}

int32_t JpegCb(PostProcSessionParams* pproc_params,
               uint32_t encoded_size,
               void* user_data) {
  if (!pproc_params) {
    QMMF_ERROR("%s: pproc_params is null", __func__);
    return BAD_VALUE;
  }
  if (!user_data) {
    QMMF_ERROR("%s: user_data is null", __func__);
    return BAD_VALUE;
  }

  JpegCbData* cb_data = reinterpret_cast<JpegCbData*>(user_data);
  OfflineJpegEncoder* enc = cb_data->encoder;
  uint32_t client = cb_data->client_id;
  int32_t out_buf_fd = pproc_params->outHandle[0].phHandle->data[0];
  enc->NotifyJpeg(client, out_buf_fd, encoded_size, pproc_params);

  return NO_ERROR;
}

};  // namespace qmmf.
