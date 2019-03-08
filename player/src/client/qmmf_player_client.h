/*
* Copyright (c) 2016-2019, The Linux Foundation. All rights reserved.
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

/*! @file qmmf_player_client.h
*/

#pragma once


#include <utils/Errors.h>
#include <utils/Mutex.h>
#include <utils/RefBase.h>
#include <utils/KeyedVector.h>
#include <map>
#include <linux/msm_ion.h>

#include "qmmf-sdk/qmmf_player_params.h"
#include "player/src/client/qmmf_player_service_intf.h"
#include "player/src/service/qmmf_player_common.h"
#include "common/utils/qmmf_log.h"

/// @namespace qmmf::player
namespace qmmf {
namespace player {

using namespace android;

/**
 * @brief Delegation to binder proxy <IPlayerService>
 * and implementation of binder CB.
 */
class PlayerClient {
 public:
  PlayerClient();

  ~PlayerClient();


  /**
   * @brief Connect to player service.
   * 
   * @param[in] cb : Player callback to register with player service 
   * @return status_t : Return the status of function call
   */
  status_t Connect(PlayerCb& cb);

  /**
   * @brief  Disconnect to player service.
   * 
   * @return status_t : Return the status of function call
   */
  status_t Disconnect();

  /**
   * @brief Create Audio Track and configure the audio params (audio codec,
   * audio type, etc) for decoding during playback
   * 
   * @param[in] track_id : Audio Track id
   * @param[in] param : Audio Track params filled by the application
   * @param[in] cb : Audio callback to register with player service
   * @return status_t : Return the status of function call
   */
  status_t CreateAudioTrack(uint32_t track_id,
                            AudioTrackCreateParam& param,
                            TrackCb& cb);

  /**
   * @brief Create Video Track and configure the Video params (Video codec,
   * bitrate, etc) for decoding during playback
   * 
   * @param[in] track_id : Video Track id
   * @param[in] param : Video Track params filled by the application
   * @param[in] cb : Video callback to register with player service
   * @return status_t : Return the status of function call
   */
  status_t CreateVideoTrack(uint32_t track_id,
                            VideoTrackCreateParam& param,
                            TrackCb& cb);

  /**
   * @brief Delete Audio Track
   * 
   * @param[in] track_id : Audio Track id
   * @return status_t : Return the status of function call
   */
  status_t DeleteAudioTrack(uint32_t track_id);

  /**
   * @brief Delete Video Track
   * 
   * @param[in] track_id : Video Track id
   * @return status_t : Return the status of function call
   */
  status_t DeleteVideoTrack(uint32_t track_id);

  /**
   * @brief Prepare prepares the playback pipeline by allocating the audio/video
   * buffers, configuring the respective decoders.
   * 
   * @return status_t : Return the status of function call
   */
  status_t Prepare();

  /**
   * @brief Dequeue Input Buffer gives a vector of empty buffers to the application
   *  to fill the data based on track id (for video and audio)
   * 
   * @param[in] track_id : Video/Audio Track id
   * @param[out] buffers : List of empty buffers returned by Player service 
   * @return status_t : Return the status of function call
   */
  status_t DequeueInputBuffer(uint32_t track_id,
                              std::vector<TrackBuffer>& buffers);

  /**
   * @brief Queue Input Buffer feeds filled buffers to the player service to
   * process play it based on track id (for video and audio)
   * 
   * @param[in] track_id : Video/Audio Track id
   * @param[in] buffers : List of filled buffers given by Player service
   * @param[in] meta_param : Input based on below mentioned meta_type
   * @param[in] meta_size : Size of meta_param
   * @param[in] meta_type : Input of type TrackMetaBufferType
   * @return status_t : Return the status of function call
   */
  status_t QueueInputBuffer(uint32_t track_id,
                            std::vector<TrackBuffer>& buffers,
                            void *meta_param,
                            size_t meta_size,
                            TrackMetaBufferType meta_type);

  /**
   * @brief Starts the playback i.e., initiates the threads to continuoulsy take
   * data from application, feed encoded input to the decoder, fetch the decoded
   * output and play it on display/speaker.
   * 
   * @return status_t : Return the status of function call
   */
  status_t Start();

  /**
   * @brief  Stops the playback i.e., stops all the threads which continuoulsy take
   * data from application, stops feeding encoded input to the decoder.
   * 
   * @param[in] handler : handler of the type PictureCallback
   * @param[in] params : params of the type PictureParam
   * @return status_t : Return the status of function call
   */
  status_t Stop(const PictureCallback& handler, const PictureParam& params);

  /**
   * @brief Pauses the playback i.e., pauses all the threads which continuoulsy
   * take data from application and the decoder.
   * 
   * @param[in] handler : handler of the type PictureCallback
   * @param[in] params : params of the type PictureParam
   * @return status_t : Return the status of function call
   */
  status_t Pause(const PictureCallback& handler, const PictureParam& params);

  /**
   * @brief Resumes the playback i.e., resumes all the threads which continuously
   * take data from application and the decoder.
   * 
   * @return status_t : Return the status of function call
   */
  status_t Resume();

  /**
   * @brief Drag during the playback i.e., displays only the IDR frames till
   * the point the user seeks.
   * 
   * @return status_t : Return the status of function call
   */
  status_t Drag();

  /**
   * @brief Set Position during the playback i.e., directly jumps to the position
   * the user seeks.
   * 
   * @param[in] seek_time : Time(in microseconds) to seek to
   * @return status_t : Return the status of function call
   */
  status_t SetPosition(int64_t seek_time);

  /**
   * @brief Set Trick mode allows the user to fast forward, slow forward, rewind
   * with different speeds.
   * 
   * @param[in] speed : speed of type TrickModeSpeed
   * @param[in] dir : dir of type TrickModeDirection
   * @return status_t : Return the status of function call
   */
  status_t SetTrickMode(TrickModeSpeed speed, TrickModeDirection dir);

 /**
  * @brief Set the Audio Track Param
  * 
  * @param[in] track_id : Audio track id
  * @param[in] type : Input of type CodecParamType
  * @param[in] param : Valid input based on above mentioned CodecParamType
  * @param[in] param_size : size of above mentioned param
  * @return status_t : Return the status of function call
  */
  status_t SetAudioTrackParam(uint32_t track_id,
                              CodecParamType type,
                              void *param,
                              size_t param_size);

  /**
   * @brief Set the Video Track Param
   * 
   * @param[in] track_id : Video track id
   * @param[in] type : Input of type CodecParamType
   * @param[in] param : Valid input based on above mentioned CodecParamType
   * @param[in] param_size : size of above mentioned param
   * @return status_t : Return the status of function call
   */
  status_t SetVideoTrackParam(uint32_t track_id,
                              CodecParamType type,
                              void *param,
                              size_t param_size);

  /// callbacks from service.
  void NotifyPlayerEvent(EventType event_type, void *event_data,
                         size_t event_data_size);

  void NotifyVideoTrackEvent(uint32_t track_id, EventType event_type,
                             void *event_data,
                             size_t event_data_size);

  void NotifyAudioTrackEvent(uint32_t track_id, EventType event_type,
                             void *event_data,
                             size_t event_data_size);

  void NotifyGrabPictureData(uint32_t track_id,
                             BufferDescriptor& buffer);

 private:

  bool CheckServiceStatus();

  /**
   * @brief Notifies the client when the sevice dies through binder
   */
  class DeathNotifier : public IBinder::DeathRecipient
  {
   public:
    DeathNotifier(PlayerClient* parent) : parent_(parent) {}

    void binderDied(const wp<IBinder>&) override {
          ALOGD("PlayerClient:%s: Player service died", __func__);
          Mutex::Autolock l(parent_->lock_);
          parent_->player_service_.clear();
          parent_->player_service_ = nullptr;
          PlayerError error = PlayerError::kServiceDied;
          parent_->player_cb_.event_cb(EventType::kError, &(error),
                                       sizeof(error));
    }
    PlayerClient* parent_;
  };
  friend class DeathNotifier;

  Mutex                                     lock_;
  sp<IPlayerService>                        player_service_;
  sp<DeathNotifier>                         death_notifier_;
  int32_t                                   ion_device_;
  PlayerCb                                  player_cb_;
  PictureCallback                           picture_cb_;
  DefaultKeyedVector<uint32_t, TrackCb >    track_cb_list_;

  typedef struct BufInfo {
    /**< fd at service */
    uint32_t buf_id;

    uint32_t client_fd;

    /**< memory mapped buffer */
    void*    vaddr;

    size_t   frame_len;

    /**< ION handle */
    ion_user_handle_t ion_handle;
  } BufInfo;

  /**< map<fd , buf_info> */
  typedef DefaultKeyedVector<uint32_t, BufInfo> buf_info_map;

  /**< map <track id , map<fd , buf info>> */
  DefaultKeyedVector<uint32_t,  buf_info_map> track_buf_map_;
};


class ServiceCallbackHandler : public BnPlayerServiceCallback {
 public:

  ServiceCallbackHandler(PlayerClient * client);

  ~ServiceCallbackHandler();

 private:
  /// Methods of BnPlayerServiceCallback.

  void NotifyPlayerEvent(EventType event_type, void *event_data,
                         size_t event_data_size) override;

  void NotifyVideoTrackEvent(uint32_t track_id, EventType event_type,
                             void *event_data,
                             size_t event_data_size) override;

  void NotifyAudioTrackEvent(uint32_t track_id, EventType event_type,
                             void *event_data,
                             size_t event_data_size) override;

  void NotifyGrabPictureData(uint32_t track_id,
                             BufferDescriptor& buffer) override;

  PlayerClient *client_;
};


};  // namespace player
};  // namespace qmmf
