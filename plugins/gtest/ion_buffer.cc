/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
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

#define LOG_TAG "PlatformBuffer"

#include <fcntl.h>
#include <linux/msm_ion.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cstring>

#include <qmmf-alg/qmmf_alg_utils.h>

#include "ion_buffer.h"

namespace qmmf {
namespace qmmf_alg_plugin {

/** PlatformBuffer
 *    @size: size of the requested buffer
 *    @cached: flag indicating whether buffer is cached
 *
 * Constructs PlatformBuffer
 *
 * return: void
 **/
PlatformBuffer::PlatformBuffer(uint32_t size, bool cached)
    : addr_(static_cast<uint8_t*>(MAP_FAILED)),
      size_(size),
      fd_(-1),
      ion_fd_(-1),
      cached_(cached) {
  ion_fd_ = open("/dev/ion", O_RDONLY);
  if (ion_fd_ < 0) {
    Utils::ThrowException(__func__, "Open ion device failed");
  }

  struct ion_allocation_data alloc;
  memset(&alloc, 0, sizeof(alloc));
  alloc.len = size;
  alloc.align = 0;
  alloc.heap_id_mask = 0x1 << ION_IOMMU_HEAP_ID;
  if (cached_) {
    alloc.flags = ION_FLAG_CACHED;
  }

  int32_t rc = ioctl(ion_fd_, ION_IOC_ALLOC, &alloc);
  if (rc < 0) {
    Utils::ThrowException(__func__, "ION alloc length %d %zu failed");
  }

  struct ion_fd_data ion_info_fd;
  memset(&ion_info_fd, 0, sizeof(ion_info_fd));
  ion_info_fd.handle = alloc.handle;
  handle_ = alloc.handle;

  rc = ioctl(ion_fd_, ION_IOC_SHARE, &ion_info_fd);
  if (rc < 0) {
    Utils::ThrowException(__func__, "ION map call failed");
  }

  fd_ = ion_info_fd.fd;
  addr_ = static_cast<uint8_t*>(
      mmap(NULL, alloc.len, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0));
  if (addr_ == MAP_FAILED) {
    Utils::ThrowException(__func__, "mmap call failed");
  }
}

/** PlatformBuffer
 *
 * Destructs PlatformBuffer
 *
 * return: void
 **/
PlatformBuffer::~PlatformBuffer() {
  if (MAP_FAILED != addr_) {
    munmap(addr_, size_);
  }

  if (-1 != fd_) {
    close(fd_);
  }

  if (-1 != handle_) {
    struct ion_handle_data handle_data;
    memset(&handle_data, 0, sizeof(handle_data));
    handle_data.handle = handle_;
    ioctl(ion_fd_, ION_IOC_FREE, &handle_data);
  }

  if (ion_fd_ >= 0) {
    close(ion_fd_);
  }
}

/** New
 *    @size: buffer size
 *    @cached: flag indicating whether buffer is cached
 *
 * creates new instance of PlatformBuffer
 *
 * return: shared pointer of PlatformBuffer
 **/
std::shared_ptr<PlatformBuffer> PlatformBuffer::New(uint32_t size,
                                                    bool cached) {
  std::shared_ptr<PlatformBuffer> new_handler(new PlatformBuffer(size, cached));
  return new_handler;
}

/** GetAddr
 *
 * returns addres
 *
 * return: address
 **/
uint8_t* PlatformBuffer::GetAddr() const { return addr_; }

/** GetFd
 *
 * returns fd
 *
 * return: fd
 **/
int32_t PlatformBuffer::GetFd() const { return fd_; }

/** CacheFlush
 *
 * flushes cache
 *
 * return: void
 **/
void PlatformBuffer::CacheFlush() const { Cache(ION_IOC_CLEAN_INV_CACHES); };

/** CacheInvalidate
 *
 * flushes cache
 *
 * return: void
 **/
void PlatformBuffer::CacheInalidate() const { Cache(ION_IOC_INV_CACHES); };

/** Cache
 *    @cmd: cache cmd
 *
 * aplies cache cmd
 *
 * return: void
 **/
void PlatformBuffer::Cache(uint32_t cmd) const {
  if (cached_) {
    struct ion_flush_data cache_inv_data {};
    struct ion_custom_data custom_data {};

    cache_inv_data.vaddr = addr_;
    cache_inv_data.fd = fd_;
    cache_inv_data.handle = handle_;
    cache_inv_data.length = size_;
    custom_data.cmd = cmd;
    custom_data.arg = (unsigned long)&cache_inv_data;

    if (ioctl(ion_fd_, ION_IOC_CUSTOM, &custom_data) < 0) {
      Utils::ThrowException(__func__, std::string("Cache cmd ") +
                                          std::to_string(cmd) + " failed");
    }
  }
}

};  // namespace qmmf_alg_plugin
};  // namespace qmmf
