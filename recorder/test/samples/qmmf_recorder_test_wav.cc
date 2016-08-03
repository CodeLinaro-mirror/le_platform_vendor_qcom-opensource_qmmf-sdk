/*
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
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

#define TAG "RecorderTestWav"

#include "recorder/test/samples/qmmf_recorder_test_wav.h"

#include <cerrno>
#include <fstream>
#include <ios>
#include <iostream>
#include <cstdint>
#include <string>

#include "include/qmmf-sdk/qmmf_recorder_params.h"
#include "common/qmmf_log.h"

using ::qmmf::AudioFormat;
using ::qmmf::recorder::AudioTrackCreateParam;
using ::qmmf::recorder::TrackBuffer;
using ::std::ios;
using ::std::ofstream;
using ::std::streampos;
using ::std::string;

static const uint32_t kIdRiff = 0x46464952;
static const uint32_t kIdWave = 0x45564157;
static const uint32_t kIdFmt  = 0x20746d66;
static const uint32_t kIdData = 0x61746164;
static const uint16_t kFormatPcm = 1;

static const char *kFilenameSuffix = ".wav";

RecorderTestWav::RecorderTestWav() : current_data_size_(0) {
  QMMF_DEBUG("%s: %s() TRACE", TAG, __func__);
}

RecorderTestWav::~RecorderTestWav() {
  QMMF_DEBUG("%s: %s() TRACE", TAG, __func__);
}

int RecorderTestWav::Configure(const string& filename_prefix,
                               const AudioTrackCreateParam& params) {
  QMMF_DEBUG("%s: %s() TRACE", TAG, __func__);
  QMMF_VERBOSE("%s: %s() INPARAM: filename_prefix[%s]", TAG, __func__,
               filename_prefix.c_str());
  QMMF_VERBOSE("%s: %s() INPARAM: params.sample_rate[%u]", TAG, __func__,
               params.sample_rate);
  QMMF_VERBOSE("%s: %s() INPARAM: params.channels[%u]", TAG, __func__,
               params.channels);
  QMMF_VERBOSE("%s: %s() INPARAM: params.bit_depth[%u]", TAG, __func__,
               params.bit_depth);
  QMMF_VERBOSE("%s: %s() INPARAM: params.format_type[%u]", TAG, __func__,
               params.format_type);

  if (params.format_type != AudioFormat::kPCM) {
    QMMF_ERROR("%s: %s() non-PCM format given: %d", TAG, __func__,
               static_cast<int>(params.format_type));
    return -EINVAL;
  }

  filename_ = filename_prefix;
  filename_.append(kFilenameSuffix);

  header_.riff_header.riff_id = kIdRiff;
  header_.riff_header.riff_size = 0;
  header_.riff_header.wave_id = kIdWave;

  header_.chunk_header.format_id = kIdFmt;
  header_.chunk_header.format_size = sizeof header_.chunk_format;

  header_.chunk_format.audio_format = kFormatPcm;
  header_.chunk_format.num_channels = params.channels;
  header_.chunk_format.sample_rate = params.sample_rate;
  header_.chunk_format.bits_per_sample = params.bit_depth;
  header_.chunk_format.byte_rate = (params.bit_depth / 8) * params.channels *
                                   params.sample_rate;
  header_.chunk_format.block_align = params.channels * (params.bit_depth / 8);

  header_.data_header.data_id = kIdData;

  return 0;
}

int RecorderTestWav::Open() {
  QMMF_DEBUG("%s: %s() TRACE", TAG, __func__);

  if (filename_.empty()) {
    QMMF_ERROR("%s: %s() called in unconfigured state", TAG, __func__);
    return -EPERM;
  }

  output_.open(filename_.c_str(), ios::out | ios::binary | ios::trunc);
  if (!output_.is_open()) {
    QMMF_ERROR("%s: %s() error opening file[%s]", TAG, __func__,
               filename_.c_str());
    return -EBADF;
  }

  output_.seekp(sizeof header_, ios::beg);
  current_data_size_ = 0;

  return 0;
}

void RecorderTestWav::Close() {
  QMMF_DEBUG("%s: %s() TRACE", TAG, __func__);

  if (output_.is_open()) {
    int frames = current_data_size_ / (header_.chunk_format.num_channels *
                                       header_.chunk_format.bits_per_sample
                                       / 8);
    QMMF_INFO("%s: %s() captured %d frames", TAG, __func__, frames);

    /* finalize the file */
    header_.data_header.data_size = frames * header_.chunk_format.block_align;
    header_.riff_header.riff_size = header_.data_header.data_size +
                                    sizeof(header_) - 8;
    output_.seekp(0, ios::beg);
    output_.write(reinterpret_cast<char *>(&header_), sizeof header_);

    output_.close();
  }
}

int RecorderTestWav::Write(const TrackBuffer& buffer) {
  QMMF_DEBUG("%s: %s() TRACE", TAG, __func__);
  QMMF_VERBOSE("%s: %s() INPARAM: buffer.data[%p]", TAG, __func__, buffer.data);
  QMMF_VERBOSE("%s: %s() INPARAM: buffer.size[%zu]", TAG, __func__,
               buffer.size);

  streampos before = output_.tellp();
  output_.write(reinterpret_cast<const char *>(buffer.data),
                buffer.size);
  streampos after = output_.tellp();

  current_data_size_ += after - before;

  return 0;
}
