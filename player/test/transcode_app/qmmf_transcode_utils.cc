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

namespace qmmf {
namespace transcode {

int32_t TransCodeBuffer::ion_device_ = -1;
int32_t TransCodeBuffer::num_instances_ = 0;

TransCodeBuffer::TransCodeBuffer() {
    num_instances_++;
}

TransCodeBuffer& TransCodeBuffer::operator=(const TransCodeBuffer& rhs) {
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

TransCodeBuffer::TransCodeBuffer(const TransCodeBuffer& obj) {
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

TransCodeBuffer::TransCodeBuffer(BufferOwner _owner,
                                 uint32_t _buf_id = 0) : owner_(_owner),
                                 buf_id_(_buf_id) {
  memset(&buf_info_, 0x0, sizeof(buf_info_));
  buf_info_.fd = -1;
  meta_handle_ = nullptr;
  if (num_instances_ == 0 && ion_device_ <= 0) {
    ion_device_ = open("/dev/ion", O_RDONLY);
    if(ion_device_ <= 0) {
      TEST_ERROR("%s:%s:%s Ion dev open failed %s", TAG, __func__, "BUFFER",
          strerror(errno));
      ion_device_ = -1;
      assert(0);
    } else {
      TEST_INFO("%s:%s:%s Ion dev open success %d", TAG, __func__,
          "BUFFER", ion_device_);
    }
  }
  num_instances_++;
}

TransCodeBuffer::~TransCodeBuffer() {
  num_instances_--;
  if (num_instances_ <= 0) {
    close(ion_device_);
    ion_device_ = -1;
    TEST_INFO("%s:%s:%s Ion Device Closed", TAG, __func__, "BUFFER");
  }
}

status_t TransCodeBuffer::Allocate(uint32_t size) {
  TEST_INFO("%s:%s:%s Enter", TAG, __func__, "BUFFER");
  status_t ret = 0;

  if (size <= 0) {
    TEST_ERROR("%s:%s:%s Undefined size(%d)", TAG, __func__, "BUFFER", size);
    return -1;
  }
  int nFds = 1;
  int nInts = 3;

  int32_t ionType =  ION_HEAP(ION_IOMMU_HEAP_ID);
  struct ion_allocation_data alloc;
  void* data = nullptr;
  struct ion_fd_data ionFdData;
  memset(&alloc, 0x0 , sizeof(ion_allocation_data));
  memset(&ionFdData, 0x0, sizeof(ion_fd_data));

  alloc.len = size;
  alloc.len = (alloc.len + 4095) & (~4095);
  alloc.align = 4096;
  alloc.flags = ION_FLAG_CACHED;
  alloc.heap_id_mask = ionType;

  ret = ioctl(ion_device_, ION_IOC_ALLOC, &alloc);
  if (ret < 0) {
      TEST_ERROR("%s:%s:%s ION allocation failed", TAG, __func__, "BUFFER");
      goto ION_ALLOC_FAILED;
  }

  ionFdData.handle = alloc.handle;
  ret = ioctl(ion_device_, ION_IOC_SHARE, &ionFdData);
  if (ret < 0) {
    TEST_ERROR("%s:%s:%s ION map failed %s", TAG, __func__, "BUFFER",
        strerror(errno));
    goto ION_MAP_FAILED;
  }

  data = mmap(nullptr, alloc.len, PROT_READ  | PROT_WRITE, MAP_SHARED,
              ionFdData.fd, 0);
  if(data == MAP_FAILED) {
    TEST_ERROR("%s:%s:%s  ION mmap failed: %s (%d)", TAG, __func__, "BUFFER",
        strerror(errno), errno);
    goto ION_MAP_FAILED;
  }

  memset(&buf_info_, 0x0, sizeof(buf_info_));
  buf_info_.ion_handle_ = alloc;
  buf_info_.fd = ionFdData.fd;
  buf_info_.vaddr = data;
  // size and alloc.len might be different
  //In general alloc.len >= size
  buf_info_.buf_size = alloc.len;
  buf_info_.capacity = size;

  meta_handle_ = (native_handle_create(1,16));
  if (meta_handle_ == nullptr) {
    TEST_ERROR("%s:%s:%s Failed to allocate metabuffer handle", TAG, __func__,
        "BUFFER");
    goto NATIVE_HANDLE_CREATION_FAILED;
  }

  meta_handle_->version = sizeof(native_handle_t);
  meta_handle_->numFds  = nFds;
  meta_handle_->numInts = nInts;
  meta_handle_->data[0] = ionFdData.fd;
  meta_handle_->data[1] = 0; //offset
  meta_handle_->data[2] = alloc.len;
  meta_handle_->data[3] = 0x200000;
  meta_handle_->data[4] = alloc.len;

  TEST_INFO("%s:%s:%s Exit", TAG, __func__, "BUFFER");
  return ret;

NATIVE_HANDLE_CREATION_FAILED:
ION_MAP_FAILED:
struct ion_handle_data ionHandleData;
memset(&ionHandleData, 0x0, sizeof(ionHandleData));
ionHandleData.handle = ionFdData.handle;
ioctl(ion_device_, ION_IOC_FREE, &ionHandleData);
ION_ALLOC_FAILED:
close(ion_device_);
ion_device_ = -1;
TEST_ERROR("%s:%s:%s ION Buffer allocation failed!", TAG, __func__, "BUFFER");
TEST_INFO("%s:%s:%s Ion Device Closed", TAG, __func__, "BUFFER");
TEST_INFO("%s:%s:%s Exit", TAG, __func__, "BUFFER");
return -1;
}

status_t TransCodeBuffer::Release() {
  status_t ret = 0;
  if(buf_info_.vaddr) {
    munmap(buf_info_.vaddr, buf_info_.buf_size);
    buf_info_.vaddr = nullptr;
  }
  if (buf_info_.fd) {
    ioctl(ion_device_, ION_IOC_FREE, &(buf_info_.ion_handle_));
    close(buf_info_.fd);
    buf_info_.fd = -1;
  }
  if (meta_handle_->data[0]) {
    close(meta_handle_->data[0]);
    meta_handle_->data[0] = -1;
    native_handle_delete(meta_handle_);
    meta_handle_ = nullptr;
  }
  return ret;
}

status_t AllocateBuffers(vector<TransCodeBuffer>& list,
                        shared_ptr<IAVCodec>& _avcodec,
                        BufferOwner _owner, const uint32_t& port_index_) {
  TEST_INFO("%s:%s:%s Enter", TAG, __func__, "BUFFER");
  status_t ret = 0;

  if (!list.empty()) {
    TEST_ERROR("%s:%s:%s buffer_list_ is not Empty", TAG, __func__, "BUFFER");
    return -1;
  }

  uint32_t count, size;
  ret = _avcodec->GetBufferRequirements(port_index_, &count, &size);
  if(ret != OK) {
    TEST_INFO("%s:%s:%s Failed to get Buffer Requirements on CoreSide",
        TAG, __func__, "BUFFER");
    return ret;
  }

  //comment If part in case of Video EncodeDecode session
  if (_owner == BufferOwner::kTransCoderSink) {
    count = 6;
  }

  for (uint32_t i = 0; i < count ; i++) {
    TransCodeBuffer buffer(_owner, OWNER_INDEX(_owner) | i);
    ret = buffer.Allocate(size);
    assert(ret == 0);
    TEST_INFO("%s:%s FD(%d)", TAG, __func__, buffer.Fd());
    list.push_back(buffer);
  }

  TEST_INFO("%s:%s:%s Exit", TAG, __func__, "BUFFER");
  return ret;
}

status_t ReleaseBuffers(vector<TransCodeBuffer>& list) {
  TEST_INFO("%s:%s:%s Enter", TAG, __func__, "BUFFER");
  status_t ret = 0;

  if (list.empty()) {
    TEST_ERROR("%s:%s:%s buffer_list_ is already empty", TAG, __func__,
        "BUFFER");
    return -1;
  }

  for (auto& iter: list) {
    ret = iter.Release();
    assert(ret == 0);
  }

  list.clear();
  TEST_INFO("%s:%s:%s Exit", TAG, __func__, "BUFFER");
  return ret;
}

};  // namespace transcode
};  // namespace qmmf