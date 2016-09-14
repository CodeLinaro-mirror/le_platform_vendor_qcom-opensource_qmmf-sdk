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

#define TAG "RecorderIon"

#include "recorder/src/service/qmmf_recorder_ion.h"

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <map>

#include <linux/msm_ion.h>

#include "common/audio/inc/qmmf_audio_definitions.h"
#include "common/qmmf_log.h"
#include "recorder/src/service/qmmf_recorder_common.h"

namespace qmmf {
namespace recorder {

using ::qmmf::common::audio::AudioBuffer;
using ::qmmf::common::audio::AudioBufferList;
using ::std::map;

static const char* kIonFilename = "/dev/ion";
static const int kBufferAlign   = 4096;

RecorderIon::RecorderIon()
    : ion_device_(-1), buffer_size_(0), request_size_(0) {
  QMMF_DEBUG("%s: %s() TRACE", TAG, __func__);
}

RecorderIon::~RecorderIon() {
  QMMF_DEBUG("%s: %s() TRACE", TAG, __func__);

  if (!buffer_map_.empty()) {
    int result = Deallocate();
    assert(result == 0);
    QMMF_INFO("%s: %s() deallocated all ion buffers", TAG, __func__);
  }
}

int RecorderIon::Allocate(int number, int size) {
  QMMF_DEBUG("%s: %s() TRACE", TAG, __func__);
  QMMF_VERBOSE("%s: %s() INPARAM: number[%d]", TAG, __func__, number);
  QMMF_VERBOSE("%s: %s() INPARAM: size[%d]", TAG, __func__, size);

  if (number <= 0) return -EINVAL;
  if (size <= 0) return -EINVAL;

  request_size_ = size;
  buffer_size_ = (size + kBufferAlign - 1) & ~(kBufferAlign - 1);

  /* open ion device */
  ion_device_ = open(kIonFilename, O_RDONLY);
  if (ion_device_ < 0) {
    QMMF_ERROR("%s: %s() error opening ion device: %d[%s]", TAG, __func__,
               errno, strerror(errno));
    return errno;
  }

  for (auto index = 0; index < number; ++index) {
    RecorderIonBuffer buffer;
    int result;

    buffer.allocate_data.len = buffer_size_;
    buffer.allocate_data.align = kBufferAlign;
    buffer.allocate_data.heap_id_mask = ION_HEAP(ION_SYSTEM_HEAP_ID);
    buffer.allocate_data.flags = 0;
    buffer.allocate_data.handle = 0;

    /* allocate ion buffer */
    result = ioctl(ion_device_, ION_IOC_ALLOC, &buffer.allocate_data);
    if (result < 0) {
      QMMF_ERROR("%s: %s() ION_IOC_ALLOC ioctl command failed: %d[%s]", TAG,
                 __func__, errno, strerror(errno));
      return errno;
    }

    buffer.free_data.handle = buffer.allocate_data.handle;
    buffer.share_data.handle = buffer.allocate_data.handle;
    buffer.share_data.fd = -1;

    /* obtain unique fd for sharing */
    result = ioctl(ion_device_, ION_IOC_SHARE, &buffer.share_data);
    if (result < 0) {
      QMMF_ERROR("%s: %s() ION_IOC_SHARE ioctl command failed: %d[%s]", TAG,
                 __func__, errno, strerror(errno));

      /* on error, attempt to deallocate the ion buffer */
      result = ioctl(ion_device_, ION_IOC_FREE, &buffer.free_data);
      if (result < 0) {
        QMMF_ERROR("%s: %s() ION_IOC_FREE ioctl command failed: %d[%s]", TAG,
                    __func__, errno, strerror(errno));
        QMMF_ERROR("%s: %s() [CRITICAL] ion memory has leaked", TAG, __func__);
      }
      return errno;
    }

    buffer_map_.insert({buffer.share_data.fd, buffer});
  }

  /* map buffers into address space */
  for (auto& buffer : buffer_map_) {
    buffer.second.data = mmap(NULL, buffer_size_, PROT_READ | PROT_WRITE,
                               MAP_SHARED, buffer.second.share_data.fd, 0);
    if (buffer.second.data == MAP_FAILED) {
      QMMF_ERROR("%s: %s() unable to map buffer[%d]: %d[%s]", TAG, __func__,
                 buffer.second.share_data.fd, errno, strerror(errno));
      return errno;
    }

    QMMF_VERBOSE("%s: %s() allocated ion buffer[%d][%s]", TAG, __func__,
                 buffer.first, buffer.second.ToString().c_str());
  }

  return 0;
}

int RecorderIon::Deallocate() {
  QMMF_DEBUG("%s: %s() TRACE", TAG, __func__);

  if (ion_device_ == -1) {
    QMMF_WARN("%s: %s() ion device is not opened", TAG, __func__);
    return 0;
  }

  for (auto& buffer : buffer_map_) {
    int result;

    QMMF_VERBOSE("%s: %s() deallocating ion buffer[%s]", TAG, __func__,
                 buffer.second.ToString().c_str());

    /* unmap buffer from address space */
    result = munmap(buffer.second.data, buffer_size_);
    if (result < 0)
      QMMF_ERROR("%s: %s() unable to unmap buffer[%d]: %d[%s]", TAG, __func__,
                 buffer.second.share_data.fd, errno, strerror(errno));
    buffer.second.data = nullptr;

    /* close fd for sharing */
    result = close(buffer.second.share_data.fd);
    if (result < 0) {
      QMMF_ERROR("%s: %s() error closing shared fd[%d]: %d[%s]", TAG, __func__,
                 buffer.second.share_data.fd, errno, strerror(errno));
      return errno;
    }
    buffer.second.share_data.fd = -1;

    /* free ion buffer */
    result = ioctl(ion_device_, ION_IOC_FREE, &buffer.second.free_data);
    if (result < 0) {
      QMMF_ERROR("%s: %s() ION_IOC_FREE ioctl command failed: %d[%s]", TAG,
                  __func__, errno, strerror(errno));
      QMMF_ERROR("%s: %s() [CRITICAL] ion memory has leaked", TAG, __func__);
    }

    buffer_map_.erase(buffer.first);
  }
  QMMF_INFO("%s: %s() deallocated all ion buffers", TAG, __func__);

  /* close ion device */
  int result = close(ion_device_);
  if (result < 0) {
    QMMF_ERROR("%s: %s() error closing ion device[%d]: %d[%s]", TAG, __func__,
               ion_device_, errno, strerror(errno));
    return errno;
  }

  ion_device_ = -1;
  buffer_size_ = 0;
  request_size_ = 0;

  return 0;
}

int RecorderIon::GetList(AudioBufferList* buffers) {
  QMMF_DEBUG("%s: %s() TRACE", TAG, __func__);

  if (buffer_map_.empty()) {
    QMMF_WARN("%s: %s() no ion buffers allocated", TAG, __func__);
    return 0;
  }

  for (auto& buffer : buffer_map_) {
    AudioBuffer export_buffer = {
        /* data      */ buffer.second.data,
        /* ion_fd    */ buffer.second.share_data.fd,
        /* buffer_id */ buffer.second.share_data.fd,
        /* capacity  */ request_size_,
        /* size      */ 0,
        /* timestamp */ 0,
        /* flags     */ 0 };

    QMMF_VERBOSE("%s: %s() OUTPARAM: export_buffer[%s]", TAG, __func__,
                 export_buffer.ToString().c_str());
    buffers->list.push_back(export_buffer);
  }

  return 0;
}

int RecorderIon::Import(const BnBuffer& bn_buffer, AudioBuffer* buffer) {
  QMMF_DEBUG("%s: %s() TRACE", TAG, __func__);
  QMMF_VERBOSE("%s:%s INPARAM: bn_buffer[%s]", TAG, __func__,
               bn_buffer.ToString().c_str());

  auto ion_buffer = buffer_map_.find(bn_buffer.buffer_id);
  if (ion_buffer == buffer_map_.end()) {
    QMMF_ERROR("%s: %s() no ion buffer for key[%d]", TAG, __func__,
               bn_buffer.buffer_id);
    return -EINVAL;
  }

  buffer->data = ion_buffer->second.data;
  buffer->ion_fd = -1;
  buffer->buffer_id = bn_buffer.buffer_id;
  buffer->capacity = bn_buffer.capacity;
  buffer->size = bn_buffer.size;
  buffer->timestamp = bn_buffer.timestamp;
  buffer->flags = bn_buffer.flag;


  QMMF_VERBOSE("%s: %s() OUTPARAM: buffer[%s]", TAG, __func__,
               buffer->ToString().c_str());
  return 0;
}

int RecorderIon::Export(const AudioBuffer& buffer, BnBuffer* bn_buffer) {
  QMMF_DEBUG("%s: %s() TRACE", TAG, __func__);
  QMMF_VERBOSE("%s:%s INPARAM: buffer[%s]", TAG, __func__,
               buffer.ToString().c_str());

  auto ion_buffer = buffer_map_.find(buffer.buffer_id);
  if (ion_buffer == buffer_map_.end()) {
    QMMF_ERROR("%s: %s() no ion buffer for key[%d]", TAG, __func__,
               buffer.buffer_id);
    return -EINVAL;
  }

  bn_buffer->ion_fd = ion_buffer->second.share_data.fd;
  bn_buffer->size = buffer.size;
  bn_buffer->timestamp = buffer.timestamp;
  bn_buffer->width = 0;
  bn_buffer->height = 0;
  bn_buffer->buffer_id = buffer.buffer_id;
  bn_buffer->flag = buffer.flags;
  bn_buffer->capacity = buffer.capacity;

  QMMF_VERBOSE("%s: %s() OUTPARAM: bn_buffer[%s]", TAG, __func__,
               bn_buffer->ToString().c_str());
  return 0;
}

}; /* namespace recorder */
}; /* namespace qmmf */
