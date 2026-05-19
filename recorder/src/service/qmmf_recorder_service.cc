/*
 * Copyright (c) 2016-2021, The Linux Foundation. All rights reserved.
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
 *
 * Changes from Qualcomm Innovation Center, Inc. are provided under the following license:
 *
 * Copyright (c) 2021-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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

#define LOG_TAG "RecorderService"

#include "recorder/src/service/qmmf_recorder_service.h"

namespace qmmf {

namespace recorder {

RecorderService::RecorderService() {

  QMMF_GET_LOG_LEVEL();
  QMMF_KPI_GET_MASK();

  QMMF_INFO("%s: Enter", __func__);

  int32_t n_preload = Property::Get("persist.qmmf.preload.cameras", 0);

  // Preload the recorder at bootup.
  if (n_preload != 0) {
    recorder_.reset(RecorderImpl::CreateRecorder());
    if (!recorder_) {
      QMMF_ERROR("%s: Can't create Recorder Instance!!", __func__);
    } else {
      std::function< const sp<RemoteCallBack>& (uint32_t id)>
        remote_cb_handle = [&] (uint32_t id) {
          QMMF_VERBOSE("%s: Client(%u): RemoteCallback request!", __func__, id);
          assert(remote_cb_list_.count(id) != 0);
          return remote_cb_list_[id];
      };
      auto ret = recorder_->Init(remote_cb_handle);
      if (ret != NO_ERROR) {
        QMMF_ERROR("%s: Recorder Initialization failed!", __func__);
        recorder_.reset();
      }
    }
    QMMF_INFO("%s: RecorderService Instantiated! ", __func__);
  }

  QMMF_INFO("%s: Exit", __func__);
  QMMF_KPI_DETAIL();
}

RecorderService::~RecorderService() {

  QMMF_INFO("%s: Enter ", __func__);
  QMMF_INFO("%s: Exit ", __func__);
  QMMF_KPI_DETAIL();
}

status_t RecorderService::onTransact(uint32_t code, const Parcel& data,
                                     Parcel* reply, uint32_t flag) {

  QMMF_DEBUG("%s: Enter:(BnRecorderService::onTransact)", __func__);
  CHECK_INTERFACE(IRecorderService, data, reply);
  int32_t ret = 0;

  switch (code) {
    case RECORDER_CONNECT: {
      sp<IRecorderServiceCallback> client_cb_handle = interface_cast
          <IRecorderServiceCallback>(data.readStrongBinder());
      uint32_t client_id;
      ret = Connect(client_cb_handle, &client_id);
      reply->writeUint32(client_id);
      reply->writeInt32(ret);
      return NO_ERROR;
    }
    break;
      case RECORDER_DISCONNECT: {
        uint32_t client_id;
        data.readUint32(&client_id);
        ret = Disconnect(client_id);
        reply->writeInt32(ret);
        return NO_ERROR;
      }
      break;
      case RECORDER_START_CAMERA: {
        uint32_t camera_id, enable_flag;
        bool enable_result_cb;
        uint32_t client_id;
        float framerate;
        data.readUint32(&client_id);
        data.readUint32(&camera_id);
        data.readFloat(&framerate);
        data.readUint32(&enable_flag);
        enable_result_cb = (1 == enable_flag) ? true : false;
        uint32_t extra_blob_size;
        android::Parcel::ReadableBlob extra_blob;
        data.readUint32(&extra_blob_size);
        data.readBlob(extra_blob_size, &extra_blob);
        CameraExtraParam extra_param(extra_blob.data(), extra_blob_size);
        ret = StartCamera(client_id, camera_id, framerate,
                          extra_param, enable_result_cb);
        extra_blob.release();
        reply->writeInt32(ret);
        return NO_ERROR;
      }
      break;
      case RECORDER_STOP_CAMERA: {
        uint32_t client_id, camera_id;
        data.readUint32(&client_id);
        data.readUint32(&camera_id);
        ret = StopCamera(client_id, camera_id);
        reply->writeInt32(ret);
        return NO_ERROR;
      }
      break;
      case RECORDER_CREATE_VIDEOTRACK: {
        uint32_t client_id, track_id;
        uint32_t blob_size, extra_blob_size;
        data.readUint32(&client_id);
        data.readUint32(&track_id);
        android::Parcel::ReadableBlob blob;
        data.readUint32(&blob_size);
        data.readBlob(blob_size, &blob);
        android::Parcel::ReadableBlob extra_blob;
        data.readUint32(&extra_blob_size);
        data.readBlob(extra_blob_size, &extra_blob);
        VideoTrackParam video_track_param;
        assert(blob_size == sizeof(video_track_param));
        memcpy(&video_track_param, blob.data(), blob_size);

        VideoExtraParam xtraparam(extra_blob.data(), extra_blob_size);
        ret = CreateVideoTrack(client_id, track_id,
                               video_track_param, xtraparam);
        blob.release();
        extra_blob.release();
        reply->writeInt32(ret);
        return NO_ERROR;
      }
      break;
      case RECORDER_DELETE_VIDEOTRACK: {
        uint32_t client_id, track_id;
        data.readUint32(&client_id);
        data.readUint32(&track_id);
        ret = DeleteVideoTrack(client_id, track_id);
        reply->writeInt32(ret);
        return NO_ERROR;
      }
      break;
      case RECORDER_START_VIDEOTRACKS: {
        uint32_t client_id, n_tracks, id;
        std::unordered_set<uint32_t> track_ids;
        data.readUint32(&client_id);
        data.readUint32(&n_tracks);
        for (uint32_t idx = 0; idx < n_tracks; idx++)  {
          data.readUint32(&id);
          track_ids.emplace(id);
        }
        ret = StartVideoTracks(client_id, track_ids);
        reply->writeInt32(ret);
        return NO_ERROR;
      }
      break;
      case RECORDER_STOP_VIDEOTRACKS: {
        uint32_t client_id, n_tracks, id;
        std::unordered_set<uint32_t> track_ids;
        data.readUint32(&client_id);
        data.readUint32(&n_tracks);
        for (uint32_t idx = 0; idx < n_tracks; idx++)  {
          data.readUint32(&id);
          track_ids.emplace(id);
        }
        ret = StopVideoTracks(client_id, track_ids);
        reply->writeInt32(ret);
        return NO_ERROR;
      }
      break;
      case RECORDER_RETURN_TRACKBUFFER: {
        uint32_t client_id, track_id;
        data.readUint32(&client_id);
        data.readUint32(&track_id);

        std::vector<BnBuffer> buffers;
        if (track_id < 100) {
          uint32_t vector_size;
          data.readUint32(&vector_size);
          for (uint32_t i = 0; i < vector_size; i++)  {
            uint32_t size;
            data.readUint32(&size);
            android::Parcel::ReadableBlob blob;
            data.readBlob(size, &blob);
            void* buffer = const_cast<void*>(blob.data());
            BnBuffer track_buffer;
            assert(size == sizeof(track_buffer));
            memcpy(&track_buffer, buffer, size);
            buffers.push_back(track_buffer);
            blob.release();
          }
        } else {
          size_t num_buffers = data.readInt32();
          for (size_t index = 0; index < num_buffers; ++index) {
            BnBuffer buffer;
            buffer.FromParcel(data, false);
            buffers.push_back(buffer);
          }
        }
        ret = ReturnTrackBuffer(client_id, track_id, buffers);
        reply->writeInt32(ret);
        return NO_ERROR;
      }
      break;
      case RECORDER_SET_VIDEOTRACK_PARAMS: {
        uint32_t client_id, track_id;
        uint32_t param_type, blob_size;
        data.readUint32(&client_id);
        data.readUint32(&track_id);
        data.readUint32(&param_type);
        data.readUint32(&blob_size);
        android::Parcel::ReadableBlob blob;
        data.readBlob(blob_size, &blob);
        void* param = const_cast<void*>(blob.data());
        ret = SetVideoTrackParam(client_id, track_id,
                                 static_cast<VideoParam>(param_type),
                                 param, blob_size);
        reply->writeInt32(ret);
        blob.release();
        return NO_ERROR;
      }
      break;
      case RECORDER_DYNAMIC_CAPTURE_IMAGE: {
        uint32_t client_id, camera_id, type, n_burst, meta_size, pad_group_size,
            pad;
        std::vector<std::vector<uint32_t>> pad_group;
        data.readUint32(&client_id);
        data.readUint32(&camera_id);
        data.readUint32(&pad_group_size);

        pad_group.resize(pad_group_size);
        for (int32_t i = 0; i < pad_group_size; i++) {
          uint32_t pad_size;
          data.readUint32(&pad_size);
          for (int32_t j = 0; j < pad_size; j++) {
            data.readUint32(&pad);
            pad_group[i].push_back(pad);
          }
        }

        for (auto &group : pad_group) {
          for (auto &pad : group) {
            QMMF_ERROR("%s pad = %d", __func__, pad);
          }
        }

        data.readUint32(&type);
        data.readUint32(&n_burst);
        data.readUint32(&meta_size);
        std::vector<CameraMetadata> meta_array;
        for (uint32_t i = 0; i < meta_size; ++i) {
          CameraMetadata meta;
          camera_metadata_t *m = nullptr;
          ret = meta.readFromParcel(data, &m);
          if ((NO_ERROR != ret) || (nullptr == m)) {
            QMMF_ERROR("%s: Metadata parcel read failed: %d meta(%p)", __func__,
                       ret, m);
            reply->writeInt32(ret);
            return ret;
          }
          meta.clear();
          meta.append(m);
          meta_array.push_back(meta);
          // We need to release this memory as meta.append() makes copy of this
          // memory
          free(m);
        }
        ret = CaptureImage(client_id, camera_id, pad_group,
                                  static_cast<SnapshotType>(type), n_burst,
                                  meta_array);

        // Clear the metadata buffers and free all storage used by it
        for (auto& meta : meta_array) {
          meta.clear();
        }

        reply->writeInt32(ret);
        return NO_ERROR;

      } break;
      case RECORDER_CAPTURE_IMAGE: {
        uint32_t client_id, camera_id, type, n_images, meta_size;
        data.readUint32(&client_id);
        data.readUint32(&camera_id);
        data.readUint32(&type);
        data.readUint32(&n_images);
        data.readUint32(&meta_size);
        std::vector<CameraMetadata> meta_array;
        for (uint32_t i = 0; i < meta_size; ++i) {
          CameraMetadata meta;
          camera_metadata_t *m = nullptr;
          ret = meta.readFromParcel(data, &m);
          if ((NO_ERROR != ret) || (nullptr == m)) {
            QMMF_ERROR("%s: Metadata parcel read failed: %d meta(%p)",
                __func__, ret, m);
            reply->writeInt32(ret);
            return ret;
          }
          meta.clear();
          meta.append(m);
          meta_array.push_back(meta);
          //We need to release this memory as meta.append() makes copy of this memory
          free(m);
        }
        ret = CaptureImage(client_id, camera_id,
                           static_cast<SnapshotType>(type), n_images,
                           meta_array);

        // Clear the metadata buffers and free all storage used by it
        for (auto meta:meta_array) {
          meta.clear();
        }

        reply->writeInt32(ret);
        return NO_ERROR;
      }
      break;
      case RECORDER_CONFIG_IMAGECAPTURE: {
        uint32_t client_id, camera_id, image_id, img_param_blob_size, blob_size;
        data.readUint32(&client_id);
        data.readUint32(&camera_id);
        data.readUint32(&image_id);
        data.readUint32(&img_param_blob_size);
        android::Parcel::ReadableBlob img_param_blob;
        data.readBlob(img_param_blob_size, &img_param_blob);
        ImageParam param;
        assert(img_param_blob_size == sizeof(param));
        memcpy(&param, img_param_blob.data(), img_param_blob_size);
        data.readUint32(&blob_size);
        android::Parcel::ReadableBlob blob;
        data.readBlob(blob_size, &blob);
        ImageExtraParam xtraparam(blob.data(), blob_size);
        ret = ConfigImageCapture(client_id, camera_id, image_id, param, xtraparam);
        reply->writeInt32(ret);
        blob.release();
        img_param_blob.release();
        return NO_ERROR;
      }
      break;
      case RECORDER_CANCEL_IMAGECAPTURE: {
        uint32_t client_id, camera_id, image_id, cache;
        data.readUint32(&client_id);
        data.readUint32(&camera_id);
        data.readUint32(&image_id);
        data.readUint32(&cache);
        ret = CancelCaptureImage(client_id, camera_id, image_id, cache);
        reply->writeInt32(ret);
        return NO_ERROR;
      }
      break;
      case  RECORDER_RETURN_IMAGECAPTURE_BUFFER: {
        uint32_t client_id, camera_id, buffer_id;
        data.readUint32(&client_id);
        data.readUint32(&camera_id);
        data.readUint32(&buffer_id);
        ret = ReturnImageCaptureBuffer(client_id, camera_id, buffer_id);
        reply->writeInt32(ret);
        return NO_ERROR;
      }
      break;
      case RECORDER_SET_CAMERA_PARAMS: {
        uint32_t client_id, camera_id;
        data.readUint32(&client_id);
        data.readUint32(&camera_id);
        CameraMetadata meta;
        camera_metadata_t *m = nullptr;
        ret = meta.readFromParcel(data, &m);
        if ((NO_ERROR != ret) || (nullptr == m)) {
          QMMF_ERROR("%s: Metadata parcel read failed: %d meta: %p\n",
              __func__, ret, m);
          reply->writeInt32(ret);
          return ret;
        }
        meta.clear();
        meta.append(m);
        ret = SetCameraParam(client_id, camera_id, meta);

        // Clear the metadata buffer and free all storage used by it
        meta.clear();
        //We need to release this memory as meta.append() makes copy of this memory
        free(m);
        reply->writeInt32(ret);
        return NO_ERROR;
      }
      break;
      case RECORDER_GET_CAMERA_PARAMS: {
        uint32_t client_id, camera_id;
        data.readUint32(&client_id);
        data.readUint32(&camera_id);
        CameraMetadata meta;
        ret = GetCameraParam(client_id, camera_id, meta);
        reply->writeInt32(ret);
        if (NO_ERROR == ret) {
          ret = meta.writeToParcel(reply);
          if (NO_ERROR != ret) {
            QMMF_ERROR("%s: Metadata parcel write failed: %d\n",
                       __func__, ret);
          }
        }
        meta.clear();
        return NO_ERROR;
      }
      break;
      case RECORDER_SET_CAMERA_SESSION_PARAMS: {
        uint32_t client_id, camera_id;
        data.readUint32(&client_id);
        data.readUint32(&camera_id);
        CameraMetadata meta;
        camera_metadata_t *m = nullptr;
        ret = meta.readFromParcel(data, &m);
        if ((NO_ERROR != ret) || (nullptr == m)) {
          QMMF_ERROR("%s: Metadata parcel read failed: %d meta: %p\n",
              __func__, ret, m);
          reply->writeInt32(ret);
          return ret;
        }
        meta.clear();
        meta.append(m);
        ret = SetCameraSessionParam(client_id, camera_id, meta);

        // Clear the metadata buffer and free all storage used by it
        meta.clear();
        //We need to release this memory as meta.append() makes copy of this memory
        free(m);
        reply->writeInt32(ret);
        return NO_ERROR;
      }
      break;
      case RECORDER_SET_SHDR: {
        uint32_t client_id, camera_id;
        int32_t enable;
        data.readUint32(&client_id);
        data.readUint32(&camera_id);
        data.readInt32(&enable);
        ret = SetSHDR(client_id, camera_id, enable);
        reply->writeInt32(ret);
        return NO_ERROR;
      }
      break;
      case RECORDER_GET_DEFAULT_CAPTURE_PARAMS: {
        uint32_t client_id, camera_id;
        data.readUint32(&client_id);
        data.readUint32(&camera_id);
        CameraMetadata meta;
        ret = GetDefaultCaptureParam(client_id, camera_id, meta);
        reply->writeInt32(ret);
        if (NO_ERROR == ret) {
          ret = meta.writeToParcel(reply);
          if (NO_ERROR != ret) {
            QMMF_ERROR("%s: Metadata parcel write failed: %d\n",
                       __func__, ret);
          }
        }
        meta.clear();
        return NO_ERROR;
      }
      break;
      case RECORDER_GET_CAMERA_CHARACTERISTICS: {
        uint32_t client_id, camera_id;
        data.readUint32(&client_id);
        data.readUint32(&camera_id);
        CameraMetadata meta;
        ret = GetCameraCharacteristics(client_id, camera_id, meta);
        reply->writeInt32(ret);
        if (NO_ERROR == ret) {
          ret = meta.writeToParcel(reply);
          if (NO_ERROR != ret) {
            QMMF_ERROR("%s: Metadata parcel write failed: %d\n",
                       __func__, ret);
          }
        }
        meta.clear();
        return NO_ERROR;
      }
      break;
      case RECORDER_GET_STATIC_CAMERA_INFO: {
        uint32_t client_id;
        data.readUint32(&client_id);
        std::vector<CameraMetadata> meta;
        ret = GetCamStaticInfo(client_id, meta);
        reply->writeInt32(ret);
        reply->writeUint32(meta.size());
        for (uint8_t i = 0; i < meta.size(); ++i) {
          meta[i].writeToParcel(reply);
        }
        return 0;
      }
      break;
      case RECORDER_GET_VENDOR_TAG_DESCRIPTOR: {
        std::shared_ptr<VendorTagDescriptor> desc;
        ret = GetVendorTagDescriptor(desc);
        reply->writeInt32(ret);
        if (NO_ERROR == ret) {
          ret = desc->writeToParcel(reply);
          if (NO_ERROR != ret) {
            QMMF_ERROR("%s: VendorTagDescriptor parcel write failed: %d\n",
                       __func__, ret);
          }
        }
        desc.reset();
        return NO_ERROR;
      }
      break;
      case RECORDER_GET_OFFLINE_PARAMS: {
        uint32_t client_id, in_params_blob_size;
        data.readUint32(&client_id);
        data.readUint32(&in_params_blob_size);
        android::Parcel::ReadableBlob in_params_blob;
        data.readBlob(in_params_blob_size, &in_params_blob);
        OfflineCameraInputParams in_params;
        assert(in_params_blob_size == sizeof(in_params));
        memcpy(&in_params, in_params_blob.data(), in_params_blob_size);

        OfflineCameraOutputParams out_params;
        ret = GetOfflineParams(client_id, in_params, out_params);
        reply->writeInt32(ret);
        if (NO_ERROR == ret) {
          uint32_t out_params_blob_size = sizeof(out_params);
          reply->writeUint32(out_params_blob_size);
          android::Parcel::WritableBlob blob;
          reply->writeBlob(out_params_blob_size, false, &blob);
          memcpy(blob.data(), &out_params, out_params_blob_size);
        }
        return NO_ERROR;
      }
      break;
      case RECORDER_CONFIGURE_OFFLINE_PROC: {
        uint32_t client_id, proc_params_blob_size;
        data.readUint32(&client_id);
        data.readUint32(&proc_params_blob_size);
        android::Parcel::ReadableBlob proc_params_blob;
        data.readBlob(proc_params_blob_size, &proc_params_blob);
        OfflineCameraCreateParams params;
        assert(proc_params_blob_size == sizeof(params) -
            sizeof(CameraMetadata) * 2);
        memcpy(&params, proc_params_blob.data(), proc_params_blob_size);

        camera_metadata_t *m0 = nullptr;
        camera_metadata_t *m1 = nullptr;
        ret = params.session_meta[0].readFromParcel(data, &m0);
        if (NO_ERROR != ret) {
          QMMF_ERROR("%s: Metadata parcel read failed: %d meta: %p\n",
              __func__, ret, m0);
          reply->writeInt32(ret);
          return ret;
        }
        ret = params.session_meta[1].readFromParcel(data, &m1);
        if (NO_ERROR != ret) {
          QMMF_ERROR("%s: Metadata parcel read failed: %d meta: %p\n",
              __func__, ret, m1);
          reply->writeInt32(ret);
          return ret;
        }
        params.session_meta[0].clear();
        params.session_meta[1].clear();
        if (m0) {
          params.session_meta[0].append(m0);
        }
        if (m1) {
          params.session_meta[1].append(m1);
        }
        ret = CreateOfflineProcess(client_id, params);

        // Clear the metadata buffer and free all storage used by it
        params.session_meta[0].clear();
        params.session_meta[1].clear();
        //We need to release this memory as meta.append() makes copy of this memory
        if (m0) {
          free(m0);
        }
        if (m1) {
          free(m1);
        }
        reply->writeInt32(ret);
        return NO_ERROR;
      }
      break;
      case RECORDER_ENCODE_OFFLINE_PROC: {
        uint32_t client_id;
        data.readUint32(&client_id);

        uint32_t present;
        BnBuffer in_buf0 = {};
        BnBuffer in_buf1 = {};
        BnBuffer out_buf = {};
        in_buf0.ion_fd = in_buf1.ion_fd = out_buf.ion_fd = -1;
        // Input buffer
        data.readUint32(&present);
        if (!present) {
          in_buf0.ion_fd = dup(data.readFileDescriptor());
        }
        data.readUint32(&in_buf0.buffer_id);
        data.readUint32(&present);
        if (!present) {
          in_buf1.ion_fd = dup(data.readFileDescriptor());
        }
        data.readUint32(&in_buf1.buffer_id);

        // Output buffer
        data.readUint32(&present);
        if (!present) {
          out_buf.ion_fd = dup(data.readFileDescriptor());
        }
        data.readUint32(&out_buf.buffer_id);

        CameraMetadata meta;
        camera_metadata_t *m = nullptr;
        ret = meta.readFromParcel(data, &m);
        if (NO_ERROR != ret) {
          QMMF_ERROR("%s: Metadata parcel read failed: %d meta: %p\n",
              __func__, ret, m);
          reply->writeInt32(ret);
          return ret;
        }
        meta.clear();
        if (m) {
          meta.append(m);
        }

        ret = ProcOfflineProcess(client_id, in_buf0, in_buf1, out_buf, meta);

        // Clear the metadata buffer and free all storage used by it
        meta.clear();
        //We need to release this memory as meta.append() makes copy of this memory
        if (m) {
          free(m);
        }

        reply->writeInt32(ret);

        return NO_ERROR;
      }
      break;
      case RECORDER_DESTROY_OFFLINE_PROC: {
        uint32_t client_id;
        data.readUint32(&client_id);
        ret = DestroyOfflineProcess(client_id);
        reply->writeInt32(ret);

        return NO_ERROR;
      }
      break;
      default: {
        QMMF_ERROR("RecorderService:%s:Method is not supported !",__func__);
        reply->writeInt32(-1);
      }
      break;
  }
  return NO_ERROR;
}

status_t RecorderService::Connect(const sp<IRecorderServiceCallback>&
                                  service_cb, uint32_t* client_id) {

  QMMF_DEBUG("%s: Enter ", __func__);
  QMMF_KPI_DETAIL();
  status_t ret;

  std::lock_guard<std::mutex> lock(lock_);

  if (!recorder_) {
    recorder_.reset(RecorderImpl::CreateRecorder());
    if (!recorder_) {
      QMMF_ERROR("%s: Can't create Recorder Instance!!", __func__);
      return NO_MEMORY;
    }
    std::function< const sp<RemoteCallBack>& (uint32_t id)>
      remote_cb_handle = [&] (uint32_t id) -> sp<RemoteCallBack>& {
        QMMF_VERBOSE("%s: Client(%u): RemoteCallback request!",
                      __func__, id);
        assert(remote_cb_list_.count(id) != 0);
        return remote_cb_list_[id];
    };
    ret = recorder_->Init(remote_cb_handle);
    if (ret != NO_ERROR) {
      QMMF_ERROR("%s: Recorder initialization failed!", __func__);
      recorder_.reset();
      return ret;
    }
  }

  ret = GetUniqueClientID(client_id);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s: Too many active clients (255)!", __func__);
    return ret;
  }

  sp<RemoteCallBack> remote_callback;
  remote_callback = new RemoteCallBack(*client_id, service_cb);
  if (!remote_callback.get()) {
      QMMF_ERROR("%s: Unable to allocate remote callback!", __func__);
      return NO_INIT;
  }

  sp<DeathNotifier> death_notifier = new DeathNotifier();
  if (!death_notifier.get()) {
    QMMF_ERROR("%s: Unable to allocate death notifier!", __func__);
    return NO_INIT;
  }
  NotifyClientDeath notify_death = [this, capture_client_id = *client_id] {
      ClientDeathHandler(capture_client_id);
  };
  death_notifier->SetDeathNotifyCB(notify_death);

  // Link death notifier to remote handle.
  IInterface::asBinder(remote_callback->getRemoteClient())
      ->linkToDeath(death_notifier);

  remote_cb_list_.emplace(*client_id, remote_callback);
  death_notifier_list_.emplace(*client_id, death_notifier);

  recorder_->RegisterClient(*client_id);

  QMMF_INFO("%s: Service is connected with client (%d)", __func__, *client_id);

  QMMF_DEBUG("%s: Exit client_id(%d)", __func__, *client_id);
  return NO_ERROR;
}

status_t RecorderService::Disconnect(uint32_t client_id) {

  QMMF_INFO("%s: Enter client_id(%d)", __func__, client_id);
  QMMF_KPI_DETAIL();
  std::lock_guard<std::mutex> lock(lock_);

  if (!recorder_) {
    QMMF_ERROR("%s: Recorder not initialized!", __func__);
    return NO_INIT;
  }

  if (death_notifier_list_.count(client_id) == 0) {
    QMMF_ERROR("%s: Death notifier for client id %d does not exist!",
        __func__, client_id);
    return BAD_VALUE;
  }

  if (remote_cb_list_.count(client_id) == 0) {
    QMMF_ERROR("%s: Remote callback for client id %d does not exist!",
        __func__, client_id);
    return BAD_VALUE;
  }

  recorder_->DeRegisterClient(client_id, true);

  sp<DeathNotifier> notifier = death_notifier_list_[client_id];
  sp<RemoteCallBack> callback = remote_cb_list_[client_id];

  IInterface::asBinder(callback->getRemoteClient())->unlinkToDeath(notifier);

  death_notifier_list_.erase(client_id);
  remote_cb_list_.erase(client_id);

  if (death_notifier_list_.empty() && remote_cb_list_.empty()) {
    if (recorder_) {
      QMMF_INFO("%s: No client is connected! de-init the recorder!", __func__);
      recorder_->DeInit();
      recorder_.reset();
    }
  }

  QMMF_INFO("%s: Exit client_id(%d)", __func__, client_id);
  return NO_ERROR;
}

status_t RecorderService::StartCamera(const uint32_t client_id,
                                      const uint32_t camera_id,
                                      const float framerate,
                                      const CameraExtraParam& extra_param,
                                      bool enable_result_cb) {

  QMMF_INFO("%s: Enter client_id(%d)", __func__, client_id);
  QMMF_KPI_DETAIL();

  if (!IsRecorderInitialized()) {
    QMMF_ERROR("%s: Recorder not initialized!", __func__);
    return BAD_VALUE;
  }

  auto ret = recorder_->StartCamera(client_id, camera_id, framerate,
                                    extra_param,
                                    enable_result_cb);
  if(ret != NO_ERROR) {
    QMMF_ERROR("%s: Can't start Camera!!", __func__);
    return ret;
  }
  QMMF_INFO("%s: Exit client_id(%d)", __func__, client_id);
  return NO_ERROR;
}

status_t RecorderService::StopCamera(const uint32_t client_id,
                                     const uint32_t camera_id) {

  QMMF_INFO("%s: Enter client_id(%d)", __func__, client_id);
  QMMF_KPI_DETAIL();

  if (!IsRecorderInitialized()) {
    QMMF_ERROR("%s: Recorder not initialized!", __func__);
    return NO_INIT;
  }

  auto ret = recorder_->StopCamera(client_id, camera_id);
  if(ret != NO_ERROR) {
    QMMF_ERROR("%s: Can't Stop Camera!!", __func__);
    return ret;
  }
  QMMF_INFO("%s: Exit client_id(%d)", __func__, client_id);
  return NO_ERROR;
}

status_t RecorderService::CreateVideoTrack(const uint32_t client_id,
                                           const uint32_t track_id,
                                           const VideoTrackParam& param,
                                           const VideoExtraParam& xtraparam) {

  QMMF_INFO("%s: Enter client_id(%d)", __func__, client_id);
  QMMF_KPI_DETAIL();

  if (!IsRecorderInitialized()) {
    QMMF_ERROR("%s: Recorder not initialized!", __func__);
    return NO_INIT;
  }

  uint32_t id = track_id & 0xffff0000;
  if (id > 0) {
    QMMF_INFO("%s: track_id should be 16 bit number!", __func__);
    return BAD_VALUE;
  }

  auto ret = recorder_->CreateVideoTrack(client_id, track_id, param, xtraparam);

  if (ret != NO_ERROR) {
    QMMF_INFO("%s: CreateVideoTrackWithExtraParam failed!", __func__);
    return ret;
  }
  QMMF_INFO("%s: Exit client_id(%d)", __func__, client_id);
  return NO_ERROR;
}

status_t RecorderService::DeleteVideoTrack(const uint32_t client_id,
                                           const uint32_t track_id) {

  QMMF_INFO("%s: Enter client_id(%d)", __func__, client_id);
  QMMF_KPI_DETAIL();

  if (!IsRecorderInitialized()) {
    QMMF_ERROR("%s: Recorder not initialized!", __func__);
    return NO_INIT;
  }

  auto ret = recorder_->DeleteVideoTrack(client_id, track_id);
  if (ret != NO_ERROR) {
    QMMF_INFO("%s: DeleteVideoTrack failed!", __func__);
    return ret;
  }
  QMMF_INFO("%s: Exit client_id(%d)", __func__, client_id);
  return NO_ERROR;
}

status_t RecorderService::StartVideoTracks(
    const uint32_t client_id,
    const std::unordered_set<uint32_t>& track_ids) {

  QMMF_INFO("%s: Enter client_id(%d)", __func__, client_id);
  QMMF_KPI_DETAIL();

  if (!IsRecorderInitialized()) {
    QMMF_ERROR("%s: Recorder not initialized!", __func__);
    return NO_INIT;
  }

  auto ret = recorder_->StartVideoTracks(client_id, track_ids);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s: StartVideoTracks failed!", __func__);
    return ret;
  }
  QMMF_INFO("%s: Exit client_id(%d)", __func__, client_id);
  return NO_ERROR;
}

status_t RecorderService::StopVideoTracks(
    const uint32_t client_id,
    const std::unordered_set<uint32_t>& track_ids) {

  QMMF_INFO("%s: Enter client_id(%d)", __func__, client_id);
  QMMF_KPI_DETAIL();

  if (!IsRecorderInitialized()) {
    QMMF_ERROR("%s: Recorder not initialized!", __func__);
    return NO_INIT;
  }

  auto ret = recorder_->StopVideoTracks(client_id, track_ids);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s: StopVideoTracks failed!", __func__);
    return ret;
  }
  QMMF_INFO("%s: Exit client_id(%d)", __func__, client_id);
  return NO_ERROR;
}

status_t RecorderService::ReturnTrackBuffer(const uint32_t client_id,
                                            const uint32_t track_id,
                                            std::vector<BnBuffer> &buffers) {

  QMMF_VERBOSE("%s: Enter client_id(%d)", __func__, client_id);

  if (!IsRecorderInitialized()) {
    QMMF_ERROR("%s: Recorder not initialized!", __func__);
    return NO_INIT;
  }

  auto ret = recorder_->ReturnTrackBuffer(client_id, track_id, buffers);
  if (ret != NO_ERROR) {
    QMMF_INFO("%s: ReturnTrackBuffer failed!", __func__);
    return BAD_VALUE;
  }
  QMMF_VERBOSE("%s: Exit client_id(%d)", __func__, client_id);
  return ret;
}

status_t RecorderService::SetVideoTrackParam(const uint32_t client_id,
                                             const uint32_t track_id,
                                             VideoParam type,
                                             void *param,
                                             size_t size) {

  QMMF_INFO("%s: Enter client_id(%d)", __func__, client_id);

  if (!IsRecorderInitialized()) {
    QMMF_ERROR("%s: Recorder not initialized!", __func__);
    return NO_INIT;
  }

  auto ret = recorder_->SetVideoTrackParam(client_id, track_id, type,
                                           param, size);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s: SetVideoTrackParam failed!", __func__);
    return ret;
  }
  QMMF_INFO("%s: Exit client_id(%d)", __func__, client_id);
  return NO_ERROR;
}

status_t RecorderService::CaptureImage(
    const uint32_t client_id, const uint32_t camera_id,
    const ImageGroupType &pad_group,
    const SnapshotType type, const uint32_t n_burst,
    const std::vector<CameraMetadata> &meta) {
  QMMF_INFO("%s: Enter client_id(%d)", __func__, client_id);

  if (!IsRecorderInitialized()) {
    QMMF_ERROR("%s: Recorder not initialized!", __func__);
    return NO_INIT;
  }

  auto ret = recorder_->CaptureImage(client_id, camera_id, pad_group,
                                            type, n_burst, meta);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s: CaptureImage failed!", __func__);
    return ret;
  }
  QMMF_INFO("%s: Exit client_id(%d)", __func__, client_id);
  return NO_ERROR;
}

status_t RecorderService::CaptureImage(const uint32_t client_id,
                                       const uint32_t camera_id,
                                       const SnapshotType type,
                                       const uint32_t n_images,
                                       const std::vector<CameraMetadata> &meta) {

  QMMF_INFO("%s: Enter client_id(%d)", __func__, client_id);

  if (!IsRecorderInitialized()) {
    QMMF_ERROR("%s: Recorder not initialized!", __func__);
    return NO_INIT;
  }

  auto ret = recorder_->CaptureImage(client_id, camera_id, type, n_images, meta);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s: CaptureImage failed!", __func__);
    return ret;
  }
  QMMF_INFO("%s: Exit client_id(%d)", __func__, client_id);
  return NO_ERROR;
}

status_t RecorderService::ConfigImageCapture(const uint32_t client_id,
                                             const uint32_t camera_id,
                                             const uint32_t image_id,
                                             const ImageParam &param,
                                             const ImageExtraParam &xtrapram) {

  QMMF_INFO("%s: Enter client_id(%d)", __func__, client_id);

  if (!IsRecorderInitialized()) {
    QMMF_ERROR("%s: Recorder not initialized!", __func__);
    return NO_INIT;
  }

  auto ret = recorder_->ConfigImageCapture(client_id, camera_id, image_id, 
                                           param, xtrapram);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s: ConfigImageCapture failed!", __func__);
    return ret;
  }
  QMMF_INFO("%s: Exit client_id(%d)", __func__, client_id);
  return NO_ERROR;
}

status_t RecorderService::CancelCaptureImage(const uint32_t client_id,
                                             const uint32_t camera_id,
                                             const uint32_t image_id,
                                             const bool cache) {

  QMMF_INFO("%s: Enter client_id(%d)", __func__, client_id);

  if (!IsRecorderInitialized()) {
    QMMF_ERROR("%s: Recorder not initialized!", __func__);
    return NO_INIT;
  }

  auto ret = recorder_->CancelCaptureImage(client_id, camera_id, image_id,
                                           cache);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s: CancelCaptureImage failed!", __func__);
    return ret;
  }
  QMMF_INFO("%s: Exit client_id(%d)", __func__, client_id);
  return NO_ERROR;
}


status_t RecorderService::ReturnImageCaptureBuffer(const uint32_t client_id,
                                                   const uint32_t camera_id,
                                                   const int32_t buffer_id) {

  QMMF_INFO("%s: Enter client_id(%d)", __func__, client_id);

  if (!IsRecorderInitialized()) {
    QMMF_ERROR("%s: Recorder not initialized!", __func__);
    return NO_INIT;
  }

  auto ret = recorder_->ReturnImageCaptureBuffer(client_id, camera_id,
                                                 buffer_id);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s: ReturnImageCaptureBuffer failed!", __func__);
    return ret;
  }
  QMMF_INFO("%s: Exit client_id(%d)", __func__, client_id);
  return NO_ERROR;
}

status_t RecorderService::SetCameraParam(const uint32_t client_id,
                                         const uint32_t camera_id,
                                         const CameraMetadata &meta) {

  QMMF_INFO("%s: Enter client_id(%d)", __func__, client_id);

  if (!IsRecorderInitialized()) {
    QMMF_ERROR("%s: Recorder not initialized!", __func__);
    return NO_INIT;
  }

  auto ret = recorder_->SetCameraParam(client_id, camera_id, meta);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s: SetCameraParam failed!", __func__);
    return ret;
  }
  QMMF_INFO("%s: Exit client_id(%d)", __func__, client_id);
  return NO_ERROR;
}

status_t RecorderService::GetCameraParam(const uint32_t client_id,
                                         const uint32_t camera_id,
                                         CameraMetadata &meta) {

  QMMF_INFO("%s: Enter client_id(%d)", __func__, client_id);

  if (!IsRecorderInitialized()) {
    QMMF_ERROR("%s: Recorder not initialized!", __func__);
    return NO_INIT;
  }

  auto ret = recorder_->GetCameraParam(client_id, camera_id, meta);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s: GetCameraParam failed!", __func__);
    return ret;
  }
  QMMF_INFO("%s: Exit client_id(%d)", __func__, client_id);
  return NO_ERROR;
}

status_t RecorderService::SetCameraSessionParam(const uint32_t client_id,
                                                const uint32_t camera_id,
                                                const CameraMetadata &meta) {

  QMMF_INFO("%s: Enter client_id(%d)", __func__, client_id);

  if (!IsRecorderInitialized()) {
    QMMF_ERROR("%s: Recorder not initialized!", __func__);
    return NO_INIT;
  }

  auto ret = recorder_->SetCameraSessionParam(client_id, camera_id, meta);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s: SetCameraSessionParam failed!", __func__);
    return ret;
  }
  QMMF_INFO("%s: Exit client_id(%d)", __func__, client_id);
  return NO_ERROR;
}

status_t RecorderService::SetSHDR(const uint32_t client_id,
                                     const uint32_t camera_id,
                                     const bool enable) {

  QMMF_INFO("%s: Enter client_id(%d)", __func__, client_id);

  if (!IsRecorderInitialized()) {
    QMMF_ERROR("%s: Recorder not initialized!", __func__);
    return NO_INIT;
  }

  auto ret = recorder_->SetSHDR(client_id, camera_id, enable);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s: GetCameraParam failed!", __func__);
    return ret;
  }
  QMMF_INFO("%s: Exit client_id(%d)", __func__, client_id);
  return NO_ERROR;
}

status_t RecorderService::GetDefaultCaptureParam(const uint32_t client_id,
                                                 const uint32_t camera_id,
                                                 CameraMetadata &meta) {

  QMMF_INFO("%s: Enter client_id(%d)", __func__, client_id);

  if (!IsRecorderInitialized()) {
    QMMF_ERROR("%s: Recorder not initialized!", __func__);
    return NO_INIT;
  }

  auto ret = recorder_->GetDefaultCaptureParam(client_id, camera_id, meta);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s: GetDefaultCaptureParam failed!", __func__);
    return ret;
  }
  QMMF_INFO("%s: Exit client_id(%d)", __func__, client_id);
  return NO_ERROR;
}

status_t RecorderService::GetCamStaticInfo(const uint32_t client_id,
                                           std::vector<CameraMetadata> &meta) {

  QMMF_DEBUG("%s: Enter client_id(%d)", __func__, client_id);

  if (!IsRecorderInitialized()) {
    QMMF_ERROR("%s: Recorder not initialized!", __func__);
    return NO_INIT;
  }

  auto ret = recorder_->GetCamStaticInfo(client_id, meta);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s: GetCamStaticInfo failed!", __func__);
    return ret;
  }
  QMMF_DEBUG("%s: Exit client_id(%d)", __func__, client_id);
  return NO_ERROR;
}

status_t RecorderService::GetCameraCharacteristics(const uint32_t client_id,
                                                   const uint32_t camera_id,
                                                   CameraMetadata &meta) {

  QMMF_INFO("%s: Enter client_id(%d)", __func__, client_id);

  if (!IsRecorderInitialized()) {
    QMMF_ERROR("%s: Recorder not initialized!", __func__);
    return NO_INIT;
  }

  auto ret = recorder_->GetCameraCharacteristics(client_id, camera_id, meta);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s: GetCameraCharacteristics failed!", __func__);
    return ret;
  }
  QMMF_INFO("%s: Exit client_id(%d)", __func__, client_id);
  return NO_ERROR;
}

status_t RecorderService::GetOfflineParams(const uint32_t client_id,
                                           const OfflineCameraInputParams &in_params,
                                           OfflineCameraOutputParams &out_params) {
  QMMF_INFO("%s:Enter client_id(%d)", __func__, client_id);
  if (!IsRecorderInitialized()) {
    QMMF_ERROR("%s: Recorder not initialized!", __func__);
    return NO_INIT;
  }
  auto ret = recorder_->GetOfflineParams(client_id, in_params, out_params);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s: Can't get OfflineCameraParams!", __func__);
  }
  QMMF_INFO("%s: Exit client_id(%d)", __func__, client_id);
  return ret;
}

status_t RecorderService::CreateOfflineProcess(
                                  const uint32_t client_id,
                                  const OfflineCameraCreateParams &params) {

  QMMF_INFO("%s:Enter client_id(%d)", __func__, client_id);

  if (!IsRecorderInitialized()) {
    QMMF_ERROR("%s: Recorder not initialized!", __func__);
    return NO_INIT;
  }
  auto ret = recorder_->CreateOfflineProcess(client_id, params);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s: Can't create Offline Proc PostProcessor!", __func__);
    return ret;
  }

  QMMF_INFO("%s: Exit client_id(%d)", __func__, client_id);
  return ret;
}

status_t RecorderService::ProcOfflineProcess(const uint32_t client_id,
                                             const BnBuffer& in_buf0,
                                             const BnBuffer& in_buf1,
                                             const BnBuffer& out_buf,
                                             const CameraMetadata& meta) {

  QMMF_INFO("%s: Enter client_id(%d)", __func__, client_id);

  if (!IsRecorderInitialized()) {
    QMMF_ERROR("%s: Recorder not initialized!", __func__);
    return NO_INIT;
  }

  auto ret = recorder_->ProcOfflineProcess(client_id, in_buf0, in_buf1, out_buf, meta);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s: Submitting request failed", __func__);
    return ret;
  }

  QMMF_INFO("%s: Exit client_id(%d)", __func__, client_id);

  return ret;
}

status_t RecorderService::DestroyOfflineProcess(const uint32_t client_id) {

  QMMF_INFO("%s: Enter client_id(%d)", __func__, client_id);

  if (!IsRecorderInitialized()) {
    QMMF_ERROR("%s: Recorder not initialized!", __func__);
    return NO_INIT;
  }
  auto ret = recorder_->DestroyOfflineProcess(client_id);
  if (ret != NO_ERROR) {
    QMMF_ERROR("%s: Destroy failed", __func__);
    return ret;
  }

  QMMF_INFO("%s: Exit client_id(%d)", __func__, client_id);

  return ret;
}

void RecorderService::ClientDeathHandler(const uint32_t client_id) {
  QMMF_INFO("%s: client_id(%d) died in battle!", __func__, client_id);
  // Internal disconnect, it would trigger resource cleanup belongs to died
  // client.
  DisconnectInternal(client_id);
}

bool RecorderService::IsRecorderInitialized() {

  std::lock_guard<std::mutex> lock(lock_);
  return (recorder_) ? true : false;
}

status_t RecorderService::DisconnectInternal(const uint32_t client_id) {

  QMMF_INFO("%s: Enter client_id(%d)", __func__, client_id);
  std::lock_guard<std::mutex> lock(lock_);

  if (!recorder_) {
    QMMF_ERROR("%s: Recorder not initialized!", __func__);
    return NO_INIT;
  }

  if (death_notifier_list_.count(client_id) == 0) {
    QMMF_ERROR("%s: Death notifier for client id %d does not exist!",
        __func__, client_id);
    return BAD_VALUE;
  }

  if (remote_cb_list_.count(client_id) == 0) {
    QMMF_ERROR("%s: Remote callback for client id %d does not exist!",
        __func__, client_id);
    return BAD_VALUE;
  }

  // Forceful cleanup.
  recorder_->DeRegisterClient(client_id, true);

  sp<DeathNotifier> notifier = death_notifier_list_[client_id];
  sp<RemoteCallBack> callback = remote_cb_list_[client_id];

  IInterface::asBinder(callback->getRemoteClient())->unlinkToDeath(notifier);

  death_notifier_list_.erase(client_id);
  remote_cb_list_.erase(client_id);

  if (death_notifier_list_.empty() && remote_cb_list_.empty()) {
    if (recorder_) {
      QMMF_INFO("%s: No client is connected! de-init the recorder!", __func__);
      recorder_->DeInit();
      recorder_.reset();
    }
  }

  QMMF_INFO("%s: Exit client_id(%d)", __func__, client_id);
  return NO_ERROR;
}

status_t RecorderService::GetVendorTagDescriptor(std::shared_ptr<VendorTagDescriptor> &desc) {

  desc = VendorTagDescriptor::getGlobalVendorTagDescriptor();
  return (desc == nullptr) ? BAD_VALUE : NO_ERROR;
}

status_t RecorderService::GetUniqueClientID(uint32_t *client_id) {

  for (uint32_t id = 1; id <= 0xFF; id++) {
    if (remote_cb_list_.count(id) == 0) {
      *client_id = id;
      return NO_ERROR;
    }
  }
  return BAD_VALUE;
}

}; //namespace recorder

}; //namespace qmmf
