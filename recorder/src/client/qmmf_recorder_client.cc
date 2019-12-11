/*
* Copyright (c) 2016, 2019, The Linux Foundation. All rights reserved.
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

#define LOG_TAG "RecorderClient"

#include <type_traits>
#include <map>
#include <fcntl.h>
#include <dirent.h>
#include <dlfcn.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/msm_ion.h>
#include <binder/Parcel.h>
#include <binder/ProcessState.h>
#include <binder/IPCThreadState.h>
#include <camera/VendorTagDescriptor.h>

#include "common/utils/qmmf_tools.h"
#include "recorder/src/client/qmmf_recorder_client.h"
#include "recorder/src/client/qmmf_recorder_client_ion.h"
#include "recorder/src/client/qmmf_recorder_params_internal.h"
#include "recorder/src/service/qmmf_recorder_common.h"

#ifdef LOG_LEVEL_KPI
volatile uint32_t kpi_debug_level = BASE_KPI_FLAG;
#endif

uint32_t qmmf_log_level;

namespace qmmf {

namespace recorder {

//
// This file has implementation of following classes:
//
// - RecorderClient    : Delegation to binder proxy <IRecorderService>
//                       and implementation of binder CB.
// - BpRecorderService : Binder proxy implementation.
// - BpRecorderServiceCallback : Binder CB proxy implementation.
// - BnRecorderServiceCallback : Binder CB stub implementation.
//

using namespace android;
using ::std::underlying_type;

RecorderClient::RecorderClient()
    : recorder_service_(nullptr),
      death_notifier_(nullptr),
      ion_device_(-1),
      client_id_(0),
      metadata_cb_(nullptr),
      vendor_tag_desc_(nullptr) {

  QMMF_GET_LOG_LEVEL();
  QMMF_KPI_GET_MASK();
  QMMF_KPI_DETAIL();
  QMMF_INFO("%s Enter ", __func__);

#ifdef ANDROID_O_OR_ABOVE
  ProcessState::initWithDriver("/dev/vndbinder");
#endif

#ifdef TARGET_USES_GBM
  gbm_fd_ = open("/dev/ion", O_RDWR);
  assert(gbm_fd_ >= 0);

  gbm_device_ = gbm_create_device(gbm_fd_);
  assert(gbm_device_ != nullptr);
#endif

  sp<ProcessState> proc(ProcessState::self());
  proc->startThreadPool();
  QMMF_INFO("%s Exit (0x%p)", __func__, this);
}

RecorderClient::~RecorderClient() {

  QMMF_INFO("%s Enter ", __func__);
  QMMF_KPI_DETAIL();

  recorder_service_.clear();
  recorder_service_ = nullptr;

#ifdef TARGET_USES_GBM
  gbm_device_destroy(gbm_device_);
  close(gbm_fd_);
#endif

  QMMF_INFO("%s Exit 0x%p", __func__, this);
}

status_t RecorderClient::Connect(const RecorderCb& cb) {

  QMMF_DEBUG("%s Enter ", __func__);
  QMMF_KPI_DETAIL();
  std::lock_guard<std::mutex> lock(lock_);

  if (CheckServiceStatus()) {
    QMMF_WARN("%s Client is already connected to service!", __func__);
    return NO_ERROR;
  }

  ion_device_ = ion_open();
  if (ion_device_ < 0) {
    QMMF_ERROR("%s: Can't open Ion device!", __func__);
    return NO_INIT;
  }

  recorder_cb_ = cb;

  NotifyServerDeathCB death_cb = [&] { ServiceDeathHandler(); };
  death_notifier_ = new DeathNotifier(death_cb);
  if (nullptr == death_notifier_.get()) {
    QMMF_ERROR("%s Unable to allocate death notifier!", __func__);
    return NO_MEMORY;
  }

  sp<IBinder> service_handle;
  sp<IServiceManager> service_manager = defaultServiceManager();

  service_handle = service_manager->
      getService(String16(QMMF_RECORDER_SERVICE_NAME));
  if (service_handle.get() == nullptr) {
    QMMF_ERROR("%s Can't get (%s) service", __func__,
        QMMF_RECORDER_SERVICE_NAME);
    return NO_INIT;
  }

  recorder_service_ = interface_cast<IRecorderService>(service_handle);
  IInterface::asBinder(recorder_service_)->linkToDeath(death_notifier_);

  sp<ServiceCallbackHandler> handler = new ServiceCallbackHandler(this);
  uint32_t client_id;
  auto ret = recorder_service_->Connect(handler, &client_id);
  if (NO_ERROR != ret) {
    QMMF_ERROR("%s Can't connect to (%s) service", __func__,
        QMMF_RECORDER_SERVICE_NAME);
  }
  client_id_ = client_id;
  QMMF_INFO("%s: client_id(%d)", __func__, client_id);

  session_cb_list_.clear();
  track_cb_list_.clear();

  QMMF_DEBUG("%s Exit ", __func__);
  return ret;
}

status_t RecorderClient::Disconnect() {

  QMMF_DEBUG("%s Enter ", __func__);
  QMMF_KPI_DETAIL();
  std::lock_guard<std::mutex> lock(lock_);

  if (!CheckServiceStatus()) {
    return NO_INIT;
  }

  auto ret = recorder_service_->Disconnect(client_id_);
  if (NO_ERROR != ret) {
    QMMF_ERROR("%s Disconnect failed!", __func__);
  }

  recorder_service_->asBinder(recorder_service_)->
      unlinkToDeath(death_notifier_);

  recorder_service_.clear();
  recorder_service_ = nullptr;

  death_notifier_.clear();
  death_notifier_ = nullptr;


  sessions_.clear();
  session_cb_list_.clear();
  track_cb_list_.clear();

  if (ion_device_ > 0) {
    ion_close(ion_device_);
    ion_device_ = -1;
  }
  client_id_ = 0;

  // Clear global tag descriptor for the process
  VendorTagDescriptor::clearGlobalVendorTagDescriptor();
  vendor_tag_desc_ = nullptr;

  QMMF_DEBUG("%s Exit ", __func__);
  return ret;
}

status_t RecorderClient::StartCamera(const uint32_t camera_id,
                                     const float frame_rate,
                                     const CameraExtraParam& extra_param,
                                     const CameraResultCb &result_cb) {
  bool enable_result_cb = false;
  QMMF_DEBUG("%s Enter ", __func__);
  QMMF_KPI_DETAIL();
  std::lock_guard<std::mutex> lock(lock_);

  if (!CheckServiceStatus()) {
    return NO_INIT;
  }

  if (nullptr != result_cb) {
    metadata_cb_ = result_cb;
    enable_result_cb = true;
  }

  assert(client_id_ > 0);
  auto ret = recorder_service_->StartCamera(client_id_, camera_id, frame_rate,
                                            extra_param,
                                            enable_result_cb);
  if (NO_ERROR != ret) {
    QMMF_ERROR("%s StartCamera failed!", __func__);
    return ret;
  }

#ifndef CAMERA_HAL1_SUPPORT
  if (vendor_tag_desc_ == nullptr) {
    vendor_tag_desc_ = new VendorTagDescriptor();
    ret = GetVendorTagDescriptor(vendor_tag_desc_);
    if (0 != ret) {
      QMMF_ERROR("%s: Unable to GetVendorTagDescriptor : %d\n", __func__, ret);
      return ret;
    }

    // Set the global descriptor to use with camera metadata
    ret =
        VendorTagDescriptor::setAsGlobalVendorTagDescriptor(vendor_tag_desc_);
    if (0 != ret) {
      QMMF_ERROR("%s: Unable to setAsGlobalVendorTagDescriptor : %d", __func__,
                 ret);
      return ret;
    }
  }
#endif

  QMMF_DEBUG("%s Exit ", __func__);
  return ret;
}

status_t RecorderClient::StopCamera(const uint32_t camera_id) {

  QMMF_DEBUG("%s Enter ", __func__);
  QMMF_KPI_DETAIL();

  std::lock_guard<std::mutex> lock(lock_);

  if (!CheckServiceStatus()) {
    return NO_INIT;
  }

  assert(client_id_ > 0);
  auto ret = recorder_service_->StopCamera(client_id_, camera_id);
  if (NO_ERROR != ret) {
    QMMF_ERROR("%s StopCamera failed!", __func__);
  }
  QMMF_DEBUG("%s Exit ", __func__);
  return ret;
}

status_t RecorderClient::CreateSession(const SessionCb& cb,
                                       uint32_t* session_id) {
  QMMF_DEBUG("%s Enter ", __func__);
  QMMF_KPI_DETAIL();
  std::lock_guard<std::mutex> lock(lock_);

  if (!CheckServiceStatus()) {
    return NO_INIT;
  }

  assert(client_id_ > 0);
  auto ret = recorder_service_->CreateSession(client_id_, session_id);
  if (NO_ERROR != ret) {
    QMMF_ERROR("%s CreateSession failed!", __func__);
  } else {
    sessions_.emplace(*session_id, std::set<uint32_t>());
    session_cb_list_.emplace(*session_id, cb);
  }

  QMMF_DEBUG("%s Exit ", __func__);
  return ret;
}

status_t RecorderClient::DeleteSession(const uint32_t session_id) {

  QMMF_DEBUG("%s Enter ", __func__);
  QMMF_KPI_DETAIL();
  std::lock_guard<std::mutex> lock(lock_);

  if (!CheckServiceStatus()) {
    return NO_INIT;
  }

  if (sessions_.count(session_id) == 0) {
    QMMF_ERROR("%s Invalid session_id(%d)!", __func__, session_id);
    return BAD_VALUE;
  }
  auto& tracks = sessions_[session_id];

  if (!tracks.empty()) {
    QMMF_ERROR("%s: Delete tracks first before deleting Session(%d)",
        __func__, session_id);
    return INVALID_OPERATION;
  }
  assert(client_id_ > 0);
  auto ret = recorder_service_->DeleteSession(client_id_, session_id);
  if (NO_ERROR != ret) {
    QMMF_ERROR("%s DeleteSession failed!", __func__);
  }

  sessions_.erase(session_id);
  session_cb_list_.erase(session_id);
  QMMF_DEBUG("%s Exit ", __func__);
  return ret;
}

status_t RecorderClient::StartSession(const uint32_t session_id) {

  QMMF_DEBUG("%s Enter ", __func__);
  QMMF_KPI_BASE();
  std::lock_guard<std::mutex> lock(lock_);

  if (!CheckServiceStatus()) {
    return NO_INIT;
  }

  if (sessions_.count(session_id) == 0) {
    QMMF_ERROR("%s: Invalid session_id(%d)!", __func__, session_id);
    return BAD_VALUE;
  }
  auto& tracks = sessions_[session_id];

  for (auto const& track : tracks) {
    buffer_ion_.Release(track);
  }

  assert(client_id_ > 0);
  auto ret = recorder_service_->StartSession(client_id_, session_id);
  if (NO_ERROR != ret) {
    QMMF_ERROR("%s StartSession failed!", __func__);
  } else {
    for (auto const& track : tracks) {
      QMMF_KPI_ASYNC_BEGIN("FirstVidFrame", track);
    }
  }
  QMMF_DEBUG("%s Exit ", __func__);
  return ret;
}

status_t RecorderClient::StopSession(const uint32_t session_id,
                                     bool do_flush) {
  QMMF_DEBUG("%s Enter ", __func__);
  QMMF_KPI_BASE();
  std::lock_guard<std::mutex> lock(lock_);

  if (!CheckServiceStatus()) {
    return NO_INIT;
  }

  if (sessions_.count(session_id) == 0) {
    QMMF_ERROR("%s Invalid session_id(%d)!", __func__, session_id);
    return BAD_VALUE;
  }

  assert(client_id_ > 0);
  auto ret = recorder_service_->StopSession(client_id_, session_id, do_flush);
  if (NO_ERROR != ret) {
    QMMF_ERROR("%s StopSession failed!", __func__);
  } else {
    auto& tracks = sessions_[session_id];
    for (auto const& track : tracks) {
      QMMF_KPI_ASYNC_BEGIN("LastVidFrame", track);
    }
  }
  QMMF_DEBUG("%s Exit ", __func__);
  return ret;
}

status_t RecorderClient::PauseSession(const uint32_t session_id) {

  QMMF_DEBUG("%s Enter ", __func__);
  QMMF_KPI_DETAIL();
  std::lock_guard<std::mutex> lock(lock_);

  if (!CheckServiceStatus()) {
    return NO_INIT;
  }

  if (sessions_.count(session_id) == 0) {
    QMMF_ERROR("%s Invalid session_id(%d)!", __func__, session_id);
    return BAD_VALUE;
  }

  assert(client_id_ > 0);
  auto ret = recorder_service_->PauseSession(client_id_, session_id);
  if (NO_ERROR != ret) {
    QMMF_ERROR("%s PauseSession failed!", __func__);
  }
  QMMF_DEBUG("%s Exit ", __func__);
  return ret;
}

status_t RecorderClient::ResumeSession(const uint32_t session_id)
{
  QMMF_DEBUG("%s Enter ", __func__);
  QMMF_KPI_DETAIL();
  std::lock_guard<std::mutex> lock(lock_);

  if (!CheckServiceStatus()) {
    return NO_INIT;
  }

  if (sessions_.count(session_id) == 0) {
    QMMF_ERROR("%s Invalid session_id(%d)!", __func__, session_id);
    return BAD_VALUE;
  }

  assert(client_id_ > 0);
  auto ret = recorder_service_->ResumeSession(client_id_, session_id);
  if (NO_ERROR != ret) {
    QMMF_ERROR("%s ResumeSession failed!", __func__);
  }
  QMMF_DEBUG("%s Exit ", __func__);
  return ret;
}

status_t RecorderClient::GetNumberOfCameras(SupportedCameras &cameras)
{
    QMMF_DEBUG("%s Enter ", __func__);
    QMMF_KPI_DETAIL();
    std::lock_guard<std::mutex> lock(lock_);

    if (!CheckServiceStatus()) {
      return NO_INIT;
    }
    assert(client_id_ > 0);
    auto ret = recorder_service_->GetNumberOfCameras(client_id_, cameras);
    if (NO_ERROR != ret) {
        QMMF_ERROR("%s GetNumberOfCameras failed!", __func__);
    }

    QMMF_DEBUG("%s Exit ", __func__);
    return ret;
}

status_t RecorderClient::GetSupportedPlugins(SupportedPlugins *plugins)
{
  QMMF_DEBUG("%s Enter ", __func__);
  QMMF_KPI_DETAIL();
  std::lock_guard<std::mutex> lock(lock_);

  if (!CheckServiceStatus()) {
    return NO_INIT;
  }

  assert(client_id_ > 0);
  auto ret = recorder_service_->GetSupportedPlugins(client_id_, plugins);
  if (NO_ERROR != ret) {
    QMMF_ERROR("%s GetSupportedPlugins failed!", __func__);
  }

  QMMF_DEBUG("%s Exit ", __func__);
  return ret;
}

status_t RecorderClient::CreatePlugin(uint32_t *uid, const PluginInfo &plugin)
{
  QMMF_DEBUG("%s Enter ", __func__);
  QMMF_KPI_DETAIL();
  std::lock_guard<std::mutex> lock(lock_);

  if (!CheckServiceStatus()) {
    return NO_INIT;
  }

  assert(client_id_ > 0);
  auto ret = recorder_service_->CreatePlugin(client_id_, uid, plugin);
  if (NO_ERROR != ret) {
    QMMF_ERROR("%s CreatePlugin failed!", __func__);
  }

  QMMF_DEBUG("%s Exit ", __func__);
  return ret;
}

status_t RecorderClient::DeletePlugin(const uint32_t &uid)
{
  QMMF_DEBUG("%s Enter ", __func__);
  QMMF_KPI_DETAIL();
  std::lock_guard<std::mutex> lock(lock_);

  if (!CheckServiceStatus()) {
    return NO_INIT;
  }

  assert(client_id_ > 0);
  auto ret = recorder_service_->DeletePlugin(client_id_, uid);
  if (NO_ERROR != ret) {
    QMMF_ERROR("%s DeletePlugin failed!", __func__);
  }

  QMMF_DEBUG("%s Exit ", __func__);
  return ret;
}

status_t RecorderClient::ConfigPlugin(const uint32_t &uid,
                                      const std::string &json_config)
{
  QMMF_DEBUG("%s Enter ", __func__);
  QMMF_KPI_DETAIL();
  std::lock_guard<std::mutex> lock(lock_);

  if (!CheckServiceStatus()) {
    return NO_INIT;
  }

  assert(client_id_ > 0);
  auto ret = recorder_service_->ConfigPlugin(client_id_, uid, json_config);
  if (NO_ERROR != ret) {
    QMMF_ERROR("%s ConfigPlugin failed!", __func__);
  }

  QMMF_DEBUG("%s Exit ", __func__);
  return ret;
}

status_t RecorderClient::ConfigPlugin(const uint32_t &uid,
                                      const int32_t type,
                                      const std::vector<uint8_t> &blob_config)
{
  QMMF_DEBUG("%s Enter ", __func__);
  QMMF_KPI_DETAIL();
  std::lock_guard<std::mutex> lock(lock_);

  assert(client_id_ > 0);

  auto ret = recorder_service_->ConfigPlugin(client_id_, uid, type, blob_config);
  if (NO_ERROR != ret) {
    QMMF_ERROR("%s: ConfigPlugin failed!", __func__);
  }

  QMMF_DEBUG("%s Exit ", __func__);
  return ret;
}

status_t RecorderClient::GetPluginConfig(const uint32_t &uid,
                                            std::string &json_config)
{
  QMMF_DEBUG("%s Enter ", __func__);
  QMMF_KPI_DETAIL();
  std::lock_guard<std::mutex> lock(lock_);

  if (!CheckServiceStatus()) {
    return NO_INIT;
  }

  assert(client_id_ > 0);
  auto ret = recorder_service_->GetPluginConfig(client_id_, uid, json_config);
  if (NO_ERROR != ret) {
    QMMF_ERROR("%s GetPluginConfig failed!", __func__);
  }

  QMMF_DEBUG("%s Exit ", __func__);
  return ret;
}

status_t RecorderClient::CreateAudioTrack(const uint32_t session_id,
                                          const uint32_t track_id,
                                          const AudioTrackCreateParam& param,
                                          const TrackCb& cb) {

  QMMF_DEBUG("%s Enter ", __func__);
  QMMF_VERBOSE("%s INPARAM: session_id[%u]", __func__, session_id);
  QMMF_VERBOSE("%s INPARAM: track_id[%u]", __func__, track_id);
  QMMF_VERBOSE("%s INPARAM: param[%s]", __func__, param.ToString().c_str());
  QMMF_KPI_DETAIL();

  std::lock_guard<std::mutex> lock(lock_);
  if (!CheckServiceStatus()) {
    return NO_INIT;
  }
  assert(track_id != 0);

  if (sessions_.count(session_id) == 0) {
    QMMF_ERROR("%s: Invalid session_id(%d)!", __func__, session_id);
    return BAD_VALUE;
  }

  if (sessions_[session_id].count(track_id) != 0) {
    QMMF_ERROR("%s track_id(%d) already exists!", __func__, track_id);
    return BAD_VALUE;
  }

  assert(client_id_ > 0);
  auto ret = recorder_service_->CreateAudioTrack(client_id_, session_id,
                                                 track_id, param);
  if (NO_ERROR != ret) {
    QMMF_ERROR("%s CreateAudioTrack failed: %d", __func__, ret);
  } else {
    UpdateSessionTopology(session_id, track_id, true /*add*/);
    std::lock_guard<std::mutex> l(track_cb_lock_);
    track_cb_list_.emplace(track_id, cb);
  }

  QMMF_DEBUG("%s Exit ", __func__);
  return ret;
}

status_t RecorderClient::CreateVideoTrack(const uint32_t session_id,
                                          const uint32_t track_id,
                                          const VideoTrackCreateParam& param,
                                          const TrackCb& cb) {

  QMMF_DEBUG("%s Enter ", __func__);
  QMMF_KPI_DETAIL();

  std::lock_guard<std::mutex> lock(lock_);
  if (!CheckServiceStatus()) {
    return NO_INIT;
  }
  assert(track_id != 0);

  if (sessions_.count(session_id) == 0) {
    QMMF_ERROR("%s: Invalid session_id(%d)!", __func__, session_id);
    return BAD_VALUE;
  }

  if (sessions_[session_id].count(track_id) != 0) {
    QMMF_ERROR("%s track_id(%d) already exists!", __func__, track_id);
    return BAD_VALUE;
  }

  assert(client_id_ > 0);
  auto ret = recorder_service_->CreateVideoTrack(client_id_, session_id,
                                                 track_id, param);
  if (NO_ERROR != ret) {
    QMMF_ERROR("%s CreateVideoTrack failed!", __func__);
  } else {
    UpdateSessionTopology(session_id, track_id, true /*add*/);
    std::lock_guard<std::mutex> l(track_cb_lock_);
    track_cb_list_.emplace(track_id, cb);
  }

  QMMF_DEBUG("%s Exit ", __func__);
  return ret;
}

status_t RecorderClient::CreateVideoTrack(const uint32_t session_id,
                                          const uint32_t track_id,
                                          const VideoTrackCreateParam& param,
                                          const VideoExtraParam& extra_param,
                                          const TrackCb& cb) {

  QMMF_DEBUG("%s Enter ", __func__);
  QMMF_KPI_DETAIL();

  std::lock_guard<std::mutex> lock(lock_);
  if (!CheckServiceStatus()) {
    return NO_INIT;
  }
  assert(track_id != 0);

  if (sessions_.count(session_id) == 0) {
    QMMF_ERROR("%s: Invalid session_id(%d)!", __func__, session_id);
    return BAD_VALUE;
  }

  if (sessions_[session_id].count(track_id) != 0) {
    QMMF_ERROR("%s track_id(%d) already exists!", __func__, track_id);
    return BAD_VALUE;
  }

  assert(client_id_ > 0);
  auto ret = recorder_service_->CreateVideoTrack(client_id_, session_id,
                                                 track_id, param, extra_param);
  if (NO_ERROR != ret) {
    QMMF_ERROR("%s CreateVideoTrackWithExtraParam failed!", __func__);
  } else {
    UpdateSessionTopology(session_id, track_id, true /*add*/);
    std::lock_guard<std::mutex> l(track_cb_lock_);
    track_cb_list_.emplace(track_id, cb);
  }

  QMMF_DEBUG("%s Exit ", __func__);
  return ret;
}

status_t RecorderClient::ReturnTrackBuffer(const uint32_t session_id,
                                           const uint32_t track_id,
                                           std::vector<BufferDescriptor>
                                           &buffers) {

  QMMF_DEBUG("%s Enter ", __func__);
  QMMF_VERBOSE("%s INPARAM: session_id[%u]", __func__, session_id);
  QMMF_VERBOSE("%s INPARAM: track_id[%u]", __func__, track_id);
  for (const BufferDescriptor& buffer : buffers)
    QMMF_VERBOSE("%s INPARAM: buffer[%s]", __func__,
                 buffer.ToString().c_str());
  if (!CheckServiceStatus()) {
    return NO_INIT;
  }

  QMMF_KPI_ASYNC_END("VideoAppCB", track_id);
  uint32_t ret = NO_ERROR;
  std::vector<BnBuffer> bn_buffers;

  for (auto const& buffer : buffers) {
    BnBuffer bn_buffer = {
      static_cast<int32_t>(buffer.buf_id), // ion_fd
      -1,                                  // metadata ion_fd
      buffer.size,                         // size
      buffer.timestamp,                    // timestamp
      0,                                   // width
      0,                                   // height
      buffer.buf_id,                       // buffer_id
      buffer.flag,                         // flag
      buffer.capacity                      // capacity
    };
    bn_buffers.push_back(bn_buffer);
  }
  assert(client_id_ > 0);
  ret = recorder_service_->ReturnTrackBuffer(client_id_, session_id, track_id,
                                             bn_buffers);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s ReturnTrackBuffer failed: %d", __func__, ret);
  }
  QMMF_DEBUG("%s Exit ", __func__);
  return ret;
}

status_t RecorderClient::SetAudioTrackParam(const uint32_t session_id,
                                            const uint32_t track_id,
                                            CodecParamType type,
                                            const void *param,
                                            size_t param_size) {

  QMMF_DEBUG("%s Enter ", __func__);
  std::lock_guard<std::mutex> lock(lock_);

  if (!CheckServiceStatus()) {
    return NO_INIT;
  }

  if (sessions_.count(session_id) == 0) {
    QMMF_ERROR("%s: Invalid session_id(%d)!", __func__, session_id);
    return BAD_VALUE;
  }

  if (sessions_[session_id].count(track_id) == 0) {
    QMMF_ERROR("%s Invalid track_id(%d)!", __func__, track_id);
    return BAD_VALUE;
  }

  assert(client_id_ > 0);
  auto ret = recorder_service_->SetAudioTrackParam(client_id_, session_id,
      track_id, type, const_cast<void*>(param), param_size);
  if (NO_ERROR != ret) {
    QMMF_ERROR("%s SetAudioTrackParam failed!", __func__);
  }
  QMMF_DEBUG("%s Exit ", __func__);
  return ret;
}

status_t RecorderClient::SetVideoTrackParam(const uint32_t session_id,
                                            const uint32_t track_id,
                                            CodecParamType type,
                                            const void *param,
                                            size_t param_size) {

  QMMF_DEBUG("%s Enter ", __func__);
  std::lock_guard<std::mutex> lock(lock_);
  if (!CheckServiceStatus()) {
    return NO_INIT;
  }

  if (sessions_.count(session_id) == 0) {
    QMMF_ERROR("%s: Invalid session_id(%d)!", __func__, session_id);
    return BAD_VALUE;
  }

  if (sessions_[session_id].count(track_id) == 0) {
    QMMF_ERROR("%s Invalid track_id(%d)!", __func__, track_id);
    return BAD_VALUE;
  }

  assert(client_id_ > 0);
  auto ret = recorder_service_->SetVideoTrackParam(client_id_, session_id,
      track_id, type, const_cast<void*>(param), param_size);
  if (NO_ERROR != ret) {
    QMMF_ERROR("%s SetVideoTrackParam failed!", __func__);
  }
  QMMF_DEBUG("%s Exit ", __func__);
  return ret;
}

status_t RecorderClient::DeleteAudioTrack(const uint32_t session_id,
                                          const uint32_t track_id) {

  QMMF_DEBUG("%s Enter ", __func__);
  QMMF_VERBOSE("%s INPARAM: session_id[%u]", __func__, session_id);
  QMMF_VERBOSE("%s INPARAM: track_id[%u]", __func__, track_id);
  QMMF_KPI_DETAIL();

  std::lock_guard<std::mutex> lock(lock_);
  if (!CheckServiceStatus()) {
    return NO_INIT;
  }

  if (sessions_.count(session_id) == 0) {
    QMMF_ERROR("%s: Invalid session_id(%d)!", __func__, session_id);
    return BAD_VALUE;
  }

  if (sessions_[session_id].count(track_id) == 0) {
    QMMF_ERROR("%s Invalid track_id(%d)!", __func__, track_id);
    return BAD_VALUE;
  }

  buffer_ion_.Release(track_id);
  assert(client_id_ > 0);
  auto ret = recorder_service_->DeleteAudioTrack(client_id_, session_id,
                                                 track_id);
  if (NO_ERROR != ret) {
    QMMF_ERROR("%s DeleteAudioTrack failed: %d", __func__, ret);
  } else {
    UpdateSessionTopology(session_id, track_id, false /*remove*/);
    std::lock_guard<std::mutex> l(track_cb_lock_);
    track_cb_list_.erase(track_id);
  }

  QMMF_DEBUG("%s Exit ", __func__);
  return ret;
}

status_t RecorderClient::DeleteVideoTrack(const uint32_t session_id,
                                          const uint32_t track_id) {

  QMMF_DEBUG("%s Enter track_id(%d)", __func__, track_id);
  QMMF_KPI_DETAIL();

  std::lock_guard<std::mutex> lock(lock_);
  if (!CheckServiceStatus()) {
    return NO_INIT;
  }

  if (sessions_.count(session_id) == 0) {
    QMMF_ERROR("%s: Invalid session_id(%d)!", __func__, session_id);
    return BAD_VALUE;
  }

  if (sessions_[session_id].count(track_id) == 0) {
    QMMF_ERROR("%s Invalid track_id(%d)!", __func__, track_id);
    return BAD_VALUE;
  }

  status_t ret = NO_ERROR;
  {
    std::lock_guard<std::mutex> l(track_buffers_lock_);
    if (track_buffers_map_.count(track_id) != 0) {
      for (auto& pair : track_buffers_map_[track_id]) {
        auto& buffer_info = pair.second;

        QMMF_INFO("%s track_id(%d): BufInfo: ion_fd(%d), vaddr(%p), size(%u)",
                  __func__, track_id, buffer_info.ion_fd, buffer_info.vaddr,
                  buffer_info.size);

        ret = UnmapBuffer(buffer_info);
        if (NO_ERROR != ret) {
          QMMF_ERROR("%s Failed to unmap buffer!", __func__);
          return ret;
        }

      }
      track_buffers_map_.erase(track_id);
    }
  }

  assert(client_id_ > 0);
  ret = recorder_service_->DeleteVideoTrack(client_id_, session_id, track_id);
  if (NO_ERROR != ret) {
    QMMF_ERROR("%s track_id(%d) DeleteVideoTrack failed!", __func__, track_id);
  } else {
    UpdateSessionTopology(session_id, track_id, false /*remove*/);
    std::lock_guard<std::mutex> l(track_cb_lock_);
    track_cb_list_.erase(track_id);
  }

  QMMF_DEBUG("%s Exit ", __func__);
  return ret;
}

status_t RecorderClient::CaptureImage(const uint32_t camera_id,
                                      const ImageParam &param,
                                      const uint32_t num_images,
                                      const std::vector<CameraMetadata> &meta,
                                      const ImageCaptureCb &cb) {

  QMMF_DEBUG("%s Enter ", __func__);
  QMMF_KPI_ASYNC_BEGIN("FirstCapImg", camera_id);

  std::lock_guard<std::mutex> lock(lock_);
  if (!CheckServiceStatus()) {
    return NO_INIT;
  }
  assert(client_id_ > 0);
  auto ret = recorder_service_->CaptureImage(client_id_, camera_id, param,
                                             num_images, meta);
  if (NO_ERROR != ret) {
    QMMF_ERROR("%s CaptureImage failed!", __func__);
  }
  image_capture_cb_ = cb;
  QMMF_DEBUG("%s Exit ", __func__);
  return ret;
}

status_t RecorderClient::ConfigImageCapture(const uint32_t camera_id,
                                            const ImageConfigParam &config) {

  QMMF_DEBUG("%s Enter ", __func__);
  std::lock_guard<std::mutex> lock(lock_);
  if (!CheckServiceStatus()) {
    return NO_INIT;
  }
  assert(client_id_ > 0);
  auto ret = recorder_service_->ConfigImageCapture(client_id_, camera_id,
                                                   config);
  if (NO_ERROR != ret) {
    QMMF_ERROR("%s ConfigImageCapture failed!", __func__);
  }
  QMMF_DEBUG("%s Exit ", __func__);
  return ret;
}

status_t RecorderClient::CancelCaptureImage(const uint32_t camera_id) {

  QMMF_DEBUG("%s Enter ", __func__);
  QMMF_KPI_DETAIL();
  std::lock_guard<std::mutex> lock(lock_);
  if (!CheckServiceStatus()) {
    return NO_INIT;
  }
  assert(client_id_ > 0);
  auto ret = recorder_service_->CancelCaptureImage(client_id_, camera_id);
  if(NO_ERROR != ret) {
    QMMF_ERROR("%s CancelCaptureImage failed!", __func__);
  }
  QMMF_DEBUG("%s Exit ", __func__);
  return ret;
}

status_t RecorderClient::ReturnImageCaptureBuffer(const uint32_t camera_id,
                                                  const BufferDescriptor
                                                      &buffer) {

  QMMF_DEBUG("%s Enter ", __func__);
  if (!CheckServiceStatus()) {
    return NO_INIT;
  }

  {
    // Unmap buffer from client process.
    std::lock_guard<std::mutex> lock(snapshot_buffers_lock_);
    if (snapshot_buffers_.count(buffer.fd) == 0) {
      QMMF_ERROR("%s Invalid buffer fd(%d)!", __func__, buffer.fd);
      return BAD_VALUE;
    }
    auto buffer_info = snapshot_buffers_[buffer.fd];

    QMMF_INFO("%s Snapshot BufInfo: ion_fd(%d), vaddr(%p), size(%u)", __func__,
              buffer_info.ion_fd, buffer_info.vaddr, buffer_info.size);

    auto ret = UnmapBuffer(buffer_info);
    if (NO_ERROR != ret) {
      QMMF_ERROR("%s Failed to unmap buffer!", __func__);
      return ret;
    }

    snapshot_buffers_.erase(buffer.fd);
  }

  QMMF_DEBUG("%s Returning buf_id(%d) back to service!", __func__,
      buffer.buf_id);
  assert(client_id_ > 0);
  auto ret = recorder_service_->ReturnImageCaptureBuffer(client_id_, camera_id,
                                                         buffer.buf_id);
  if (NO_ERROR != ret) {
      QMMF_ERROR("%s ReturnImageCaptureBuffer failed!", __func__);
  }
  QMMF_DEBUG("%s Exit ", __func__);
  return ret;
}

status_t RecorderClient::SetCameraParam(const uint32_t camera_id,
                                        const CameraMetadata &meta) {

  QMMF_DEBUG("%s Enter ", __func__);
  std::lock_guard<std::mutex> lock(lock_);
  if (!CheckServiceStatus()) {
    return NO_INIT;
  }
  assert(client_id_ > 0);
  auto ret = recorder_service_->SetCameraParam(client_id_, camera_id, meta);
  if (NO_ERROR != ret) {
    QMMF_ERROR("%s SetCameraParam failed!", __func__);
  }
  QMMF_DEBUG("%s Exit ", __func__);
  return ret;
}

status_t RecorderClient::GetCameraParam(const uint32_t camera_id,
                                        CameraMetadata &meta) {

  QMMF_DEBUG("%s Enter ", __func__);

  std::lock_guard<std::mutex> lock(lock_);
  if (!CheckServiceStatus()) {
    return NO_INIT;
  }
  assert(client_id_ > 0);
  auto ret = recorder_service_->GetCameraParam(client_id_, camera_id, meta);
  if (NO_ERROR != ret) {
    QMMF_ERROR("%s GetCameraParam failed!", __func__);
  }
  QMMF_DEBUG("%s Exit ", __func__);
  return ret;
}

status_t RecorderClient::GetDefaultCaptureParam(const uint32_t camera_id,
                                                CameraMetadata &meta) {

  QMMF_DEBUG("%s Enter ", __func__);
  std::lock_guard<std::mutex> lock(lock_);
  if (!CheckServiceStatus()) {
    return NO_INIT;
  }
  assert(client_id_ > 0);
  auto ret = recorder_service_->GetDefaultCaptureParam(client_id_,  camera_id,
                                                       meta);
  if (NO_ERROR != ret) {
    QMMF_ERROR("%s GetDefaultCaptureParam failed!", __func__);
  }
  QMMF_DEBUG("%s Exit ", __func__);
  return ret;
}

status_t RecorderClient::GetCameraCharacteristics(const uint32_t camera_id,
                                                  CameraMetadata &meta) {

  QMMF_DEBUG("%s Enter ", __func__);
  std::lock_guard<std::mutex> lock(lock_);
  if (!CheckServiceStatus()) {
    return NO_INIT;
  }
  assert(client_id_ > 0);
  auto ret = recorder_service_->GetCameraCharacteristics(client_id_,  camera_id,
                                                         meta);
  if (NO_ERROR != ret) {
    QMMF_ERROR("%s GetCameraCharacteristics failed!", __func__);
  }
  QMMF_DEBUG("%s Exit ", __func__);
  return ret;
}

status_t RecorderClient::CreateOverlayObject(const uint32_t track_id,
                                             const OverlayParam &param,
                                             uint32_t *overlay_id) {

  QMMF_DEBUG("%s Enter ", __func__);
  std::lock_guard<std::mutex> lock(lock_);
  if (!CheckServiceStatus()) {
    return NO_INIT;
  }
  assert(client_id_ > 0);
  auto ret = recorder_service_->CreateOverlayObject(client_id_, track_id,
                                                    const_cast<OverlayParam*>
                                                    (&param), overlay_id);
  if (NO_ERROR != ret) {
    QMMF_ERROR("%s CreateOverlayObject failed!", __func__);
  }
  QMMF_DEBUG("%s Exit ", __func__);
  return ret;
}

status_t RecorderClient::DeleteOverlayObject(const uint32_t track_id,
                                             const uint32_t overlay_id) {

  QMMF_DEBUG("%s Enter ", __func__);
  std::lock_guard<std::mutex> lock(lock_);
  if (!CheckServiceStatus()) {
    return NO_INIT;
  }
  assert(client_id_ > 0);
  auto ret = recorder_service_->DeleteOverlayObject(client_id_, track_id,
                                                    overlay_id);
  if (NO_ERROR != ret) {
    QMMF_ERROR("%s DeleteOverlayObject failed!", __func__);
  }
  QMMF_DEBUG("%s Exit ", __func__);
  return ret;
}

status_t RecorderClient::GetOverlayObjectParams(const uint32_t track_id,
                                                const uint32_t overlay_id,
                                                OverlayParam &param) {

  QMMF_DEBUG("%s Enter ", __func__);
  std::lock_guard<std::mutex> lock(lock_);
  if (!CheckServiceStatus()) {
    return NO_INIT;
  }
  assert(client_id_ > 0);
  auto ret = recorder_service_->GetOverlayObjectParams(client_id_, track_id,
                                                       overlay_id, param);
  if (NO_ERROR != ret) {
    QMMF_ERROR("%s GetOverlayObjectParams failed!", __func__);
  }
  QMMF_DEBUG("%s Exit ", __func__);
  return ret;
}

status_t RecorderClient::UpdateOverlayObjectParams(const uint32_t track_id,
                                                   const uint32_t overlay_id,
                                                   const OverlayParam &param) {

  QMMF_DEBUG("%s Enter ", __func__);
  std::lock_guard<std::mutex> lock(lock_);
  if (!CheckServiceStatus()) {
    return NO_INIT;
  }
  assert(client_id_ > 0);
  auto ret = recorder_service_->UpdateOverlayObjectParams(client_id_, track_id,
      overlay_id, const_cast<OverlayParam*>(&param));
  if (NO_ERROR != ret) {
      QMMF_ERROR("%s UpdateOverlayObjectParams failed!", __func__);
  }
  QMMF_DEBUG("%s Exit ", __func__);
  return ret;
}

status_t RecorderClient::SetOverlay(const uint32_t track_id,
                                    const uint32_t overlay_id) {

  QMMF_DEBUG("%s Enter ", __func__);
  std::lock_guard<std::mutex> lock(lock_);
  if (!CheckServiceStatus()) {
    return NO_INIT;
  }
  assert(client_id_ > 0);
  auto ret = recorder_service_->SetOverlayObject(client_id_, track_id,
                                                 overlay_id);
  if (NO_ERROR != ret) {
    QMMF_ERROR("%s SetOverlay failed!", __func__);
  }
  QMMF_DEBUG("%s Exit ", __func__);
  return ret;
}

status_t RecorderClient::RemoveOverlay(const uint32_t track_id,
                                       const uint32_t overlay_id) {

  QMMF_DEBUG("%s Enter ", __func__);
  std::lock_guard<std::mutex> lock(lock_);
  if (!CheckServiceStatus()) {
    return NO_INIT;
  }
  assert(client_id_ > 0);
  auto ret = recorder_service_->RemoveOverlayObject(client_id_, track_id,
                                                    overlay_id);
  if (NO_ERROR != ret) {
    QMMF_ERROR("%s RemoveOverlay failed!", __func__);
  }
  QMMF_DEBUG("%s Exit ", __func__);
  return ret;
}

status_t RecorderClient::CreateMultiCamera(const std::vector<uint32_t>
                                           camera_ids,
                                           uint32_t *virtual_camera_id) {

  QMMF_DEBUG("%s Enter ", __func__);
  QMMF_KPI_DETAIL();

  std::lock_guard<std::mutex> lock(lock_);
  if (!CheckServiceStatus()) {
    return NO_INIT;
  }
  assert(client_id_ > 0);
  auto ret = recorder_service_->CreateMultiCamera(client_id_, camera_ids,
                                                  virtual_camera_id);
  if (NO_ERROR != ret) {
    QMMF_ERROR("%s CreateMultiCamera failed!", __func__);
  }
  QMMF_DEBUG("%s Exit ", __func__);
  return ret;
}

status_t RecorderClient::ConfigureMultiCamera(const uint32_t virtual_camera_id,
                                              const MultiCameraConfigType type,
                                              const void *param,
                                              const uint32_t param_size) {

  QMMF_DEBUG("%s Enter ", __func__);

  std::lock_guard<std::mutex> lock(lock_);
  if (!CheckServiceStatus()) {
    return NO_INIT;
  }
  assert(client_id_ > 0);
  auto ret = recorder_service_->ConfigureMultiCamera(client_id_,
                                                     virtual_camera_id, type,
                                                     param, param_size);
  if (NO_ERROR != ret) {
    QMMF_ERROR("%s ConfigureMultiCamera failed!", __func__);
  }
  QMMF_DEBUG("%s Exit ", __func__);
  return ret;
}

status_t RecorderClient::GetVendorTagDescriptor(sp<VendorTagDescriptor> &desc) {

  QMMF_DEBUG("%s Enter ", __func__);

  if (!CheckServiceStatus()) {
    return NO_INIT;
  }
  assert(client_id_ > 0);
  auto ret = recorder_service_->GetVendorTagDescriptor(desc);
  if (NO_ERROR != ret) {
    QMMF_ERROR("%s GetVendorTagDescriptor failed!", __func__);
  }
  QMMF_DEBUG("%s Exit ", __func__);
  return ret;
}

#ifdef TARGET_USES_GBM
void RecorderClient::ImportBuffer(int32_t fd, int32_t metafd,
                                  const MetaData& meta) {

  std::lock_guard<std::mutex> lock(gbm_lock_);
  if (gbm_buffers_map_.count(fd) != 0) {
    // Already imported fd and metafd.
    return;
  } else if (metafd == -1) {
    // The metadata FD is missing, do not import.
    return;
  }

  uint32_t width = 0, height = 0, format = 0;
  if (meta.meta_flag && static_cast<uint32_t>(MetaParamType::kCamBufMetaData)) {
    auto& bufdata = meta.cam_buffer_meta_data;
    width = bufdata.plane_info[0].width;
    height = bufdata.plane_info[0].height;

    switch (bufdata.format) {
      case BufferFormat::kNV12:
        format = GBM_FORMAT_NV12;
        break;
      case BufferFormat::kNV21:
        format = GBM_FORMAT_NV21_ZSL;
        break;
      case BufferFormat::kNV16:
        format = GBM_FORMAT_NV16;
        break;
      case BufferFormat::kBLOB:
        format = GBM_FORMAT_BLOB;
        break;
      case BufferFormat::kNV12UBWC:
        format = GBM_FORMAT_YCbCr_420_SP_VENUS_UBWC;
        break;
      default:
        format = 0;
    }
  }

  gbm_buf_info bufinfo = { fd, metafd, width , height, format };

  auto bo = gbm_bo_import(gbm_device_, GBM_BO_IMPORT_GBM_BUF_TYPE, &bufinfo, 0);
  gbm_buffers_map_.emplace(fd, bo);
}

void RecorderClient::ReleaseBuffer(int32_t& fd) {

  std::lock_guard<std::mutex> lock(gbm_lock_);
  if (gbm_buffers_map_.count(fd) == 0) {
    // Already released or never imported.
    return;
  }

  gbm_bo_destroy(gbm_buffers_map_[fd]);
  gbm_buffers_map_.erase(fd);
  fd = -1;
}
#endif

status_t RecorderClient::MapBuffer(BufferInfo& info) {

  QMMF_DEBUG("%s Enter ", __func__);
  void* vaddr = nullptr;
  assert(ion_device_ > 0);

  vaddr = mmap(NULL, info.size, PROT_READ | PROT_WRITE, MAP_SHARED,
               info.ion_fd, 0);
  if (nullptr == vaddr) {
    QMMF_ERROR("%s Failed to map ion_fd %d : %d[%s]!", __func__, info.ion_fd,
               -errno, strerror(errno));
    return NO_MEMORY;
  }
  SyncStart(info.ion_fd);
  info.vaddr      = vaddr;
  QMMF_DEBUG("%s Exit ", __func__);
  return NO_ERROR;
}

status_t RecorderClient::UnmapBuffer(BufferInfo& info) {

  QMMF_DEBUG("%s Enter ", __func__);
  assert(ion_device_ > 0);

  if (info.vaddr != nullptr) {
    SyncEnd(info.ion_fd);
    if (munmap(info.vaddr, info.size) < 0) {
      QMMF_ERROR("%s() unable to unmap buffer[%d]: %d[%s]", __func__,
                 info.ion_fd, errno, strerror(errno));
      return errno;
    }
    info.vaddr = nullptr;

#ifdef TARGET_USES_GBM
    ReleaseBuffer(info.ion_fd);
#endif
    if ((info.ion_fd != -1) && (close(info.ion_fd) < 0)) {
      QMMF_ERROR("%s() error closing shared fd[%d]: %d[%s]", __func__,
                 info.ion_fd, errno, strerror(errno));
      return errno;
    }
    info.ion_fd = -1;
  }

  QMMF_DEBUG("%s Exit ", __func__);
  return NO_ERROR;
}

bool RecorderClient::CheckServiceStatus() {

  if (nullptr == recorder_service_.get()) {
    QMMF_WARN("%s Not connected to Recorder service!", __func__);
    return false;
  }
  return true;
}

void RecorderClient::UpdateSessionTopology(const uint32_t& session_id,
                                           const uint32_t& track_id, bool add) {
  QMMF_DEBUG("%s Enter ", __func__);

  auto& tracks = sessions_[session_id];
  if (!add) {
    tracks.erase(track_id);
  } else {
    tracks.emplace(track_id);
  }

  for (auto const& track : tracks) {
    QMMF_INFO("%s session_id(%d): track_id(%d)", __func__, session_id, track);
  }
  QMMF_DEBUG("%s Exit ", __func__);
}

void RecorderClient::ServiceDeathHandler() {
  QMMF_INFO("%s Enter ", __func__);

  std::lock_guard<std::mutex> lock(lock_);
  int32_t ret = NO_ERROR;
  //Clear all pending buffers.
  {
    std::lock_guard<std::mutex> l(track_buffers_lock_);
    for (auto& iter : track_buffers_map_) {
      uint32_t track_id = iter.first;
      BufferInfoMap& info_map = iter.second;

      for (auto& it : info_map) {
        BufferInfo& buffer_info = it.second;

        QMMF_INFO("%s track_id(%d): BufInfo: ion_fd(%d), vaddr(%p), size(%u)",
                  __func__, track_id, buffer_info.ion_fd,
                  buffer_info.vaddr, buffer_info.size);

        ret = UnmapBuffer(buffer_info);
        if (NO_ERROR != ret) {
          QMMF_ERROR("%s Failed to unmap buffer!", __func__);
          return;
        }
      }
    }
    track_buffers_map_.clear();
  }

  {
    std::lock_guard<std::mutex> l(snapshot_buffers_lock_);
    for (auto& it : snapshot_buffers_) {
      auto& buffer_info = it.second;

      QMMF_INFO("%s Snapshot BufInfo: ion_fd(%d), vaddr(%p), size(%u)",
                __func__, buffer_info.ion_fd,
                buffer_info.vaddr, buffer_info.size);

      ret = UnmapBuffer(buffer_info);
      if (NO_ERROR != ret) {
        QMMF_ERROR("%s Failed to unmap buffer!", __func__);
        return;
      }
    }
    snapshot_buffers_.clear();
  }

  recorder_service_->asBinder(recorder_service_)->
      unlinkToDeath(death_notifier_);

  recorder_service_.clear();
  recorder_service_ = nullptr;

  death_notifier_.clear();
  death_notifier_ = nullptr;

  sessions_.clear();
  session_cb_list_.clear();
  track_cb_list_.clear();

  image_capture_cb_ = nullptr;
  metadata_cb_ = nullptr;

  if (ion_device_ > 0) {
    ion_close(ion_device_);
    ion_device_ = -1;
  }
  client_id_ = 0;

  if (recorder_cb_.event_cb != nullptr) {
    recorder_cb_.event_cb(EventType::kServerDied, nullptr, 0);
  }
  QMMF_INFO("%s Exit ", __func__);
}

void RecorderClient::NotifyRecorderEvent(EventType event_type, void *event_data,
                                         size_t event_data_size) {
  QMMF_DEBUG("%s Enter ", __func__);
  if (recorder_cb_.event_cb != nullptr) {
    recorder_cb_.event_cb(event_type, event_data, event_data_size);
  }
  QMMF_DEBUG("%s Exit ", __func__);
}

void RecorderClient::NotifySessionEvent(EventType event_type, void *event_data,
                                        size_t event_data_size) {
    QMMF_DEBUG("%s Enter ", __func__);
    QMMF_DEBUG("%s Exit ", __func__);
}

void RecorderClient::NotifySnapshotData(uint32_t camera_id,
                                        uint32_t image_sequence_count,
                                        BnBuffer& buffer, MetaData& meta_data) {

  QMMF_DEBUG("%s Enter ", __func__);

  assert(image_capture_cb_ != nullptr);
  assert(buffer.ion_fd > 0);
  assert(buffer.buffer_id > 0);

  BufferInfo buffer_info {};
  buffer_info.ion_fd      = buffer.ion_fd;
  buffer_info.ion_meta_fd = buffer.ion_meta_fd;
  buffer_info.size        = buffer.capacity;

  auto ret = MapBuffer(buffer_info);
  if (NO_ERROR != ret) {
    QMMF_ERROR("%s Failed to map buffer!", __func__);
    return;
  }
  {
    std::lock_guard<std::mutex> lock(snapshot_buffers_lock_);
    snapshot_buffers_.emplace(buffer.ion_fd, buffer_info);
  }

#ifdef TARGET_USES_GBM
  ImportBuffer(buffer.ion_fd, buffer.ion_meta_fd, meta_data);
#endif

  BufferDescriptor image_buffer {};
  image_buffer.data      = buffer_info.vaddr;
  image_buffer.size      = buffer.size;
  image_buffer.timestamp = buffer.timestamp;
  image_buffer.flag      = buffer.flag;
  image_buffer.capacity  = buffer.capacity;
  image_buffer.buf_id    = buffer.buffer_id;
  image_buffer.fd        = buffer.ion_fd;

  if(image_sequence_count == 0) {
    QMMF_KPI_ASYNC_END("FirstCapImg", camera_id);
  } else {
    QMMF_KPI_ASYNC_END("SnapShot-Shot", camera_id);
  }

  QMMF_KPI_ASYNC_BEGIN("SnapShot-Shot", camera_id);

  image_capture_cb_(camera_id, image_sequence_count, image_buffer, meta_data);
  QMMF_DEBUG("%s Exit ", __func__);
}

void RecorderClient::NotifyVideoTrackData(uint32_t track_id,
                                          std::vector<BnBuffer> &bn_buffers,
                                          std::vector<MetaData> &meta_buffers) {

  QMMF_DEBUG("%s Enter track_id=%d", __func__, track_id);

  std::vector<BufferDescriptor> track_buffers;
  for (uint32_t idx = 0; idx < bn_buffers.size(); ++idx) {
    BnBuffer& bn_buffer = bn_buffers[idx];

    bool is_mapped = false;
    BufferInfo buffer_info {};

    // Check if ION buffer is already imported and mapped, if it is then get
    // buffer info from map.
    {
      std::lock_guard<std::mutex> l(track_buffers_lock_);
      if (track_buffers_map_.count(track_id) != 0) {
        auto& info_map = track_buffers_map_[track_id];

        if (info_map.count(bn_buffer.buffer_id) != 0) {
          buffer_info = info_map[bn_buffer.buffer_id];

          bn_buffer.ion_fd = buffer_info.ion_fd;
          bn_buffer.ion_meta_fd = buffer_info.ion_meta_fd;
          is_mapped = true;

          QMMF_VERBOSE("%s Buffer is already mapped! buffer_id(%d):ion_fd(%d):"
              "vaddr(%p)",  __func__, bn_buffer.buffer_id,
              buffer_info.ion_fd, buffer_info.vaddr);
        }
      } else {
        QMMF_KPI_ASYNC_END("FirstVidFrame", track_id);
      }
    }
    if (!is_mapped) {
      buffer_info.ion_fd      = bn_buffer.ion_fd;
      buffer_info.ion_meta_fd = bn_buffer.ion_meta_fd;
      buffer_info.size        = bn_buffer.capacity;

      auto ret = MapBuffer(buffer_info);
      if (NO_ERROR != ret) {
        QMMF_ERROR("%s Failed to map buffer!", __func__);
        return;
      }

#ifdef TARGET_USES_GBM
      ImportBuffer(bn_buffer.ion_fd, bn_buffer.ion_meta_fd, meta_buffers[idx]);
#endif

      QMMF_INFO("%s track_id(%d): BufInfo: ion_fd(%d), "
          "vaddr(%p), size(%u)", __func__, track_id, buffer_info.ion_fd,
           buffer_info.vaddr, buffer_info.size);

      // Update existing entry or add new one.
      std::lock_guard<std::mutex> l(track_buffers_lock_);
      BufferInfoMap& buffer_info_map = track_buffers_map_[track_id];
      buffer_info_map.emplace(bn_buffer.buffer_id, buffer_info);

      QMMF_VERBOSE("%s track_buffers_map_.size = %d", __func__,
          track_buffers_map_.size());

      for (auto const& iter : track_buffers_map_) {
        QMMF_VERBOSE("%s track_id(%d): BufInfoMap size = %d", __func__,
            iter.first, iter.second.size());

        for (auto const& it : iter.second) {
          QMMF_VERBOSE("%s BufInfo: key(%d), ion_fd(%d), vaddr(%p)",
              __func__, it.first, it.second.ion_fd, it.second.vaddr);
        }
      }
    }

    BufferDescriptor buffer {};
    buffer.data      = buffer_info.vaddr;
    buffer.size      = bn_buffer.size;
    buffer.timestamp = bn_buffer.timestamp;
    buffer.flag      = bn_buffer.flag;
    buffer.buf_id    = bn_buffer.buffer_id;
    buffer.capacity  = bn_buffer.capacity;
    buffer.fd        = buffer_info.ion_fd;
    track_buffers.push_back(buffer);

    if (buffer.flag & static_cast<uint32_t>(BufferFlags::kFlagEOS)) {
      QMMF_KPI_ASYNC_END("LastVidFrame", track_id);
    }
  }
  QMMF_DEBUG("%s Buffer Prepared for Callback track_id=%d", __func__, track_id);

  // Get the handle to track callbacks.
  std::unique_lock<std::mutex> l(track_cb_lock_);
  if (track_cb_list_.count(track_id) != 0) {
    TrackCb callbacks = track_cb_list_[track_id];
    l.unlock();

    QMMF_KPI_ASYNC_BEGIN("VideoAppCB", track_id);
    callbacks.data_cb(track_id, track_buffers, meta_buffers);
  } else {
    QMMF_ERROR("%s Track(%u) has not callback!", __func__, track_id);
  }
  QMMF_DEBUG("%s Exit ", __func__);
}

void RecorderClient::NotifyVideoTrackEvent(uint32_t track_id,
                                           EventType event_type,
                                           void *event_data,
                                           size_t event_data_size) {
  QMMF_DEBUG("%s Enter ", __func__);
  QMMF_VERBOSE("%s Track(%u): Received event type = %d", __func__, track_id,
    (int32_t) event_type);

  // Get the handle to track callbacks.
  std::unique_lock<std::mutex> l(track_cb_lock_);
  if (track_cb_list_.count(track_id) != 0) {
    TrackCb callbacks = track_cb_list_[track_id];
    l.unlock();

    callbacks.event_cb(track_id, event_type, event_data, event_data_size);
  } else {
    QMMF_ERROR("%s Track(%u) has not callback!", __func__, track_id);
  }
  QMMF_DEBUG("%s Exit ", __func__);
}

void RecorderClient::NotifyAudioTrackData(uint32_t track_id,
                                          const std::vector<BnBuffer>&
                                          bn_buffers,
                                          const std::vector<MetaData>&
                                          meta_buffers) {

  QMMF_DEBUG("%s Enter ", __func__);
  QMMF_VERBOSE("%s INPARAM: track_id[%u]", __func__, track_id);
  for (const BnBuffer& bn_buffer : bn_buffers)
    QMMF_VERBOSE("%s INPARAM: bn_buffer[%s]", __func__,
                 bn_buffer.ToString().c_str());

  std::vector<BufferDescriptor> track_buffers;
  for (const BnBuffer& bn_buffer : bn_buffers) {
    BufferDescriptor buffer {};
    auto ret = buffer_ion_.Associate(track_id, bn_buffer, &buffer);
    if (ret != 0) {
      QMMF_ERROR("%s Failed to associate audio buffer: %d[%s]\n", __func__,
          -ret, strerror(ret));
      return;
    }
    track_buffers.push_back(buffer);
  }

  // Get the handle to track callbacks.
  std::unique_lock<std::mutex> l(track_cb_lock_);
  if (track_cb_list_.count(track_id) != 0) {
    TrackCb callbacks = track_cb_list_[track_id];
    l.unlock();

    callbacks.data_cb(track_id, track_buffers, meta_buffers);
  } else {
    QMMF_ERROR("%s Track(%u) has not callback!", __func__, track_id);
  }
  QMMF_DEBUG("%s Exit ", __func__);
}

void RecorderClient::NotifyAudioTrackEvent(uint32_t track_id,
                                           EventType event_type,
                                           void *event_data,
                                           size_t event_data_size) {
  QMMF_DEBUG("%s Enter ", __func__);
  QMMF_VERBOSE("%s Track(%u): Received event type = %d", __func__, track_id,
    (int32_t) event_type);

  // Get the handle to track callbacks.
  std::unique_lock<std::mutex> l(track_cb_lock_);
  if (track_cb_list_.count(track_id) != 0) {
    TrackCb callbacks = track_cb_list_[track_id];
    l.unlock();

    QMMF_KPI_ASYNC_BEGIN("VideoAppCB", track_id);
    callbacks.event_cb(track_id, event_type, event_data, event_data_size);
  } else {
    QMMF_ERROR("%s Track(%u) has not callback!", __func__, track_id);
  }
  QMMF_DEBUG("%s Exit ", __func__);
}

void RecorderClient::NotifyCameraResult(uint32_t camera_id,
                                        const CameraMetadata &result) {
  if (nullptr != metadata_cb_) {
    metadata_cb_(camera_id, result);
  } else {
    QMMF_ERROR("%s No client registered result callback!\n", __func__);
  }
}

//Binder Proxy implementation of IRecoderService.
class BpRecorderService: public BpInterface<IRecorderService> {
 public:
  BpRecorderService(const sp<IBinder>& impl)
  : BpInterface<IRecorderService>(impl) {}

  status_t Connect(const sp<IRecorderServiceCallback>& service_cb,
                   uint32_t* client_id) {
    Parcel data, reply;
    data.writeInterfaceToken(IRecorderService::getInterfaceDescriptor());
    //Register service callback to get callbacks from recorder service.
    //eg : JPEG buffer, Tracks elementry buffers, Recorder/Session status
    //callbacks etc.
    data.writeStrongBinder(IInterface::asBinder(service_cb));
    remote()->transact(uint32_t(QMMF_RECORDER_SERVICE_CMDS::
                            RECORDER_CONNECT), data, &reply);
    *client_id = reply.readUint32();
    return reply.readInt32();
  }

  status_t Disconnect(const uint32_t client_id) {
    Parcel data, reply;
    data.writeInterfaceToken(IRecorderService::getInterfaceDescriptor());
    data.writeUint32(client_id);
    remote()->transact(uint32_t(QMMF_RECORDER_SERVICE_CMDS::
                            RECORDER_DISCONNECT), data, &reply);
    return reply.readInt32();
  }

  status_t StartCamera(const uint32_t client_id, const uint32_t camera_id,
                       const float frame_rate,
                       const CameraExtraParam& extra_param,
                       bool enable_result_cb) {
    Parcel data, reply;
    data.writeInterfaceToken(IRecorderService::getInterfaceDescriptor());
    data.writeUint32(client_id);
    data.writeUint32(camera_id);
    data.writeFloat(frame_rate);
    data.writeUint32(enable_result_cb ? 1 : 0);
    uint32_t extra_param_size = extra_param.Size();
    data.writeUint32(extra_param_size);
    const void *extra_data = extra_param.GetAndLock();
    android::Parcel::WritableBlob extra_blob;
    data.writeBlob(extra_param_size, false, &extra_blob);
    memset(extra_blob.data(), 0x0, extra_param_size);
    memcpy(extra_blob.data(), extra_data, extra_param_size);
    remote()->transact(uint32_t(QMMF_RECORDER_SERVICE_CMDS::
                            RECORDER_START_CAMERA), data, &reply);
    extra_param.ReturnAndUnlock(extra_data);
    extra_blob.release();
    return reply.readInt32();
  }

  status_t StopCamera(const uint32_t client_id, const uint32_t camera_id) {
    Parcel data, reply;
    data.writeInterfaceToken(IRecorderService::getInterfaceDescriptor());
    data.writeUint32(client_id);
    data.writeUint32(camera_id);
    remote()->transact(uint32_t(QMMF_RECORDER_SERVICE_CMDS::
                            RECORDER_STOP_CAMERA), data, &reply);
    return reply.readInt32();
  }

  status_t CreateSession(const uint32_t client_id, uint32_t *session_id) {
    Parcel data, reply;
    data.writeInterfaceToken(IRecorderService::getInterfaceDescriptor());
    data.writeUint32(client_id);
    remote()->transact(uint32_t(QMMF_RECORDER_SERVICE_CMDS::
                            RECORDER_CREATE_SESSION), data, &reply);
    uint32_t id;
    reply.readUint32(&id);
    *session_id = id;
    return reply.readInt32();
  }

  status_t DeleteSession(const uint32_t client_id, const uint32_t session_id) {
    Parcel data, reply;
    data.writeInterfaceToken(IRecorderService::getInterfaceDescriptor());
    data.writeUint32(client_id);
    assert(session_id != 0);
    data.writeUint32(session_id);
    remote()->transact(uint32_t(QMMF_RECORDER_SERVICE_CMDS::
                            RECORDER_DELETE_SESSION), data, &reply);
    return reply.readInt32();
  }

  status_t StartSession(const uint32_t client_id, const uint32_t session_id) {
    Parcel data, reply;
    data.writeInterfaceToken(IRecorderService::getInterfaceDescriptor());
    data.writeUint32(client_id);
    assert(session_id != 0);
    data.writeUint32(session_id);
    remote()->transact(uint32_t(QMMF_RECORDER_SERVICE_CMDS::
        RECORDER_START_SESSION), data, &reply);
    return reply.readInt32();
  }

  status_t StopSession(const uint32_t client_id, const uint32_t session_id,
                       bool do_flush) {
    Parcel data, reply;
    data.writeInterfaceToken(IRecorderService::getInterfaceDescriptor());
    data.writeUint32(client_id);
    assert(session_id != 0);
    data.writeUint32(session_id);
    data.writeInt32(do_flush);
    remote()->transact(uint32_t(QMMF_RECORDER_SERVICE_CMDS::
        RECORDER_STOP_SESSION), data, &reply);
    return reply.readInt32();
  }

  status_t PauseSession(const uint32_t client_id, const uint32_t session_id) {
    Parcel data, reply;
    data.writeInterfaceToken(IRecorderService::getInterfaceDescriptor());
    data.writeUint32(client_id);
    assert(session_id != 0);
    data.writeUint32(session_id);
    remote()->transact(uint32_t(QMMF_RECORDER_SERVICE_CMDS::
                            RECORDER_PAUSE_SESSION), data, &reply);
    return reply.readInt32();
  }

  status_t ResumeSession(const uint32_t client_id, const uint32_t session_id) {
    Parcel data, reply;
    data.writeInterfaceToken(IRecorderService::getInterfaceDescriptor());
    data.writeUint32(client_id);
    assert(session_id != 0);
    data.writeUint32(session_id);
    remote()->transact(uint32_t(QMMF_RECORDER_SERVICE_CMDS::
                            RECORDER_RESUME_SESSION), data, &reply);
    return reply.readInt32();
  }

  status_t GetNumberOfCameras(const uint32_t client_id,
                              SupportedCameras &cameras) {
    Parcel data, reply;
    data.writeInterfaceToken(IRecorderService::getInterfaceDescriptor());
    data.writeUint32(client_id);
    remote()->transact(uint32_t(QMMF_RECORDER_SERVICE_CMDS::
                       RECORDER_GET_NUMBER_OF_CAMERAS), data, &reply);
    uint32_t num_cameras;
    reply.readUint32(&num_cameras);
    CameraCapability camera;
    for (uint32_t i = 0; i < num_cameras; i++)  {
      uint32_t blob_size;
      reply.readUint32(&blob_size);
      android::Parcel::ReadableBlob blob;
      reply.readBlob(blob_size, &blob);
      memcpy(&camera, blob.data(), blob_size);
      cameras.push_back(camera);
      blob.release();
    }
    return reply.readInt32();
  }

  status_t GetSupportedPlugins(const uint32_t client_id,
                               SupportedPlugins *plugins) {
    Parcel data, reply;
    data.writeInterfaceToken(IRecorderService::getInterfaceDescriptor());
    data.writeUint32(client_id);
    remote()->transact(uint32_t(QMMF_RECORDER_SERVICE_CMDS::
                       RECORDER_GET_SUPPORTED_PLUGINS), data, &reply);
    uint32_t num_plugins;
    reply.readUint32(&num_plugins);
    for (uint32_t i = 0; i < num_plugins; i++)  {
      uint32_t blob_size;
      reply.readUint32(&blob_size);
      android::Parcel::ReadableBlob blob;
      reply.readBlob(blob_size, &blob);
      PluginInfo plugin(blob.data(), blob_size);
      plugins->push_back(plugin);
      blob.release();
    }
    return reply.readInt32();
  }

  status_t CreatePlugin(const uint32_t client_id, uint32_t *uid,
                        const PluginInfo &plugin) {
    Parcel data, reply;
    data.writeInterfaceToken(IRecorderService::getInterfaceDescriptor());
    data.writeUint32(client_id);
    uint32_t blob_size = plugin.Size();
    data.writeUint32(blob_size);
    android::Parcel::WritableBlob blob;
    data.writeBlob(blob_size, false, &blob);
    memset(blob.data(), 0x0, blob_size);
    memcpy(blob.data(), plugin.ToBlob().get(), blob_size);
    remote()->transact(uint32_t(QMMF_RECORDER_SERVICE_CMDS::
                       RECORDER_CREATE_PLUGIN), data, &reply);
    auto ret = reply.readInt32();
    *uid = reply.readUint32();
    blob.release();
    return ret;
  }

  status_t DeletePlugin(const uint32_t client_id, const uint32_t &uid) {
    Parcel data, reply;
    data.writeInterfaceToken(IRecorderService::getInterfaceDescriptor());
    data.writeUint32(client_id);
    data.writeUint32(uid);
    remote()->transact(uint32_t(QMMF_RECORDER_SERVICE_CMDS::
                       RECORDER_DELETE_PLUGIN), data, &reply);
    return reply.readInt32();
  }

  status_t ConfigPlugin(const uint32_t client_id, const uint32_t &uid,
                        const std::string &json_config) {
    Parcel data, reply;
    data.writeInterfaceToken(IRecorderService::getInterfaceDescriptor());
    data.writeUint32(client_id);
    data.writeUint32(uid);
    size_t blob_size = json_config.size();
    data.writeUint32(blob_size);
    android::Parcel::WritableBlob blob;
    data.writeBlob(blob_size, false, &blob);
    memset(blob.data(), 0x0, blob_size);
    memcpy(blob.data(), json_config.data(), blob_size);
    remote()->transact(uint32_t(QMMF_RECORDER_SERVICE_CMDS::
                       RECORDER_CONFIGURE_PLUGIN), data, &reply);
    blob.release();
    return reply.readInt32();
  }

  status_t ConfigPlugin(const uint32_t client_id,
                        const uint32_t &uid,
                        const int32_t type,
                        const std::vector<uint8_t> &blob_config) {
    Parcel data, reply;
    data.writeInterfaceToken(IRecorderService::getInterfaceDescriptor());
    data.writeUint32(client_id);
    data.writeUint32(uid);
    data.writeInt32(type);
    size_t blob_size = blob_config.size();
    data.writeUint32(blob_size);
    android::Parcel::WritableBlob blob;
    data.writeBlob(blob_size, false, &blob);
    memcpy(blob.data(), blob_config.data(), blob_size);
    remote()->transact(uint32_t(QMMF_RECORDER_SERVICE_CMDS::
                       RECORDER_CONFIGURE_PLUGIN_WITH_BLOB), data, &reply);
    blob.release();
    return reply.readInt32();
  }

  status_t GetPluginConfig(const uint32_t client_id, const uint32_t &uid,
                           std::string &json_config) {
    status_t ret;
    Parcel data, reply;
    data.writeInterfaceToken(IRecorderService::getInterfaceDescriptor());
    data.writeUint32(client_id);
    data.writeUint32(uid);
    remote()->transact(uint32_t(QMMF_RECORDER_SERVICE_CMDS::
                       RECORDER_GET_PLUGIN_CONFIG), data, &reply);
    uint32_t blob_size;
    ret = reply.readInt32();
    reply.readUint32(&blob_size);
    android::Parcel::ReadableBlob blob;
    reply.readBlob(blob_size, &blob);
    const char *string = reinterpret_cast<const char *>(blob.data());
    json_config.assign(string);
    return ret;
  }

  status_t CreateAudioTrack(const uint32_t client_id, const uint32_t session_id,
                            const uint32_t track_id,
                            const AudioTrackCreateParam& param) {
    QMMF_DEBUG("%s Enter", __func__);
    QMMF_VERBOSE("%s INPARAM: session_id[%u]", __func__, session_id);
    QMMF_VERBOSE("%s INPARAM: track_id[%u]", __func__, track_id);
    QMMF_VERBOSE("%s INPARAM: param[%s]", __func__,
                 param.ToString().c_str());
    Parcel data, reply;

    data.writeInterfaceToken(IRecorderService::getInterfaceDescriptor());
    data.writeUint32(client_id);
    data.writeUint32(session_id);
    data.writeUint32(track_id);
    AudioTrackCreateParamInternal(param).ToParcel(&data);

    remote()->transact(
        uint32_t(QMMF_RECORDER_SERVICE_CMDS::RECORDER_CREATE_AUDIOTRACK),
        data, &reply);

    return reply.readInt32();
  }

  status_t CreateVideoTrack(const uint32_t client_id,
                            const uint32_t session_id,
                            const uint32_t track_id,
                            const VideoTrackCreateParam& params) {
    Parcel data, reply;
    data.writeInterfaceToken(IRecorderService::getInterfaceDescriptor());
    data.writeUint32(client_id);
    data.writeUint32(session_id);
    data.writeUint32(track_id);
    uint32_t param_size = sizeof params;
    data.writeUint32(param_size);
    android::Parcel::WritableBlob blob;
    data.writeBlob(param_size, false, &blob);
    memset(blob.data(), 0x0, param_size);
    VideoTrackCreateParam* track_params =
        const_cast<VideoTrackCreateParam*>(&params);
    memcpy(blob.data(), reinterpret_cast<void*>(track_params), param_size);
    remote()->transact(uint32_t(QMMF_RECORDER_SERVICE_CMDS::
        RECORDER_CREATE_VIDEOTRACK), data, &reply);
    blob.release();
    return reply.readInt32();
  }

  status_t CreateVideoTrack(const uint32_t client_id,
                            const uint32_t session_id,
                            const uint32_t track_id,
                            const VideoTrackCreateParam& params,
                            const VideoExtraParam& extra_param) {
    Parcel data, reply;
    data.writeInterfaceToken(IRecorderService::getInterfaceDescriptor());
    data.writeUint32(client_id);
    data.writeUint32(session_id);
    data.writeUint32(track_id);
    uint32_t param_size = sizeof params;
    data.writeUint32(param_size);
    android::Parcel::WritableBlob blob;
    data.writeBlob(param_size, false, &blob);
    memset(blob.data(), 0x0, param_size);
    memcpy(blob.data(), &params, param_size);
    uint32_t extra_param_size = extra_param.Size();
    data.writeUint32(extra_param_size);
    const void *extra_data = extra_param.GetAndLock();
    android::Parcel::WritableBlob extra_blob;
    data.writeBlob(extra_param_size, false, &extra_blob);
    memset(extra_blob.data(), 0x0, extra_param_size);
    memcpy(extra_blob.data(), extra_data, extra_param_size);
    remote()->transact(uint32_t(QMMF_RECORDER_SERVICE_CMDS::
        RECORDER_CREATE_VIDEOTRACK_EXTRAPARAMS), data, &reply);
    extra_param.ReturnAndUnlock(extra_data);
    extra_blob.release();
    blob.release();
    return reply.readInt32();
  }

  status_t DeleteAudioTrack(const uint32_t client_id,
                            const uint32_t session_id,
                            const uint32_t track_id) {
    QMMF_DEBUG("%s Enter", __func__);
    QMMF_VERBOSE("%s INPARAM: session_id[%u]", __func__, session_id);
    QMMF_VERBOSE("%s INPARAM: track_id[%u]", __func__, track_id);
    Parcel data, reply;
    data.writeInterfaceToken(IRecorderService::getInterfaceDescriptor());
    data.writeUint32(client_id);
    data.writeUint32(session_id);
    data.writeUint32(track_id);

    remote()->transact(
        uint32_t(QMMF_RECORDER_SERVICE_CMDS::RECORDER_DELETE_AUDIOTRACK),
        data, &reply);

    return reply.readInt32();
  }

  status_t DeleteVideoTrack(const uint32_t client_id,
                            const uint32_t session_id,
                            const uint32_t track_id) {
    Parcel data, reply;
    data.writeInterfaceToken(IRecorderService::getInterfaceDescriptor());
    data.writeUint32(client_id);
    data.writeUint32(session_id);
    data.writeUint32(track_id);
    remote()->transact(uint32_t(QMMF_RECORDER_SERVICE_CMDS::
                            RECORDER_DELETE_VIDEOTRACK), data, &reply);
    return reply.readInt32();
  }

  status_t ReturnTrackBuffer(const uint32_t client_id,
                             const uint32_t session_id,
                             const uint32_t track_id,
                             std::vector<BnBuffer> &buffers) {

    QMMF_DEBUG("%s Enter", __func__);
    QMMF_VERBOSE("%s INPARAM: session_id[%u]", __func__, session_id);
    QMMF_VERBOSE("%s INPARAM: track_id[%u]", __func__, track_id);
    for (const BnBuffer& buffer : buffers) {
      QMMF_VERBOSE("%s INPARAM: buffers[%s]", __func__,
          buffer.ToString().c_str());
    }

    Parcel data, reply;
    data.writeInterfaceToken(IRecorderService::getInterfaceDescriptor());
    data.writeUint32(client_id);
    data.writeUint32(session_id);
    data.writeUint32(track_id);
    if (track_id < 100) {
      uint32_t size = buffers.size();
      assert(size > 0);
      data.writeUint32(size);
      // TODO: combine all BnBuffers together in single blob
      for (uint32_t i = 0; i < size; i++) {
        uint32_t param_size = sizeof (BnBuffer);
        data.writeUint32(param_size);
        android::Parcel::WritableBlob blob;
        data.writeBlob(param_size, false, &blob);
        memset(blob.data(), 0x0, param_size);
        buffers[i].ion_fd = buffers[i].ion_fd;
        buffers[i].ion_meta_fd = buffers[i].ion_meta_fd;
        memcpy(blob.data(), reinterpret_cast<void*>(&buffers[i]), param_size);
      }
    } else {
      data.writeInt32(static_cast<int32_t>(buffers.size()));
      for (const BnBuffer& buffer : buffers) {
        buffer.ToParcel(&data, false);
      }
    }

    remote()->transact(
        uint32_t(QMMF_RECORDER_SERVICE_CMDS::RECORDER_RETURN_TRACKBUFFER),
        data, &reply, IBinder::FLAG_ONEWAY);

    return NO_ERROR;
  }

  status_t SetAudioTrackParam(const uint32_t client_id,
                              const uint32_t session_id,
                              const uint32_t track_id,
                              CodecParamType type,
                              void *param,
                              size_t param_size) {
    Parcel data, reply;
    data.writeInterfaceToken(IRecorderService::getInterfaceDescriptor());
    data.writeUint32(client_id);
    data.writeUint32(session_id);
    data.writeUint32(track_id);
    data.writeUint32(static_cast<uint32_t>(type));
    data.writeUint32(param_size);
    android::Parcel::WritableBlob blob;
    data.writeBlob(param_size, false, &blob);
    memcpy(blob.data(), reinterpret_cast<void*>(param), param_size);
    remote()->transact(uint32_t(QMMF_RECORDER_SERVICE_CMDS::
                            RECORDER_SET_AUDIOTRACK_PARAMS), data, &reply);
    blob.release();
    return reply.readInt32();
  }

  status_t SetVideoTrackParam(const uint32_t client_id,
                              const uint32_t session_id,
                              const uint32_t track_id,
                              CodecParamType type,
                              void *param,
                              size_t param_size) {
    Parcel data, reply;
    data.writeInterfaceToken(IRecorderService::getInterfaceDescriptor());
    data.writeUint32(client_id);
    data.writeUint32(session_id);
    data.writeUint32(track_id);
    data.writeUint32(static_cast<uint32_t>(type));
    data.writeUint32(param_size);
    android::Parcel::WritableBlob blob;
    data.writeBlob(param_size, false, &blob);
    memcpy(blob.data(), reinterpret_cast<void*>(param), param_size);
    remote()->transact(uint32_t(QMMF_RECORDER_SERVICE_CMDS::
                            RECORDER_SET_VIDEOTRACK_PARAMS), data, &reply);
    blob.release();
    return reply.readInt32();
  }

  status_t CaptureImage(const uint32_t client_id, const uint32_t camera_id,
                        const ImageParam &param, const uint32_t num_images,
                        const std::vector<CameraMetadata> &meta) {
    Parcel data, reply;
    data.writeInterfaceToken(IRecorderService::getInterfaceDescriptor());
    data.writeUint32(client_id);
    data.writeUint32(camera_id);
    uint32_t param_size = sizeof param;
    data.writeUint32(param_size);
    android::Parcel::WritableBlob blob;
    data.writeBlob(param_size, false, &blob);
    memcpy(blob.data(), &param, param_size);
    data.writeUint32(num_images);
    data.writeUint32(meta.size());
    for (uint8_t i = 0; i < meta.size(); ++i) {
      meta[i].writeToParcel(&data);
    }
    remote()->transact(uint32_t(QMMF_RECORDER_SERVICE_CMDS::
        RECORDER_CAPTURE_IMAGE), data, &reply);
    return reply.readInt32();
  }

  status_t ConfigImageCapture(const uint32_t client_id,
                              const uint32_t camera_id,
                              const ImageConfigParam &config) {

    Parcel data, reply;
    data.writeInterfaceToken(IRecorderService::getInterfaceDescriptor());
    data.writeUint32(client_id);
    data.writeUint32(camera_id);
    uint32_t param_size = config.Size();
    data.writeUint32(param_size);
    const void *config_data = config.GetAndLock();
    android::Parcel::WritableBlob blob;
    data.writeBlob(param_size, false, &blob);
    memcpy(blob.data(), config_data, param_size);
    remote()->transact(uint32_t(QMMF_RECORDER_SERVICE_CMDS::
        RECORDER_CONFIG_IMAGECAPTURE), data, &reply);
    config.ReturnAndUnlock(config_data);
    blob.release();
    return reply.readInt32();
  }

  status_t CancelCaptureImage(const uint32_t client_id,
                              const uint32_t camera_id) {
    Parcel data, reply;
    data.writeInterfaceToken(IRecorderService::getInterfaceDescriptor());
    data.writeUint32(client_id);
    data.writeUint32(camera_id);
    remote()->transact(uint32_t(QMMF_RECORDER_SERVICE_CMDS::
                       RECORDER_CANCEL_IMAGECAPTURE), data, &reply);
    return reply.readInt32();
  }

  status_t ReturnImageCaptureBuffer(const uint32_t client_id,
                                    const uint32_t camera_id,
                                    const int32_t buffer_id) {
    Parcel data, reply;
    data.writeInterfaceToken(IRecorderService::getInterfaceDescriptor());
    data.writeUint32(client_id);
    data.writeUint32(camera_id);
    data.writeUint32(buffer_id);
    remote()->transact(uint32_t(QMMF_RECORDER_SERVICE_CMDS::
                       RECORDER_RETURN_IMAGECAPTURE_BUFFER), data, &reply);
    return reply.readInt32();
  }

  status_t SetCameraParam(const uint32_t client_id,
                          const uint32_t camera_id,
                          const CameraMetadata &meta) {
    Parcel data, reply;
    data.writeInterfaceToken(IRecorderService::getInterfaceDescriptor());
    data.writeUint32(client_id);
    data.writeUint32(camera_id);
    meta.writeToParcel(&data);
    remote()->transact(uint32_t(QMMF_RECORDER_SERVICE_CMDS::
                       RECORDER_SET_CAMERA_PARAMS), data, &reply);
    return reply.readInt32();
  }

  status_t GetCameraParam(const uint32_t client_id,
                          const uint32_t camera_id,
                          CameraMetadata &meta) {
    Parcel data, reply;
    data.writeInterfaceToken(IRecorderService::getInterfaceDescriptor());
    data.writeUint32(client_id);
    data.writeUint32(camera_id);
    remote()->transact(uint32_t(QMMF_RECORDER_SERVICE_CMDS::
                                RECORDER_GET_CAMERA_PARAMS), data, &reply);
    auto ret = reply.readInt32();
    if (NO_ERROR == ret) {
      ret = meta.readFromParcel(&reply);
    }
    return ret;
  }

  status_t GetDefaultCaptureParam(const uint32_t client_id,
                                  const uint32_t camera_id,
                                  CameraMetadata &meta) {
    Parcel data, reply;
    data.writeInterfaceToken(IRecorderService::getInterfaceDescriptor());
    data.writeUint32(client_id);
    data.writeUint32(camera_id);
    remote()->transact(uint32_t(QMMF_RECORDER_SERVICE_CMDS::
                                RECORDER_GET_DEFAULT_CAPTURE_PARAMS), data,
                                &reply);
    auto ret = reply.readInt32();
    if (NO_ERROR == ret) {
      ret = meta.readFromParcel(&reply);
    }
    return ret;
  }

  status_t GetCameraCharacteristics(const uint32_t client_id,
                                    const uint32_t camera_id,
                                    CameraMetadata &meta) {
    Parcel data, reply;
    data.writeInterfaceToken(IRecorderService::getInterfaceDescriptor());
    data.writeUint32(client_id);
    data.writeUint32(camera_id);
    remote()->transact(uint32_t(QMMF_RECORDER_SERVICE_CMDS::
                                RECORDER_GET_CAMERA_CHARACTERISTICS), data,
                                &reply);
    auto ret = reply.readInt32();
    if (NO_ERROR == ret) {
      ret = meta.readFromParcel(&reply);
    }
    return ret;
  }

  status_t CreateOverlayObject(const uint32_t client_id,
                               const uint32_t track_id, OverlayParam *params,
                               uint32_t *overlay_id) {
    Parcel data, reply;
    android::Parcel::WritableBlob image_blob;

    data.writeInterfaceToken(IRecorderService::getInterfaceDescriptor());
    data.writeUint32(client_id);
    data.writeUint32(track_id);
    uint32_t param_size = sizeof(OverlayParam);

    data.writeUint32(param_size);
    android::Parcel::WritableBlob blob;
    data.writeBlob(param_size, false, &blob);
    memset(blob.data(), 0x0, param_size);
    memcpy(blob.data(), reinterpret_cast<void*>(params), param_size);

    if (params->type ==  OverlayType::kStaticImage &&
        params->image_info.image_type == OverlayImageType::kBlobType) {
      data.writeUint32(params->image_info.image_size);
      data.writeBlob(params->image_info.image_size, false, &image_blob);
      memset(image_blob.data(), 0x0, params->image_info.image_size);
      memcpy(image_blob.data(), reinterpret_cast<void*>
          (params->image_info.image_buffer), params->image_info.image_size);
    }

    remote()->transact(uint32_t(QMMF_RECORDER_SERVICE_CMDS::
                            RECORDER_CREATE_OVERLAYOBJECT), data, &reply);
    blob.release();
    image_blob.release();
    *overlay_id = reply.readUint32();
    return reply.readInt32();;
  }

  status_t DeleteOverlayObject(const uint32_t client_id,
                               const uint32_t track_id,
                               const uint32_t overlay_id) {
    Parcel data, reply;
    data.writeInterfaceToken(IRecorderService::getInterfaceDescriptor());
    data.writeUint32(client_id);
    data.writeUint32(track_id);
    data.writeUint32(overlay_id);
    remote()->transact(uint32_t(QMMF_RECORDER_SERVICE_CMDS::
                            RECORDER_DELETE_OVERLAYOBJECT), data, &reply);
    return reply.readInt32();
  }

  status_t GetOverlayObjectParams(const uint32_t client_id,
                                  const uint32_t track_id,
                                  const uint32_t overlay_id,
                                  OverlayParam &param) {
    Parcel data, reply;
    data.writeInterfaceToken(IRecorderService::getInterfaceDescriptor());
    data.writeUint32(client_id);
    data.writeUint32(track_id);
    data.writeUint32(overlay_id);
    remote()->transact(uint32_t(QMMF_RECORDER_SERVICE_CMDS::
                          RECORDER_GET_OVERLAYOBJECT_PARAMS), data, &reply);
    auto ret = reply.readInt32();
    if (NO_ERROR == ret) {
        uint32_t param_size;
        reply.readUint32(&param_size);
        android::Parcel::ReadableBlob blob;
        reply.readBlob(param_size, &blob);
        memcpy(&param, blob.data(), param_size);
        blob.release();
    }
    return ret;
  }

  status_t UpdateOverlayObjectParams(const uint32_t client_id,
                                     const uint32_t track_id,
                                     const uint32_t overlay_id,
                                     OverlayParam *params) {
    Parcel data, reply;
    android::Parcel::WritableBlob image_blob;
    data.writeInterfaceToken(IRecorderService::getInterfaceDescriptor());
    data.writeUint32(client_id);
    data.writeUint32(track_id);
    data.writeUint32(overlay_id);
    uint32_t param_size = sizeof(OverlayParam);
    data.writeUint32(param_size);
    android::Parcel::WritableBlob blob;
    data.writeBlob(param_size, false, &blob);
    memset(blob.data(), 0x0, param_size);
    memcpy(blob.data(), reinterpret_cast<void*>(params), param_size);

    if (params->type ==  OverlayType::kStaticImage &&
        params->image_info.image_type == OverlayImageType::kBlobType &&
        params->image_info.buffer_updated == true) {
      data.writeUint32(params->image_info.image_size);
      data.writeBlob(params->image_info.image_size, false, &image_blob);
      memset(image_blob.data(), 0x0, params->image_info.image_size);
      memcpy(image_blob.data(), reinterpret_cast<void*>
          (params->image_info.image_buffer), params->image_info.image_size);
    }

    remote()->transact(uint32_t(QMMF_RECORDER_SERVICE_CMDS::
                       RECORDER_UPDATE_OVERLAYOBJECT_PARAMS), data, &reply);
    blob.release();
    image_blob.release();
    return reply.readInt32();
  }

  status_t SetOverlayObject(const uint32_t client_id,
                            const uint32_t track_id,
                            const uint32_t overlay_id) {
    Parcel data, reply;
    data.writeInterfaceToken(IRecorderService::getInterfaceDescriptor());
    data.writeUint32(client_id);
    data.writeUint32(track_id);
    data.writeUint32(overlay_id);
    remote()->transact(uint32_t(QMMF_RECORDER_SERVICE_CMDS::
                       RECORDER_SET_OVERLAYOBJECT), data, &reply);
    return reply.readInt32();
  }

  status_t RemoveOverlayObject(const uint32_t client_id,
                               const uint32_t track_id,
                               const uint32_t overlay_id) {
    Parcel data, reply;
    data.writeInterfaceToken(IRecorderService::getInterfaceDescriptor());
    data.writeUint32(client_id);
    data.writeUint32(track_id);
    data.writeUint32(overlay_id);
    remote()->transact(uint32_t(QMMF_RECORDER_SERVICE_CMDS::
                       RECORDER_REMOVE_OVERLAYOBJECT), data, &reply);
    return reply.readInt32();
  }

  status_t CreateMultiCamera(const uint32_t client_id,
                             const std::vector<uint32_t> camera_ids,
                             uint32_t *virtual_camera_id) {
    Parcel data, reply;
    data.writeInterfaceToken(IRecorderService::getInterfaceDescriptor());
    data.writeUint32(client_id);
    uint32_t vector_size = camera_ids.size();
    data.writeUint32(vector_size);
    for (uint8_t i = 0; i < vector_size; ++i) {
      data.writeUint32(camera_ids[i]);
    }
    remote()->transact(uint32_t(QMMF_RECORDER_SERVICE_CMDS::
                                RECORDER_CREATE_MULTICAMERA), data, &reply);
    *virtual_camera_id = reply.readUint32();
    return reply.readInt32();;
  }

  status_t ConfigureMultiCamera(const uint32_t client_id,
                                const uint32_t virtual_camera_id,
                                const MultiCameraConfigType type,
                                const void *param,
                                const uint32_t param_size) {
    Parcel data, reply;
    data.writeInterfaceToken(IRecorderService::getInterfaceDescriptor());
    data.writeUint32(client_id);
    data.writeUint32(virtual_camera_id);
    data.writeUint32(static_cast<uint32_t>(type));
    data.writeUint32(param_size);
    android::Parcel::WritableBlob blob;
    data.writeBlob(param_size, false, &blob);
    memcpy(blob.data(), param, param_size);
    remote()->transact(uint32_t(QMMF_RECORDER_SERVICE_CMDS::
                                RECORDER_CONFIGURE_MULTICAMERA), data, &reply);
    blob.release();
    return reply.readInt32();
  }

  status_t GetVendorTagDescriptor(sp<VendorTagDescriptor> &desc) {
    Parcel data, reply;
    data.writeInterfaceToken(IRecorderService::getInterfaceDescriptor());
    remote()->transact(uint32_t(QMMF_RECORDER_SERVICE_CMDS::
                                RECORDER_GET_VENDOR_TAG_DESCRIPTOR), data, &reply);
    auto ret = reply.readInt32();
    if (NO_ERROR == ret) {
      ret = desc->readFromParcel(&reply);
    }
    return ret;
  }
};

IMPLEMENT_META_INTERFACE(RecorderService, QMMF_RECORDER_SERVICE_NAME);

ServiceCallbackHandler::ServiceCallbackHandler(RecorderClient* client)
    : client_(client) {
    QMMF_GET_LOG_LEVEL();
    QMMF_DEBUG("%s Enter ", __func__);
    QMMF_DEBUG("%s Exit ", __func__);
}

ServiceCallbackHandler::~ServiceCallbackHandler() {
    QMMF_DEBUG("%s Enter ", __func__);
    QMMF_DEBUG("%s Exit ", __func__);
}

void ServiceCallbackHandler::NotifyRecorderEvent(EventType event_type,
                                                 void *event_data,
                                                 size_t event_data_size) {
  QMMF_DEBUG("%s Enter ", __func__);
  assert(client_ != nullptr);
  client_->NotifyRecorderEvent(event_type, event_data, event_data_size);
  QMMF_DEBUG("%s Exit ", __func__);
}

void ServiceCallbackHandler::NotifySessionEvent(EventType event_type,
                                                void *event_data,
                                                size_t event_data_size) {
    QMMF_DEBUG("%s Enter ", __func__);
    QMMF_DEBUG("%s Exit ", __func__);
}

void ServiceCallbackHandler::NotifySnapshotData(uint32_t camera_id,
                                                uint32_t image_sequence_count,
                                                BnBuffer& buffer,
                                                MetaData& meta_data) {
  assert(client_ != nullptr);
  client_->NotifySnapshotData(camera_id, image_sequence_count, buffer,
                              meta_data);
}


void ServiceCallbackHandler::NotifyVideoTrackData(uint32_t track_id,
                                                  std::vector<BnBuffer>&
                                                  bn_buffers,
                                                  std::vector<MetaData>&
                                                  meta_buffers) {

  QMMF_VERBOSE("%s Enter ", __func__);
  assert(client_ != nullptr);
  client_->NotifyVideoTrackData(track_id, bn_buffers, meta_buffers);
  QMMF_DEBUG("%s Exit ", __func__);
}

void ServiceCallbackHandler::NotifyVideoTrackEvent(uint32_t track_id,
                                                   EventType event_type,
                                                   void *event_data,
                                                   size_t event_data_size) {
  QMMF_DEBUG("%s Enter ", __func__);
  QMMF_DEBUG("%s Exit ", __func__);
}

void ServiceCallbackHandler::NotifyAudioTrackData(uint32_t track_id,
                                                  const std::vector<BnBuffer>&
                                                  bn_buffers,
                                                  const std::vector<MetaData>&
                                                  meta_buffers) {
  QMMF_DEBUG("%s Enter ", __func__);
  QMMF_VERBOSE("%s INPARAM: track_id[%u]", __func__, track_id);
  for (const BnBuffer& bn_buffer : bn_buffers)
    QMMF_VERBOSE("%s INPARAM: bn_buffer[%s]", __func__,
                 bn_buffer.ToString().c_str());
  assert(client_ != nullptr);

  client_->NotifyAudioTrackData(track_id, bn_buffers, meta_buffers);

  QMMF_DEBUG("%s Exit ", __func__);
}

void ServiceCallbackHandler::NotifyAudioTrackEvent(uint32_t track_id,
                                                   EventType event_type,
                                                   void *event_data,
                                                   size_t event_data_size) {
  QMMF_DEBUG("%s Enter ", __func__);
  QMMF_VERBOSE("%s INPARAM: track_id[%u]", __func__, track_id);
  QMMF_VERBOSE("%s INPARAM: event_type[%d]", __func__,
               static_cast<underlying_type<EventType>::type>(event_type));
  assert(client_ != nullptr);

  client_->NotifyAudioTrackEvent(track_id, event_type, event_data,
                                 event_data_size);

  QMMF_DEBUG("%s Exit ", __func__);
}

void ServiceCallbackHandler::NotifyCameraResult(uint32_t camera_id,
                                                const CameraMetadata &result) {
  assert(client_ != nullptr);
  client_->NotifyCameraResult(camera_id, result);
}

class BpRecorderServiceCallback: public BpInterface<IRecorderServiceCallback> {
 public:
  BpRecorderServiceCallback(const sp<IBinder>& impl)
     : BpInterface<IRecorderServiceCallback>(impl) {}

  ~BpRecorderServiceCallback() {
    track_buffers_map_.clear();
    //TODO: Expose DeleteTrack Api from Binder proxy and call it from service.
  }

  void NotifyRecorderEvent(EventType event_type, void *event_data,
                           size_t event_data_size) {

    QMMF_DEBUG("%s Enter ", __func__);
    Parcel data, reply;

    data.writeInterfaceToken(
        IRecorderServiceCallback::getInterfaceDescriptor());
    data.writeInt32(static_cast<underlying_type<EventType>::type>(event_type));
    data.writeUint32(event_data_size);

    android::Parcel::WritableBlob blob;
    if (event_data_size) {
      data.writeBlob(event_data_size, false, &blob);
      memset(blob.data(), 0x0, event_data_size);
      memcpy(blob.data(), event_data, event_data_size);
    }
    remote()->transact(
        uint32_t(RECORDER_SERVICE_CB_CMDS::RECORDER_NOTIFY_EVENT),
        data, &reply, IBinder::FLAG_ONEWAY);

    if (event_data_size) {
      blob.release();
    }
    QMMF_DEBUG("%s Exit ", __func__);
  }

  void NotifySessionEvent(EventType event_type, void *event_data,
                          size_t event_data_size) {

  }

  void NotifySnapshotData(uint32_t camera_id, uint32_t image_sequence_count,
                          BnBuffer& buffer, MetaData& meta_data) {

    Parcel data, reply;
    data.writeInterfaceToken(IRecorderServiceCallback::
        getInterfaceDescriptor());
    data.writeUint32(camera_id);
    data.writeUint32(image_sequence_count);
    data.writeFileDescriptor(buffer.ion_fd);
    data.writeFileDescriptor(buffer.ion_meta_fd);
    uint32_t size = sizeof buffer;
    data.writeUint32(size);
    android::Parcel::WritableBlob blob;
    data.writeBlob(size, false, &blob);
    memset(blob.data(), 0x0, size);
    memcpy(blob.data(), reinterpret_cast<void*>(&buffer), size);
    // Pack meta
    size = sizeof meta_data;
    data.writeUint32(size);
    android::Parcel::WritableBlob meta_blob;
    data.writeBlob(size, false, &meta_blob);
    memset(meta_blob.data(), 0x0, size);
    memcpy(meta_blob.data(), reinterpret_cast<void*>(&meta_data), size);

    remote()->transact(uint32_t(RECORDER_SERVICE_CB_CMDS::
        RECORDER_NOTIFY_SNAPSHOT_DATA), data, &reply, IBinder::FLAG_ONEWAY);

    blob.release();
    meta_blob.release();
  }

  void NotifyVideoTrackData(uint32_t track_id, std::vector<BnBuffer>& buffers,
                            std::vector<MetaData>& meta_buffers) {

    QMMF_VERBOSE("Bp%s: Enter", __func__);

    Parcel data, reply;
    data.writeInterfaceToken(IRecorderServiceCallback::
        getInterfaceDescriptor());

    data.writeUint32(track_id);
    data.writeUint32(buffers.size());

    for(uint32_t i = 0; i < buffers.size(); i++) {
      bool ismapped = false;
      {
        std::lock_guard<std::mutex> l(track_buffers_lock_);
        auto& buffer_ids = track_buffers_map_[track_id];

        // If ION fd has already been sent to client, no binder packing is
        // required, only index would be sufficient for client to get mapped
        // buffer from his own map.
        ismapped = (buffer_ids.count(buffers[i].buffer_id) != 0);

        QMMF_VERBOSE("Bp%s: buffers[%d].ion_fd=%d ismapped:%d",
            __func__, i, buffers[i].ion_fd, ismapped);
      }
      // If buffer has not been sent to client then pack the file descriptor
      // and provide hint about incoming fd.
      data.writeInt32(ismapped);

      if (!ismapped) {
        // Pack file descriptor.
        data.writeFileDescriptor(buffers[i].ion_fd);
        bool hasmetafd = (buffers[i].ion_meta_fd > 0);
        data.writeUint32(hasmetafd);
        if (hasmetafd) {
          data.writeFileDescriptor(buffers[i].ion_meta_fd);
        }
        {
          std::lock_guard<std::mutex> l(track_buffers_lock_);
          auto& buffer_ids = track_buffers_map_[track_id];
          buffer_ids.emplace(buffers[i].buffer_id);
        }
        QMMF_VERBOSE("%s: Bp: track_id=%d", __func__, track_id);
        QMMF_VERBOSE("%s: Bp: buffers[%d].ion_fd=%d mapping:%d", __func__,
            i, buffers[i].ion_fd, true);
      }
      uint32_t size = sizeof (BnBuffer);
      data.writeUint32(size);
      android::Parcel::WritableBlob blob;
      data.writeBlob(size, false, &blob);
      memset(blob.data(), 0x0, size);
      memcpy(blob.data(), reinterpret_cast<void*>(&buffers[i]), size);
    }
    // Pack meta
    data.writeUint32(meta_buffers.size());
    for(uint32_t i = 0; i < meta_buffers.size(); ++i) {
      uint32_t size = sizeof (MetaData);
      data.writeUint32(size);
      android::Parcel::WritableBlob meta_blob;
      data.writeBlob(size, false, &meta_blob);
      memset(meta_blob.data(), 0x0, size);
      memcpy(meta_blob.data(), reinterpret_cast<void*>(&meta_buffers[i]), size);
    }

    remote()->transact(
        uint32_t(RECORDER_SERVICE_CB_CMDS::RECORDER_NOTIFY_VIDEO_TRACK_DATA),
        data, &reply, IBinder::FLAG_ONEWAY);

    QMMF_VERBOSE("%s: Exit - Sent Message One Way!!", __func__);
  }

  void NotifyVideoTrackEvent(uint32_t track_id, EventType event_type,
                             void *event_data, size_t event_data_size) {

  }

  void NotifyAudioTrackData(uint32_t track_id,
                            const std::vector<BnBuffer>& buffers,
                            const std::vector<MetaData>& meta_buffers) {
    QMMF_DEBUG("%s Enter ", __func__);
    QMMF_VERBOSE("%s INPARAM: track_id[%u]", __func__, track_id);
    for (const BnBuffer& buffer : buffers)
      QMMF_VERBOSE("%s INPARAM: buffer[%s]", __func__,
                   buffer.ToString().c_str());
    Parcel data, reply;

    data.writeInterfaceToken(
        IRecorderServiceCallback::getInterfaceDescriptor());
    data.writeUint32(track_id);
    data.writeInt32(static_cast<int32_t>(buffers.size()));
    for (const BnBuffer& buffer : buffers)
      buffer.ToParcel(&data, true);

    remote()->transact(
        uint32_t(RECORDER_SERVICE_CB_CMDS::RECORDER_NOTIFY_AUDIO_TRACK_DATA),
        data, &reply, IBinder::FLAG_ONEWAY);

    QMMF_DEBUG("%s Exit ", __func__);
  }

  void NotifyAudioTrackEvent(uint32_t track_id, EventType event_type,
                             void *event_data, size_t event_data_size) {
    QMMF_DEBUG("%s Enter ", __func__);
    QMMF_VERBOSE("%s INPARAM: track_id[%u]", __func__, track_id);
    QMMF_VERBOSE("%s INPARAM: event_type[%d]", __func__,
                 static_cast<underlying_type<EventType>::type>(event_type));
    Parcel data, reply;

    data.writeInterfaceToken(
        IRecorderServiceCallback::getInterfaceDescriptor());
    data.writeUint32(track_id);
    data.writeInt32(static_cast<underlying_type<EventType>::type>(event_type));

    remote()->transact(
        uint32_t(RECORDER_SERVICE_CB_CMDS::RECORDER_NOTIFY_AUDIO_TRACK_EVENT),
        data, &reply, IBinder::FLAG_ONEWAY);

    QMMF_DEBUG("%s Exit ", __func__);
  }

  void NotifyCameraResult(uint32_t camera_id, const CameraMetadata &result) {
    Parcel data, reply;
    data.writeInterfaceToken(IRecorderServiceCallback::getInterfaceDescriptor());
    data.writeUint32(camera_id);
    result.writeToParcel(&data);
    remote()->transact(uint32_t(RECORDER_SERVICE_CB_CMDS::
                                RECORDER_NOTIFY_CAMERA_RESULT), data, &reply,
                                IBinder::FLAG_ONEWAY);
  }

  void NotifyDeleteVideoTrack(uint32_t track_id) {
    QMMF_VERBOSE("Bp%s: Enter", __func__);
    std::lock_guard<std::mutex> l(track_buffers_lock_);
    track_buffers_map_.erase(track_id);
    QMMF_VERBOSE("Bp%s: Exit", __func__);
  }

 private:
  // map <track_id , set <buffer_id> >
  std::map<uint32_t,  std::set<uint32_t> > track_buffers_map_;
  // to protect track_buffers_map_
  std::mutex  track_buffers_lock_;
};

IMPLEMENT_META_INTERFACE(RecorderServiceCallback,
                            "recorder.service.IRecorderServiceCallback");

status_t BnRecorderServiceCallback::onTransact(uint32_t code,
                                               const Parcel& data,
                                               Parcel* reply,
                                               uint32_t flags) {
  QMMF_DEBUG("%s: Enter:(BnRecorderServiceCallback::onTransact)",
      __func__);
  CHECK_INTERFACE(IRecorderServiceCallback, data, reply);

  switch(code) {
    case RECORDER_SERVICE_CB_CMDS::RECORDER_NOTIFY_EVENT: {
      uint32_t event_data_size;
      int32_t event_type;

      data.readInt32(&event_type);
      data.readUint32(&event_data_size);

      android::Parcel::ReadableBlob blob;
      void* event_data = nullptr;
      if (event_data_size) {
        data.readBlob(event_data_size, &blob);
        event_data = const_cast<void*>(blob.data());
      }
      NotifyRecorderEvent(static_cast<EventType>(event_type), event_data,
                          event_data_size);
      if (event_data_size) {
        blob.release();
      }
      return NO_ERROR;
    }
    break;
    case RECORDER_SERVICE_CB_CMDS::RECORDER_NOTIFY_SESSION_EVENT: {
      //TODO:
      return NO_ERROR;
    }
    break;
    case RECORDER_SERVICE_CB_CMDS::RECORDER_NOTIFY_SNAPSHOT_DATA: {
      uint32_t camera_id, count, size;
      data.readUint32(&camera_id);
      data.readUint32(&count);
      uint32_t ion_fd = dup(data.readFileDescriptor());
      uint32_t ion_meta_fd = dup(data.readFileDescriptor());
      data.readUint32(&size);
      android::Parcel::ReadableBlob blob;
      data.readBlob(size, &blob);
      void* buf = const_cast<void*>(blob.data());
      BnBuffer bn_buffer{};
      memcpy(&bn_buffer, buf, size);
      bn_buffer.ion_fd = ion_fd;
      bn_buffer.ion_meta_fd = ion_meta_fd;
      uint32_t meta_size;
      MetaData meta_data{};
      android::Parcel::ReadableBlob meta_blob;
      data.readUint32(&meta_size);
      if (meta_size > 0) {
        data.readBlob(meta_size, &meta_blob);
        void* meta = const_cast<void*>(meta_blob.data());
        memcpy(&meta_data, meta, meta_size);
      }
      NotifySnapshotData(camera_id, count, bn_buffer, meta_data);
      blob.release();
      if (meta_size > 0) {
        meta_blob.release();
      }
      return NO_ERROR;
    }
    break;
    case RECORDER_SERVICE_CB_CMDS::RECORDER_NOTIFY_VIDEO_TRACK_DATA: {

      uint32_t track_id, vector_size;
      std::vector<BnBuffer> buffers;
      data.readUint32(&track_id);
      data.readUint32(&vector_size);
      QMMF_VERBOSE("Bn%s: vector_size=%d", __func__, vector_size);
      uint32_t size = 0;
      for (uint32_t i = 0; i < vector_size; i++)  {
        int32_t ismapped = 0, hasmetafd = 0;
        int32_t ion_fd = -1, ion_meta_fd = -1;
        data.readInt32(&ismapped);
        if (ismapped == 0) {
          ion_fd = dup(data.readFileDescriptor());
          data.readInt32(&hasmetafd);
          if (hasmetafd == 1) {
            ion_meta_fd = dup(data.readFileDescriptor());
          }
        }
        data.readUint32(&size);
        android::Parcel::ReadableBlob blob;
        data.readBlob(size, &blob);
        void* buffer = const_cast<void*>(blob.data());
        BnBuffer track_buffer;
        memcpy(&track_buffer, buffer, size);
        track_buffer.ion_fd = ion_fd;
        track_buffer.ion_meta_fd = ion_meta_fd;
        buffers.push_back(track_buffer);
        blob.release();
      }
      uint32_t meta_vector_size = 0;
      std::vector<MetaData> meta_buffers;
      data.readUint32(&meta_vector_size);
      QMMF_VERBOSE("%s: Bn: meta_vector_size=%d", __func__,
          meta_vector_size);
      android::Parcel::ReadableBlob meta_blob;
      for (uint32_t i = 0; i < meta_vector_size; i++)  {
        data.readUint32(&size);
        data.readBlob(size, &meta_blob);
        void* buffer = const_cast<void*>(meta_blob.data());
        MetaData meta_data;
        memcpy(&meta_data, buffer, size);
        meta_buffers.push_back(meta_data);
        meta_blob.release();
      }
      NotifyVideoTrackData(track_id, buffers, meta_buffers);
      return NO_ERROR;
    }
    break;
    case RECORDER_SERVICE_CB_CMDS::RECORDER_NOTIFY_VIDEO_TRACK_EVENT: {
      //TODO:
      return NO_ERROR;
    }
    break;
    case RECORDER_SERVICE_CB_CMDS::RECORDER_NOTIFY_AUDIO_TRACK_DATA: {
      uint32_t track_id = data.readUint32();
      size_t num_buffers = static_cast<size_t>(data.readInt32());
      std::vector<BnBuffer> buffers;
      for (size_t index = 0; index < num_buffers; ++index) {
        BnBuffer buffer;
        buffer.FromParcel(data, true);
        buffers.push_back(buffer);
      }
      QMMF_DEBUG("%s-NotifyAudioTrackData() TRACE", __func__);
      QMMF_VERBOSE("%s-NotifyAudioTrackData() INPARAM: track_id[%u]",
                   __func__, track_id);
      for (const BnBuffer& buffer : buffers)
        QMMF_VERBOSE("%s-NotifyAudioTrackData() INPARAM: buffer[%s]",
                   __func__, buffer.ToString().c_str());
      //TODO: Current implementation of audio is not using meta_data, add
      // support to pack meta data at proxy side, and unpack at Bn side.
      std::vector<MetaData> meta_buffers;
      NotifyAudioTrackData(track_id, buffers, meta_buffers);
      return NO_ERROR;
    }
    break;
    case RECORDER_SERVICE_CB_CMDS::RECORDER_NOTIFY_AUDIO_TRACK_EVENT: {
      uint32_t track_id = data.readUint32();
      EventType event_type = static_cast<EventType>(data.readInt32());

      QMMF_DEBUG("%s-NotifyAudioTrackEvent() TRACE", __func__);
      QMMF_VERBOSE("%s-NotifyAudioTrackEvent() INPARAM: track_id[%u]",
                   __func__, track_id);
      QMMF_VERBOSE("%s-NotifyAudioTrackEvent() INPARAM: event_type[%d]",
                   __func__,
                   static_cast<underlying_type<EventType>::type>(event_type));
      NotifyAudioTrackEvent(track_id, event_type, nullptr, 0);

      return NO_ERROR;
    }
    break;
    case RECORDER_SERVICE_CB_CMDS::RECORDER_NOTIFY_CAMERA_RESULT: {
      camera_metadata *meta = nullptr;
      uint32_t camera_id = data.readUint32();
      auto ret = CameraMetadata::readFromParcel(data, &meta);
      if ((NO_ERROR == ret) && (nullptr != meta)) {
        CameraMetadata result(meta);
        NotifyCameraResult(camera_id, result);
      } else {
        QMMF_ERROR("%s Failed to read camera result from parcel: %d\n",
                     __func__, ret);
        if (nullptr != meta) {
          free_camera_metadata(meta);
          meta = nullptr;
        }
      }
      return NO_ERROR;
    }
    break;
    default: {
      QMMF_ERROR("%s Method not supported ", __func__);
    }
    break;
  }
  return NO_ERROR;
}

}; //namespace qmmf

}; //namespace recorder
