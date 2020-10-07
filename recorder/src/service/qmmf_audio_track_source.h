/*
 * Copyright (c) 2016-2019, 2021, The Linux Foundation. All rights reserved.
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

//! @file qmmf_audio_track_source.h

#pragma once

#include <atomic>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

#include "common/utils/qmmf_condition.h"
#include "recorder/src/service/qmmf_audio_pulse_client.h"
#include "recorder/src/service/qmmf_recorder_common.h"
#include "recorder/src/service/qmmf_recorder_ion.h"

namespace qmmf {
namespace recorder {

/*! @brief Interface of the audio track source.
 *
 *  Methods are implemented by the audio track source and RecorderImpl calls
 *  these methods.
 */
class IAudioTrackSource {
 public:

  //! Destructor
  virtual ~IAudioTrackSource() {}

  /*! @brief Initializes the internal state.
   *
   *  @returns Status indicating success or failure.
   */
  virtual status_t Init() = 0;

  /*! @brief De-initializes the internal state so that the instance may be
   *         destroyed.
   *
   *  @returns Status indicating success or failure.
   */
  virtual status_t DeInit() = 0;

  /*! @brief Starts the data flow.
   *
   *  @returns Status indicating success or failure.
   */
  virtual status_t StartTrack() = 0;

  /*! @brief Stops the data flow.
   *
   *  @returns Status indicating success or failure.
   */
  virtual status_t StopTrack() = 0;

  /*! @brief Pauses the data flow.
   *
   *  @returns Status indicating success or failure.
   */
  virtual status_t PauseTrack() = 0;

  /*! @brief Resumes the data flow.
   *
   *  @returns Status indicating success or failure.
   */
  virtual status_t ResumeTrack() = 0;

  /*! @brief Sets a parameter to a new value.
   *
   *  @param [in] key Indicates which parameter to set.
   *  @param [in] value New parameter value.
   *  @returns Status indicating success or failure.
   */
  virtual status_t SetParameter(const ::std::string& key,
                                const ::std::string& value) = 0;

  /*! @brief Handles returned buffers from RecorderImpl.
   *
   *  @param [in] buffers Vector of returned buffers.
   *  @returns Status indicating success or failure.
   */
  virtual status_t ReturnTrackBuffer(const ::std::vector<BnBuffer>& buffers) = 0;
};

/*! @brief Manages the audio data flow between RecorderImpl and AudioPulseClient.
 *
 *  Sets up the data path between the RecorderImpl and a newly created
 *  instance of an AudioPulseClient.  Configures the pulseaudio client based
 *  on the parameters of AudioTrackParam.  Also allocates a number
 *  of audio ION buffers and pushes them to the thread's buffer queue.
 *
 *  Once setup, manages the data flow of audio buffers between the RecorderImpl
 *  and the pulseaudio client.
 *
 *  After streaming has completed, destroys the instance of the AudioPulseClient
 *  and the data path.
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
 *  participant AudioRawTrackSource as arts
 *  participant AudioPulseClient as apc
 *
 *  arts -> apc : AudioPulseClient::SendBuffers()
 *  apc -> arts : AudioRawTrackSource::BufferHandler()
 *  arts -> ri  : RecorderImpl::AudioTrackBufferCb()
 *  ri -> arts  : AudioRawTrackSource::ReturnTrackBuffer()
 *
 *  @enduml
 */
class AudioRawTrackSource : public IAudioTrackSource {
 public:

  /*! @brief Constructor
   *
   *  @param [in] params Given parameters for audio path configuration.
   *  @returns Status indicating success or failure.
   */
  AudioRawTrackSource(const uint32_t id, const AudioTrackParam& params,
                      const BnBufferCallback& cb);

  //! Default constructor
  virtual ~AudioRawTrackSource();

  //! @copydoc IAudioTrackSource::Init()
  status_t Init() override;

  //! @copydoc IAudioTrackSource::Deinit()
  status_t DeInit() override;

  //! @copydoc IAudioTrackSource::StartTrack()
  status_t StartTrack() override;

  //! @copydoc IAudioTrackSource::StopTrack()
  status_t StopTrack() override;

  //! @copydoc IAudioTrackSource::PauseTrack()
  status_t PauseTrack() override;

  //! @copydoc IAudioTrackSource::ResumeTrack()
  status_t ResumeTrack() override;

  //! @copydoc IAudioTrackSource::SetParameter()
  status_t SetParameter(const ::std::string& key,
                        const ::std::string& value) override;

  //! @copydoc IAudioTrackSource::ReturnTrackBuffer()
  status_t ReturnTrackBuffer(const std::vector<BnBuffer>& buffers) override;

 private:

  //! The different types of messages that can be sent to the running thread.
  enum class AudioMessageType {
    kMessageStop, //!< Directs the thread to stop data flow.
    kMessagePause, //!< Directs the thread to pause data flow.
    kMessageResume, //!< Directs the thread to resume data flow.
    kMessageBuffer, //!< Message contains a buffer from the AudioPulseClient.
    kMessageBnBuffer, //!< Message contains a buffer from the RecorderImpl.
  };

  //! A message to be sent to the running thread.
  struct AudioMessage {
    AudioMessageType type; //!< The type of message.
    union {
      BufferDescriptor buffer; //!< Buffer from AudioPulseClient.
      BnBuffer bn_buffer; //!< Buffer from RecorderImpl.
    };
  };

  /*! @brief Entry point for the AudioRawTrackSource thread.
   *
   *  @param [in] souorce Pointer to the AudioRawTrackSource instance.
   */
  static void ThreadEntry(AudioRawTrackSource* source);

  //! The AudioRawTrackSource thread that manages data flow.
  void Thread();

  /*! @brief Handles error-related callbacks from the AudioPulseClient.
   *
   *  @param [in] error Indicates which error occurred.
   */
  void ErrorHandler(const int32_t error);

  /*! @brief Callback handler used to receive a filled buffer from the
   *         AudioPulseClient.
   *
   *  @param [in] buffer Audio data from the AudioPulseClient.
   */
  void BufferHandler(const BufferDescriptor& buffer);

  uint32_t id_ //!< Track ID
  AudioTrackParam params_; //!< Saved set of given parameters.
  BnBufferCallback buffer_cb_; //!< Buffer callback
  AudioPulseClient* pulse_client_; //!< PulseAudio client.
  RecorderIon ion_; //!< Instance of the RecorderIon.

  ::std::thread* thread_; //!< Pointer to the running thread.
  ::std::mutex message_lock_; //!< Protects the message queue.
  ::std::queue<AudioMessage> messages_; //!< Yet to be processed by the thread.
  QCondition signal_; //!< Sync mechanism for message queue access.

  // disable copy, assignment, and move
  AudioRawTrackSource(const AudioRawTrackSource&) = delete;
  AudioRawTrackSource(AudioRawTrackSource&&) = delete;
  AudioRawTrackSource& operator=(const AudioRawTrackSource&) = delete;
  AudioRawTrackSource& operator=(const AudioRawTrackSource&&) = delete;
};

}; // namespace recorder
}; // namespace qmmf
