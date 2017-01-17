/*
* Copyright (c) 2017, The Linux Foundation. All rights reserved.
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

#include "qmmf_transcode_params.h"

#define TAG "TranscodeBuffer"

namespace qmmf {
namespace transcode {

using ::qmmf::avcodec::IAVCodec;
using ::std::shared_ptr;
using ::std::string;
using ::std::vector;

int32_t TranscodeBuffer::ion_device_ = -1;
TSInt32 TranscodeBuffer::num_instances_;

TranscodeBuffer& TranscodeBuffer::operator=(const TranscodeBuffer& rhs) {
  meta_handle_  = rhs.meta_handle_;
  owner_        = rhs.owner_;
  buf_info_     = rhs.buf_info_;
  buf_id_       = rhs.buf_id_;
  timestamp_    = rhs.timestamp_;
  flag_         = rhs.flag_;
  filled_size_  = rhs.filled_size_;
  offset_       = rhs.offset_;

  return *this;
}

TranscodeBuffer::TranscodeBuffer(const TranscodeBuffer& obj) {
  meta_handle_  = obj.meta_handle_;
  owner_        = obj.owner_;
  buf_info_     = obj.buf_info_;
  buf_id_       = obj.buf_id_;
  timestamp_    = obj.timestamp_;
  flag_         = obj.flag_;
  filled_size_  = obj.filled_size_;
  offset_       = obj.offset_;

  num_instances_++;
}

TranscodeBuffer::TranscodeBuffer(const BufferOwner owner,
                                 const uint32_t buf_id = 0)
    : owner_(owner), buf_id_(buf_id) {
  memset(&buf_info_, 0x0, sizeof(buf_info_));
  buf_info_.fd = -1;
  meta_handle_ = nullptr;
  if (num_instances_.value() == 0 && ion_device_ <= 0) {
    ion_device_ = open("/dev/ion", O_RDONLY);
    if (ion_device_ <= 0) {
      QMMF_ERROR("%s:%s Ion dev open failed %s", TAG, __func__,
                 strerror(errno));
      ion_device_ = -1;
      assert(0);
    } else {
      QMMF_INFO("%s:%s Ion dev open success %d", TAG, __func__, ion_device_);
    }
  }
  num_instances_++;
}

TranscodeBuffer::~TranscodeBuffer() {
  num_instances_--;
  if (num_instances_.value() <= 0) {
    close(ion_device_);
    ion_device_ = -1;
    QMMF_INFO("%s:%s Ion Device Closed", TAG, __func__);
  }
}

status_t TranscodeBuffer::Allocate(const uint32_t size) {
  QMMF_DEBUG("%s:%s Enter", TAG, __func__);

  status_t ret = 0;
  if (size <= 0) {
    QMMF_ERROR("%s:%s Undefined size(%u)", TAG, __func__, size);
    return -1;
  }

  int nFds = 1;
  int nInts = 3;
  int32_t ionType = ION_HEAP(ION_IOMMU_HEAP_ID);
  struct ion_allocation_data alloc;
  void* data = nullptr;
  struct ion_fd_data ionFdData;
  memset(&alloc, 0x0, sizeof(ion_allocation_data));
  memset(&ionFdData, 0x0, sizeof(ion_fd_data));

  alloc.len = size;
  alloc.len = (alloc.len + 4095) & (~4095);
  alloc.align = 4096;
  alloc.flags = ION_FLAG_CACHED;
  alloc.heap_id_mask = ionType;

  ret = ioctl(ion_device_, ION_IOC_ALLOC, &alloc);
  if (ret < 0) {
    QMMF_ERROR("%s:%s ION allocation failed", TAG, __func__);
    goto ION_ALLOC_FAILED;
  }

  ionFdData.handle = alloc.handle;
  ret = ioctl(ion_device_, ION_IOC_SHARE, &ionFdData);
  if (ret < 0) {
    QMMF_ERROR("%s:%s ION map failed %s", TAG, __func__, strerror(errno));
    goto ION_MAP_FAILED;
  }

  data = mmap(nullptr, alloc.len, PROT_READ | PROT_WRITE, MAP_SHARED,
              ionFdData.fd, 0);
  if (data == MAP_FAILED) {
    QMMF_ERROR("%s:%s  ION mmap failed: %s (%d)", TAG, __func__,
               strerror(errno), errno);
    goto ION_MAP_FAILED;
  }

  memset(&buf_info_, 0x0, sizeof(buf_info_));
  buf_info_.ion_handle = alloc;
  buf_info_.fd = ionFdData.fd;
  buf_info_.vaddr = data;
  // size and alloc.len might be different
  // In general alloc.len >= size
  buf_info_.buf_size = alloc.len;
  buf_info_.capacity = size;

  meta_handle_ = (native_handle_create(1, 16));
  if (meta_handle_ == nullptr) {
    QMMF_ERROR("%s:%s Failed to allocate metabuffer handle", TAG, __func__);
    goto NATIVE_HANDLE_CREATION_FAILED;
  }

  meta_handle_->version = sizeof(native_handle_t);
  meta_handle_->numFds  = nFds;
  meta_handle_->numInts = nInts;
  meta_handle_->data[0] = ionFdData.fd;
  meta_handle_->data[1] = 0;  // offset
  meta_handle_->data[2] = alloc.len;
  meta_handle_->data[3] = 0x200000;
  meta_handle_->data[4] = alloc.len;

  QMMF_DEBUG("%s:%s Exit", TAG, __func__);
  return ret;

NATIVE_HANDLE_CREATION_FAILED:
ION_MAP_FAILED:
  struct ion_handle_data ionHandleData;
  memset(&ionHandleData, 0x0, sizeof(ionHandleData));
  ionHandleData.handle = ionFdData.handle;
  ioctl(ion_device_, ION_IOC_FREE, &ionHandleData);

ION_ALLOC_FAILED:
  QMMF_ERROR("%s:%s ION Buffer allocation failed!", TAG, __func__);
  QMMF_DEBUG("%s:%s Exit", TAG, __func__);
  return -1;
}

void TranscodeBuffer::Release() {
  QMMF_DEBUG("%s:%s Enter", TAG, __func__);

  if (buf_info_.vaddr) {
    munmap(buf_info_.vaddr, buf_info_.buf_size);
    buf_info_.vaddr = nullptr;
  }

  QMMF_DEBUG("%s:%s Releasing buffer with fd(%d)", TAG, __func__, buf_info_.fd);

  if (buf_info_.fd) {
    ioctl(ion_device_, ION_IOC_FREE, &(buf_info_.ion_handle));
    close(buf_info_.fd);
    buf_info_.fd = -1;
  }

  if (meta_handle_->data[0]) {
    close(meta_handle_->data[0]);
    meta_handle_->data[0] = -1;
    native_handle_delete(meta_handle_);
    meta_handle_ = nullptr;
  }

  QMMF_DEBUG("%s:%s Exit", TAG, __func__);
}

status_t TranscodeBuffer::CreateTranscodeBuffersVector(
    const shared_ptr<IAVCodec>& avcodec,
    const BufferOwner owner, const uint32_t port_index,
    vector<TranscodeBuffer>* buffer_list) {
  QMMF_INFO("%s:%s Enter", TAG, __func__);

  status_t ret = 0;
  if (buffer_list == nullptr) {
    QMMF_ERROR("%s:%s Invalid Parameters : buffer_list = nullptr",
               TAG, __func__);
    return -1;
  }

  if (!buffer_list->empty()) {
    QMMF_ERROR("%s:%s buffer_list_ is not Empty", TAG, __func__);
    return -1;
  }

  uint32_t count, size;
  ret = avcodec->GetBufferRequirements(port_index, &count, &size);
  if (ret != 0) {
    QMMF_ERROR("%s:%s Failed to get Buffer Requirements", TAG, __func__);
    return ret;
  }

  for (uint32_t i = 0; i < count; i++) {
    TranscodeBuffer buffer(owner, OwnerIndex(owner) | i);
    ret = buffer.Allocate(size);
    if (ret != 0) {
      QMMF_ERROR("%s:%s Failed to allocate %uth buffer", TAG, __func__, i + 1);
      goto release_buffer;
    }
    QMMF_INFO("%s:%s Buffer allocated with fd(%d)",
              TAG, __func__, buffer.GetFd());
    buffer_list->push_back(buffer);
  }

  QMMF_INFO("%s:%s Exit", TAG, __func__);
  return ret;

release_buffer:
  TranscodeBuffer::FreeTranscodeBuffersVector(buffer_list);
  assert(buffer_list->empty());
  QMMF_INFO("%s:%s Exit", TAG, __func__);
  return ret;
}

void TranscodeBuffer::FreeTranscodeBuffersVector(
    vector<TranscodeBuffer>* buffer_list) {
  QMMF_INFO("%s:%s Enter", TAG, __func__);

  if (buffer_list == nullptr)
    return;
  if (buffer_list->empty())
    return;

  while (!buffer_list->empty()) {
    buffer_list->begin()->Release();
    buffer_list->erase(buffer_list->begin());
  }

  QMMF_INFO("%s:%s Exit", TAG, __func__);
}

FramerateCalculator::FramerateCalculator(uint64_t period, string str)
    : count_(0), period_(period), log_str_(str), is_first_trigger_(true) {
  QMMF_DEBUG("%s: Enter", __func__);
  QMMF_DEBUG("%s: Exit", __func__);
}

FramerateCalculator::~FramerateCalculator() {
  QMMF_DEBUG("%s: Enter", __func__);
  QMMF_DEBUG("%s: Exit", __func__);
}

void FramerateCalculator::Trigger() {
  QMMF_VERBOSE("%s: Enter", __func__);

  if (is_first_trigger_) {
    is_first_trigger_ = false;
    gettimeofday(&prevtv_, nullptr);
  } else {
    count_++;
    gettimeofday(&currtv_, nullptr);
    uint64_t time_diff =
        (uint64_t)((currtv_.tv_sec * 1000000 + currtv_.tv_usec) -
                   (prevtv_.tv_sec * 1000000 + prevtv_.tv_usec));
    if (time_diff >= period_) {
      float framerate = (count_ * 1000000) / (float)time_diff;
      QMMF_INFO("%s(%0.2f)", log_str_.c_str(), framerate);
      count_ = 0;
      prevtv_ = currtv_;
    }
  }

  QMMF_VERBOSE("%s: Exit", __func__);
}

void FramerateCalculator::Reset() {
  QMMF_DEBUG("%s: Enter", __func__);

  is_first_trigger_ = true;
  count_ = 0;

  QMMF_DEBUG("%s: Exit", __func__);
}

};  // namespace transcode
};  // namespace qmmf
