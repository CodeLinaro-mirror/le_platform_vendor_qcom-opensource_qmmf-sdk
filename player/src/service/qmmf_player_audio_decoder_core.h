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

/*! @file qmmf_player_audio_decoder_core.h
*/

#pragma once

#include <memory>

#include <utils/KeyedVector.h>

#include "player/src/service/qmmf_player_audio_sink.h"
#include "include/qmmf-sdk/qmmf_player_params.h"
#include "include/qmmf-sdk/qmmf_codec.h"
#include "player/src/service/qmmf_player_common.h"
#include "include/qmmf-sdk/qmmf_avcodec_params.h"
#include "include/qmmf-sdk/qmmf_avcodec.h"
#include "include/qmmf-sdk/qmmf_buffer.h"


namespace qmmf {
namespace player {

using namespace android;

class AudioTrackSink;
class AudioTrackDecoder;

class AudioDecoderCore {
 public:

  /// Default destructor
  ~AudioDecoderCore();

  /** @brief Factory method for the singleton.
   *
   *  @returns AudioDecoderCore : Pointer to the singleton.
   */
  static AudioDecoderCore* CreateAudioDecoderCore();

  /**
   * @brief Create Audio Track
   *
   * @param[in] params : Audio Track params recevied from player impl
   * @param[in] track_callback : Callback function for the audio track
   * @param[in] player_callback : Callback function for the player
   * @return status_t : Return success or failure
   */
  status_t CreateAudioTrack(AudioTrackParams& params,
                            TrackCb& track_callback, PlayerCb player_callback);

  /**
   * @brief Dequeue buffer for a particular audio track
   *
   * @param[in] track_id : Audio Track id
   * @param[out] buffers : Vector of empty AVCodecBuffers
   * @return status_t : Return success or failure
   */
  status_t DequeueTrackInputBuffer(uint32_t track_id,
                         std::vector<AVCodecBuffer>& buffers);

  /**
   * @brief Queue buffer for a particular audio track
   *
   * @param[in] track_id : Audio Track id
   * @param[in] buffers : Vector of filled AVCodecBuffers
   * @return status_t : Return success or failure
   */
  status_t QueueTrackInputBuffer(uint32_t track_id,
                         std::vector<AVCodecBuffer>& buffers);

  /**
   * @brief Prepare pipeline for a particular audio track
   *
   * @param[in] track_id : Audio track id
   * @param[in] audio_track_sink : Shared pointer of the audio sink
   * @return status_t : Return success or failure
   */
  status_t PrepareTrackPipeline(uint32_t track_id,
      const ::std::shared_ptr<AudioTrackSink>& audio_track_sink);

  /**
   * @brief Start the audio track
   *
   * @param[in] track_id : Audio track id
   * @return status_t : Return success or failure
   */
  status_t StartTrackDecoder(uint32_t track_id);

   /**
   * @brief Stop the audio track
   *
   * @param[in] track_id : Audio track id
   * @return status_t : Return success or failure
   */
  status_t StopTrackDecoder(uint32_t track_id);

   /**
   * @brief Pause the audio track
   *
   * @param[in] track_id : Audio track id
   * @return status_t : Return success or failure
   */
  status_t PauseTrackDecoder(uint32_t track_id);

   /**
   * @brief Resume the audio track
   *
   * @param[in] track_id : Audio track id
   * @return status_t : Return success or failure
   */
  status_t ResumeTrackDecoder(uint32_t track_id);

  /**
   * @brief Prepare the audio drag for drag
   *
   * @param[in] track_id : Audio track id
   * @param[in] ignore_fps : If true, ignore notifying regarding empty input buffers
   * @return status_t : Return success or failure
   */
  status_t PrepareDrag(uint32_t track_id, bool ignore_fps);

  /**
   * @brief Notify the client on empty input buffers for a particular audio track
   *
   * @param[in] track_id : Audio track id
   * @return status_t : Return success or failure
   */
  status_t NotifyInputBuffer(uint32_t track_id);

  /**
   * @brief Set the Audio Track Decoder Params for a particular audio track
   *
   * @param[in] track_id : Audio track id
   * @param[in] param_type : The type of params to be set from CodecParamType
   * @param[in] param : The params to be set based on  param_type
   * @param[in] param_size : The size of param
   * @return status_t : Return success or failure
   */
  status_t SetAudioTrackDecoderParams(uint32_t track_id,
                               CodecParamType param_type, void* param,
                               uint32_t param_size);

  /**
   * @brief Delete a particular audio track
   *
   * @param[in] track_id : Audio track id
   * @return status_t : Return success or failure
   */
  status_t DeleteTrackDecoder(uint32_t track_id);

  /**
   * @brief Set the Position for audio track
   *
   * @param[in] track_id : Audio track id
   * @param[in] seek_time : Time to seek to
   * @return status_t : Return success or failure
   */
  status_t SetPosition(uint32_t track_id, int64_t seek_time);

 private:

  /**
   * @brief Checks the existence of a particular audio track
   *
   * @param[in] track_id : Audio Track
   * @return true : Track id exists
   * @return false : Track id doesn not exist
   */
  bool isTrackValid(uint32_t track_id);

  /// Default constructor, copy constructor, assignement are invalid
  AudioDecoderCore();
  AudioDecoderCore(const AudioDecoderCore&);
  AudioDecoderCore& operator=(const AudioDecoderCore&);

  /**< Map of track id and audio decoder */
  DefaultKeyedVector<uint32_t, ::std::shared_ptr<AudioTrackDecoder>> audio_track_decoders_;

  /**< Singleton instance of the Audio decoder core */
  static AudioDecoderCore* instance_;
  int32_t ion_device_; /**< Variable to manage the ion device */
};

/**
 * @brief This class behaves as both producer and consumer. At one end, it takes
 * encoded data from the demuxer; and on the other end, it provides those
 * encoded data to Decoder. It also manages buffer circulation, skip, etc.
 */

class AudioTrackDecoder : public ::qmmf::avcodec::ICodecSource {
 public:

  /// Default constructor
  AudioTrackDecoder(int32_t ion_device);

  /// Default destructor
  ~AudioTrackDecoder();

  /**
   * @brief Handler which calls the callbacks for events from AVCodec
   *
   * @param[out] event_type : Type of event from AVCodec
   * @param[out] event_data : Data of the event based on event from EventType
   * @param[out] event_data_size : Size of event_data
   */
  void AVCodecHandler(qmmf::avcodec::EventType event_type, void *event_data,
                      size_t event_data_size);

  /**
   * @brief Configure AVCodec for Audio Decoding
   *
   * @param[in] track_params : Audio params like format, etc needed to configure the AVCodec instance
   * @param[in] track_callback : Callback for the audio track
   * @param[in] player_callback : Callback for the player
   * @return status_t : Return success or failure
   */
  status_t ConfigureTrackDecoder(AudioTrackParams& track_params,
                                 TrackCb& track_callback,
                                 PlayerCb& player_callback);
  /**
   * @brief Dequeue input buffers
   *
   * @param[out] buffers : Send a list of empty AVCodecBuffers
   * @return status_t : Return success or failure
   */
  status_t DequeueInputBuffer(std::vector<AVCodecBuffer>& buffers);

  /**
   * @brief Queue input buffers
   *
   * @param[in] buffers : Get a list of filled AVCodecBuffers
   * @return status_t : Return success or failure
   */
  status_t QueueInputBuffer(std::vector<AVCodecBuffer>& buffers);

  /**
   * @brief Prepare the AVCodec for audio decoding
   *
   * @param[in] audio_track_sink : The output port instance for the AVCodec
   * @param[in] audio_track_decoder : The input port instance for the AVCodec
   * @return status_t : Return success or failure
   */
  status_t PreparePipeline(const ::std::shared_ptr<AudioTrackSink>& audio_track_sink,
                           const ::std::shared_ptr<AudioTrackDecoder>& audio_track_decoder);

  /**
   * @brief Start the AVCodec decode
   *
   * @return status_t : Return success or failure
   */

  status_t StartDecoder();

  /**
   * @brief Stop the AVCodec decode
   *
   * @return status_t : Return success or failure
   */
  status_t StopDecoder();

  /**
   * @brief Pause the AVCodec decode
   *
   * @return status_t : Return success or failure
   */
  status_t PauseDecoder();

  /**
   * @brief Resume the AVCodec decode
   *
   * @return status_t : Return success or failure
   */
  status_t ResumeDecoder();

  /**
   * @brief Delete the AVCodec instance for audio decoding
   *
   * @return status_t : Return success or failure
   */
  status_t DeleteDecoder();

  /**
   * @brief PrepareDrag
   *
   * @param[in] ignore_fps : If true, ignore notifying regarding empty input buffers
   * @return status_t : Return success or failure
   */
  status_t PrepareDrag(bool ignore_fps);

  /**
   * @brief Notify the availability of empty input buffers
   *
   * @return status_t : Return success or failure
   */
  status_t NotifyInputBuffer();

  /**
   * @brief Set the Audio Track Decoder Params
   *
   * @param[in] param_type : The type of params to be set from CodecParamType
   * @param[in] param : The params to be set based on  param_type
   * @param[in] param_size : The size of param
   * @return status_t : Return success or failure
   */
  status_t SetAudioDecoderParams(CodecParamType param_type, void* param,
                                  uint32_t param_size);


  /**
   * @brief Set Position
   *
   * @param[in] seek_time : Time to seek to
   * @return status_t : Return success or failure
   */
  status_t SetPosition(int64_t seek_time);

  //! @copydoc ICodecSource::GetBuffer()
  status_t GetBuffer(BufferDescriptor& stream_buffer,
                     void* client_data) override;

  //! @copydoc ICodecSource::ReturnBuffer()
  status_t ReturnBuffer(BufferDescriptor& stream_buffer,
                        void* client_data) override;

  //! @copydoc ICodecSource::NotifyPortEvent()
  status_t NotifyPortEvent(::qmmf::avcodec::PortEventType event_type,
                           void* event_data) override;

 private:

  /**
   * @brief Allocate the input port buffers for AVCodec
   *
   * @return status_t : Return success or failure
   */
  status_t AllocInputPortBufs();

  /**
   * @brief Allocate the output port buffers for AVCodec
   *
   * @return status_t : Return success or failure
   */
  status_t AllocOutputPortBufs();

  /**
   * @brief Release the output port buffers for AVCodec
   *
   * @return status_t : Return success or failure
   */
  status_t ReleaseOutputBuffers();

  /**
   * @brief Release the input port buffers for AVCodec
   *
   * @return status_t : Returns success or failure
   */
  status_t ReleaseInputBuffers();

  /**
   * @brief Get the audio track id
   *
   * @return uint32_t : returns the audio track id
   */
  uint32_t TrackId() { return audio_track_params_.track_id; }

  /**
   * @brief Check if the player is paused
   *
   * @return true : player is paused
   * @return false : player is not paused
   */
  inline bool IsPause() {
    std::lock_guard<std::mutex> lock(pause_lock_);
    return pause_;
  }

  typedef struct BufInfo {
    /**< FD at service */
    uint32_t buf_id;

    /**< Memory mapped buffer */
    void*    vaddr;
  } BufInfo;

  /**< map<fd , buf_info> */
  DefaultKeyedVector<uint32_t, BufInfo> buf_info_map;

  ::std::shared_ptr<AudioTrackSink> audio_track_sink_; /*<< Output port obejct for AVCodec */
  AudioTrackParams                  audio_track_params_;
  ::qmmf::avcodec::AVCodec*         avcodec_; /*<< instance of the AVCodec */

  /**< For bitstream */
  Vector<StreamBuffer>      input_buffer_list_; /*<< List which contains all allocated input buffers */
  TSQueue<StreamBuffer>     unfilled_frame_queue_; /*<< Empty buffers available to client to fill */
  TSQueue<StreamBuffer>     filled_frame_queue_; /*<<Buffers filled by the client */
  TSQueue<StreamBuffer>     frames_to_decode_; /*<< Filled buffers yet to be given to AVCodec*/
  TSQueue<StreamBuffer>     frames_being_decoded_; /*<< Filled buffers given to AVCodec*/

  std::map<int32_t, struct ion_handle_data> ion_handle_data_;

  Vector<::qmmf::avcodec::CodecBuffer> output_buffer_list_; /*<< List which contains all allocated output buffers */

  std::mutex                wait_for_empty_frame_lock_;
  QCondition                wait_for_empty_frame_;

  std::mutex                wait_for_frame_lock_;
  QCondition                wait_for_frame_;
  int32_t                   ion_device_;
  std::mutex                queue_lock_;
  bool                      stop_received_;

#ifdef DUMP_PCM_DATA
  int32_t                   file_fd_audio_;
#endif
  TrackCb                   track_callback_;
  PlayerCb                  player_callback_;
  InputBufferNotifyParams   input_buffer_notify_params_;
  bool                      pause_;
  std::mutex                pause_lock_;

};

};  // namepsse player
};  // namespace qmmf
