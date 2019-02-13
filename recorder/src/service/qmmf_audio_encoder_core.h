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

//! @file qmmf_audio_encoder_core.h

#pragma once

#include <map>
#include <memory>
#include <mutex>
#include <queue>

#include "common/utils/qmmf_condition.h"
#include "common/codecadaptor/src/qmmf_avcodec.h"
#include "recorder/src/service/qmmf_audio_track_source.h"
#include "recorder/src/service/qmmf_recorder_common.h"
#include "recorder/src/service/qmmf_recorder_ion.h"

namespace qmmf {
namespace recorder {

/*! @brief Manages the audio data flow between RecorderImpl and AVCodec.
 *
 *  Sets up the data path between the RecorderImpl and a newly created
 *  instance of an AVCodec audio encoder.  Configures the audio encoder based
 *  on the parameters of the CreateAudioTrackParam.  Also allocates a number
 *  of audio ION buffers and pushes them to the buffer queue.
 *
 *  Once setup, manages the data flow of audio buffers between the RecorderImpl
 *  and the AVCodec audio encoder.
 *
 *  After streaming has completed, destroys the instance of the AVCodec audio
 *  encoder and the data path.
 *
 *  @startuml
 *  
 *  skinparam ArrowColor blue
 *  hide footbox
 *  autonumber
 *  
 *  title Data Flow
 *  
 *  participant RecorderImpl as ri
 *  participant AudioEncoderCore as aec
 *  participant AudioTrackEncoder as ate
 *  participant AVCodec as avc
 *  
 *  ate -> avc : AudioTrackEncoder::GetBuffer()
 *  avc -> ate : AudioTrackEncoder::ReturnBuffer()
 *  ate -> ri  : RecorderImpl::AudioTrackBufferCb()
 *  aec -> ate : AudioTrackEncoder::OnBufferReturnFromClient()
 *  
 *  @enduml
 */
class AudioTrackEncoder : public ::qmmf::avcodec::ICodecSource {
 public:

  //! Default constructor
  AudioTrackEncoder();

  //! Destructor
  virtual ~AudioTrackEncoder();

  /*! @brief Initializes this object.
   *
   *  @param [in] params Given parameters for audio path configuration.
   *  @returns Status indicating success or failure.
   */
  status_t Init(const AudioTrackParams& params);

  /*! @brief Starts the data flow.
   *
   *  @param [in] track_source Handle to the track source feeding the AVCodec.
   *  @param [in] track_encoder Handle to this object.
   *  @returns Status indicating success or failure.
   */
  status_t Start(const ::std::shared_ptr<ICodecSource> &track_source,
                 const ::std::shared_ptr<ICodecSource> &track_encoder);

  /*! @brief Stops the data flow.
   *
   *  @returns Status indicating success or failure.
   */
  status_t Stop();

  /*! @brief Pauses the data flow.
   *
   *  @returns Status indicating success or failure.
   */
  status_t Pause();

  /*! @brief Resumes the data flow.
   *
   *  @returns Status indicating success or failure.
   */
  status_t Resume();


  /*! @brief Sets a parameter to a new value.
   *
   *  @param [in] param_type Indicates which parameter to set.
   *  @param [in] param New parameter value.
   *  @param [in] param_size Size (in bytes) of the new value.
   *  @returns Status indicating success or failure.
   */
  status_t SetParam(CodecParamType param_type, void* param,
                    uint32_t param_size);

  //! @copydoc ICodecSource::GetBuffer()
  status_t GetBuffer(BufferDescriptor& codec_buffer,
                     void* client_data) override;

  //! @copydoc ICodecSource::ReturnBuffer()
  status_t ReturnBuffer(BufferDescriptor& codec_buffer,
                        void* client_data) override;

  //! @copydoc ICodecSource::NotifyPortEvent()
  status_t NotifyPortEvent(::qmmf::avcodec::PortEventType event_type,
                           void* event_data) override;

  /*! @brief Handles returned buffers from RecorderImpl.
   *
   *  @param [in] buffers Vector of returned buffers.
   *  @returns Status indicating success or failure.
   */
  status_t OnBufferReturnFromClient(const std::vector<BnBuffer> &buffers);

 private:
  AudioTrackParams track_params_;  //!< Saved set of given parameters
  ::qmmf::avcodec::AVCodec* avcodec_;  //!< Handle to audio encoder
  ::std::queue<BufferDescriptor> buffers_;  //!< Queue of allocated buffers
  RecorderIon ion_;  //!< Handle to ION wrapper for Recorder

  std::mutex mutex_;  //!< Protects the buffer queue
  QCondition signal_;  //!< Sync mechanism for buffer queue access

  bool eos_received_;  //!< Indicates last buffer received from RecorderImpl
  ::std::mutex eos_mutex_;  //!< Mutex protecting the EOS flag
  QCondition eos_signal_;  //!< Sync mechanism for EOS flag access
};

/*! @brief Routes commands and data between the RecorderImpl and multiple
 *                AudioTrackEncoders.
 *
 *  A singleton that manages the life-cycle of per-track instances of
 *  AudioTrackEncoders.  In addition, routes commands and data between the
 *  RecorderImpl and each AudioTrackEncoder instance.
 */
class AudioEncoderCore {
 public:

  /*! @brief Factory method for the singleton.
   *
   *  @returns Pointer to the singleton.
   */
  static AudioEncoderCore* CreateAudioEncoderCore();

  //! Destructor
  virtual ~AudioEncoderCore();

  /*! @brief Creates a new AudioTrackEncoder instance.
   *
   *  @param [in] params Given parameters for audio path configuration.
   *  @returns Status indicating success or failure.
   */
  status_t AddSource(const AudioTrackParams& params);

  /*! @brief Deletes an AudioTrackEncoder instance.
   *
   *  @param [in] track_id Identifies a particular AudioTrackEncoder.
   *  @returns Status indicating success or failure.
   */
  status_t DeleteTrackEncoder(const uint32_t track_id);

  /*! @brief Invokes the start command on the specified AudioTrackEncoder.
   *
   *  @param [in] track_id Identifies a particular AudioTrackEncoder.
   *  @param [in] track_source Handle to the track source feeding the AVCodec.
   *  @returns Status indicating success or failure.
   */
  status_t StartTrackEncoder(const uint32_t track_id,
                             const ::std::shared_ptr<IAudioTrackSource>&
                             track_source);

  /*! @brief Invokes the stop command on the specified AudioTrackEncoder.
   *
   *  @param [in] track_id Identifies a particular AudioTrackEncoder.
   *  @returns Status indicating success or failure.
   */
  status_t StopTrackEncoder(const uint32_t track_id);

  /*! @brief Invokes the pause command on the specified AudioTrackEncoder.
   *
   *  @param [in] track_id Identifies a particular AudioTrackEncoder.
   *  @returns Status indicating success or failure.
   */
  status_t PauseTrackEncoder(const uint32_t track_id);

  /*! @brief Invokes the resume command on the specified AudioTrackEncoder.
   *
   *  @param [in] track_id Identifies a particular AudioTrackEncoder.
   *  @returns Status indicating success or failure.
   */
  status_t ResumeTrackEncoder(const uint32_t track_id);

  /*! @brief Sets a parameter for the specified AudioTrackEncoder to a new value.
   *
   *  @param [in] track_id Identifies a particular AudioTrackEncoder.
   *  @param [in] param_type Indicates which parameter to set.
   *  @param [in] param New parameter value.
   *  @param [in] param_size Size (in bytes) of the new value.
   *  @returns Status indicating success or failure.
   */
  status_t SetTrackEncoderParam(const uint32_t track_id,
                                const CodecParamType param_type, void* param,
                                const uint32_t param_size);

  /*! @brief Sends returned buffers from RecorderImpl to the specified
   *         AudioTrackEncoder.
   *
   *  @param [in] track_id Identifies a particular AudioTrackEncoder.
   *  @param [in] buffers Vector of returned buffers.
   *  @returns Status indicating success or failure.
   */
  status_t ReturnTrackBuffer(const uint32_t track_id,
                             const std::vector<BnBuffer>& buffers);

 private:
  //! Associates a track id with an AudioTrackEncoder.
  typedef ::std::map<uint32_t, ::std::shared_ptr<AudioTrackEncoder>>
          AudioTrackEncoderMap;

  //! Default constructor
  AudioEncoderCore();
  static AudioEncoderCore* instance_; //!< Pointer to the singleton.

  AudioTrackEncoderMap track_encoder_map_; //!< Map of AudioTrackEncoders.
  std::mutex track_encoder_map_lock_; //!< Protects the AudioTrackEncoder map.

  // disable copy, assignment, and move
  AudioEncoderCore(const AudioEncoderCore&) = delete;
  AudioEncoderCore(AudioEncoderCore&&) = delete;
  AudioEncoderCore& operator=(const AudioEncoderCore&) = delete;
  AudioEncoderCore& operator=(const AudioEncoderCore&&) = delete;
};

}; // namespace recorder
}; // namespace qmmf
