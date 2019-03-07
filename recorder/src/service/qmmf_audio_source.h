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

//! @file qmmf_audio_source.h

#pragma once

#include <atomic>
#include <map>
#include <memory>
#include <string>

#include "recorder/src/service/qmmf_audio_track_source.h"
#include "recorder/src/service/qmmf_recorder_common.h"

namespace qmmf {
namespace recorder {

/*! @brief Routes commands and data between the RecorderImpl and multiple
 *                IAudioTrackSource implementations.
 *
 *  A singleton that manages the life-cycle of per-track instances of
 *  IAudioTrackSource implementations.  In addition, routes commands and data
 *  between the RecorderImpl and each IAudioTrackSource-implemented instance.
 */
class AudioSource {
 public:

  /*! @brief Factory method for the singleton.
   *
   *  @returns Pointer to the singleton.
   */
  static AudioSource* CreateAudioSource();

  //! Destructor
  ~AudioSource();

  /*! @brief Creates a new IAudioTrackSource-implemented instance.
   *
   *  @param [in] track_id Used to identify this particular
   *                       IAudioTrackSource-implemented instance.
   *  @param [in] params Given parameters for audio path configuration.
   *  @returns Status indicating success or failure.
   */
  status_t CreateTrackSource(const uint32_t track_id, AudioTrackParams& params);

  /*! @brief Deletes an IAudioTrackSource-implemented instance.
   *
   *  @param [in] track_id Identifies a particular IAudioTrackSource-implemented
   *                       instance.
   *  @returns Status indicating success or failure.
   */
  status_t DeleteTrackSource(const uint32_t track_id);

  /*! @brief Invokes the start command on the specified
   *         IAudioTrackSource-implemented instance.
   *
   *  @param [in] track_id Identifies a particular IAudioTrackSource-implemented
   *                       instance.
   *  @returns Status indicating success or failure.
   */
  status_t StartTrackSource(const uint32_t track_id);

  /*! @brief Invokes the stop command on the specified
   *         IAudioTrackSource-implemented instance.
   *
   *  @param [in] track_id Identifies a particular IAudioTrackSource-implemented
   *                       instance.
   *  @returns Status indicating success or failure.
   */
  status_t StopTrackSource(const uint32_t track_id);

  /*! @brief Invokes the pause command on the specified
   *         IAudioTrackSource-implemented instance.
   *
   *  @param [in] track_id Identifies a particular IAudioTrackSource-implemented
   *                       instance.
   *  @returns Status indicating success or failure.
   */
  status_t PauseTrackSource(const uint32_t track_id);

  /*! @brief Invokes the resume command on the specified
   *         IAudioTrackSource-implemented instance.
   *
   *  @param [in] track_id Identifies a particular IAudioTrackSource-implemented
   *                       instance.
   *  @returns Status indicating success or failure.
   */
  status_t ResumeTrackSource(const uint32_t track_id);

  /*! @brief Sets a parameter for the specified IAudioTrackSource-implemented
   *         instance to a new value.
   *
   *  @param [in] track_id Identifies a particular IAudioTrackSource-implemented
   *                       instance.
   *  @param [in] key Indicates which parameter to set.
   *  @param [in] value New parameter value.
   *  @returns Status indicating success or failure.
   */
  status_t SetParameter(const uint32_t track_id,
                        const ::std::string& key,
                        const ::std::string& value);

  /*! @brief Sends returned buffers from RecorderImpl to the specified
   *         AudioRawTrackSource.
   *
   *  @param [in] track_id Identifies a particular AudioRawTrackSource.
   *  @param [in] buffers Vector of returned buffers.
   *  @returns Status indicating success or failure.
   */
  status_t ReturnTrackBuffer(const uint32_t track_id,
                             const ::std::vector<BnBuffer>& buffers);

  /*! @brief Returns a handle to the specified IAudioTrackSource-implemented
   *         instance.
   *
   *  @param [in] track_id Identifies a particular IAudioTrackSource-implemented
   *                       instance.
   *  @param [out] track_source Handle to the specified
   *                            IAudioTrackSource-implemented instance.
   *  @returns Status indicating success or failure.
   */
  status_t getTrackSource(const uint32_t track_id,
                          ::std::shared_ptr<IAudioTrackSource>* track_source);

 private:
  //! Associates a track id with an IAudioTrackSource implementation.
  typedef ::std::map<uint32_t, ::std::shared_ptr<IAudioTrackSource>>
          AudioTrackSourceMap;

  //! Default constructor
  AudioSource();
  static AudioSource* instance_; //!< Pointer to the singleton.

  //! Map of IAudioTrackSource implmentations.
  AudioTrackSourceMap track_source_map_;

  // disable copy, assignment, and move
  AudioSource(const AudioSource&) = delete;
  AudioSource(AudioSource&&) = delete;
  AudioSource& operator=(const AudioSource&) = delete;
  AudioSource& operator=(const AudioSource&&) = delete;
};

}; // namespace recorder
}; // namespace qmmf
