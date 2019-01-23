/*
 * Copyright (c) 2018, 2019, The Linux Foundation. All rights reserved.
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

#pragma once

#include <sstream>
#include <string>

#include <cutils/properties.h>
#include <linux/msm_ion.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <utils/Log.h>

#if TARGET_ION_ABI_VERSION >= 2
#include <ion/ion.h>
#include <linux/dma-buf.h>
#else
#include <fcntl.h>
#endif

#include <qmmf-alg/qmmf_alg_plugin.h>
#include <qmmf-alg/qmmf_alg_utils.h>

namespace qmmf {

/** SyncStart
 *    @fd: ion fd
 *
 * Start CPU Access
 *
 **/
inline void SyncStart(int32_t fd) {
  ALOGV("%s: Enter", __func__);
#if TARGET_ION_ABI_VERSION >= 2
  struct dma_buf_sync buf_sync;
  buf_sync.flags = DMA_BUF_SYNC_START | DMA_BUF_SYNC_RW;

  auto result = ioctl(fd, DMA_BUF_IOCTL_SYNC, &buf_sync);
  if (result) ALOGE("%s: Failed first DMA_BUF_IOCTL_SYNC start", __func__);
#endif
  ALOGV("%s: Exit", __func__);
}

/** SyncEnd
 *    @fd: ion fd
 *
 * End CPU Access
 *
 **/
inline void SyncEnd(int32_t fd) {
  ALOGV("%s: Enter", __func__);
#if TARGET_ION_ABI_VERSION >= 2
  struct dma_buf_sync buf_sync;
  buf_sync.flags = DMA_BUF_SYNC_END | DMA_BUF_SYNC_RW;

  auto result = ioctl(fd, DMA_BUF_IOCTL_SYNC, &buf_sync);
  if (result) ALOGE("%s: Failed first DMA_BUF_IOCTL_SYNC End", __func__);
#endif
  ALOGV("%s: Exit", __func__);
}

/** Property:
 *
 *  This class defines property operations
 **/
class Property {
 public:
  /** Get
   *    @property: property
   *    @default_value: default value
   *
   * Gets requested property value
   *
   * return: property value
   **/
  template <typename TProperty>
  static TProperty Get(std::string property, TProperty default_value) {
    TProperty value = default_value;
    char prop_val[PROPERTY_VALUE_MAX];
    std::stringstream s;
    s << default_value;
    property_get(property.c_str(), prop_val, s.str().c_str());

    std::stringstream output(prop_val);
    output >> value;
    return value;
  }

  /** Set
   *    @property: property
   *    @value: value
   *
   * Sets requested property value
   *
   * return: nothing
   **/
  template <typename TProperty>
  static void Set(std::string property, TProperty value) {
    std::stringstream s;
    s << value;
    std::string value_string = s.str();
    value_string.resize(PROPERTY_VALUE_MAX);
    property_set(property.c_str(), value_string.c_str());
  }
};

#if TARGET_ION_ABI_VERSION >= 2
/** BufferHolder
 *
 * Buffer Holder
 *
 **/
class BufferHolder : public qmmf_alg_plugin::IBufferHolder {
 private:
  BufferHolder(const uint8_t *vaddr, const int32_t fd, const uint32_t size,
               const bool cached)
      : vaddr_(const_cast<uint8_t *>(vaddr)),
        fd_(fd),
        size_(size),
        cache_manipulations_(cached && (fd >= 0)),
        imported_(true),
        ion_device_(-1) {}

  BufferHolder(const uint32_t size, const bool cached)
      : vaddr_(nullptr),
        fd_(-1),
        size_(size),
        cache_manipulations_(cached),
        imported_(false),

        ion_device_(-1) {
    ion_device_ = ion_open();
    if (ion_device_ < 0) {
      qmmf_alg_plugin::Utils::ThrowException(__func__,
                                             "Open ion device failed");
    }

    uint32_t flags = 0;
    uint32_t heap_id_mask = ION_HEAP(ION_SYSTEM_HEAP_ID);
    if (cached) {
      flags = ION_FLAG_CACHED;
    }

    int32_t rc = ion_alloc_fd(ion_device_, size, 0, heap_id_mask, flags, &fd_);
    if (rc) {
      std::stringstream s;
      s << "ION alloc failed rc " << rc;
      qmmf_alg_plugin::Utils::ThrowException(__func__, s.str());
    }

    vaddr_ = static_cast<uint8_t *>(
        mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0));
    if (vaddr_ == MAP_FAILED) {
      qmmf_alg_plugin::Utils::ThrowException(__func__, "mmap call failed");
    }
  }

 public:
  ~BufferHolder() {
    if (!imported_) {
      if (MAP_FAILED != vaddr_) {
        munmap(vaddr_, size_);
      }

      if (-1 != fd_) {
        close(fd_);
      }

      if (ion_device_ >= 0) {
        ion_close(ion_device_);
      }
    }
  }

  static std::shared_ptr<BufferHolder> New(const uint8_t *vaddr,
                                           const int32_t fd,
                                           const uint32_t size,
                                           const bool cached) {
    std::shared_ptr<BufferHolder> new_holder(
        new BufferHolder(vaddr, fd, size, cached));
    return new_holder;
  }

  static std::shared_ptr<BufferHolder> New(const uint32_t size,
                                           const bool cached) {
    std::shared_ptr<BufferHolder> new_holder(new BufferHolder(size, cached));
    return new_holder;
  }

  void CpuAccessStart() const {
    DmaBufCommand(DMA_BUF_SYNC_END | DMA_BUF_SYNC_RW);
  }

  void CpuAccessEnd() const {
    DmaBufCommand(DMA_BUF_SYNC_START | DMA_BUF_SYNC_RW);
  }

  const uint8_t *GetAddr() const { return const_cast<const uint8_t *>(vaddr_); }

  int32_t GetFd() const { return fd_; }

  uint32_t GetSize() const { return size_; }

 private:
  void DmaBufCommand(uint32_t cmd) const {
    if (cache_manipulations_) {
      struct dma_buf_sync buf_sync {};
      buf_sync.flags = cmd;
      if (ioctl(fd_, DMA_BUF_IOCTL_SYNC, &buf_sync) < 0) {
        qmmf_alg_plugin::Utils::ThrowException(
            __func__,
            std::string("CacheCommand cmd ") + std::to_string(cmd) + " failed");
      }
    }
  }

 private:
  uint8_t *vaddr_;
  int32_t fd_;
  const uint32_t size_;
  const bool cache_manipulations_;
  const bool imported_;
  int32_t ion_device_;
};
#else
/** BufferHolder
 *
 * Buffer Holder
 *
 **/
class BufferHolder : public qmmf_alg_plugin::IBufferHolder {
 private:
  BufferHolder(const uint32_t size, const bool cached)
      : vaddr_(static_cast<uint8_t *>(MAP_FAILED)),
        fd_(-1),
        size_(size),
        cache_manipulations_(cached),
        imported_(false),
        handle_(-1),
        ion_device_(-1) {
    ion_device_ = open("/dev/ion", O_RDONLY);
    if (ion_device_ < 0) {
      qmmf_alg_plugin::Utils::ThrowException(__func__,
                                             "Open ion device failed");
    }

    struct ion_allocation_data alloc {};
    alloc.len = size;
    alloc.align = 0;
    alloc.heap_id_mask = 0x1 << ION_IOMMU_HEAP_ID;
    if (cached) {
      alloc.flags = ION_FLAG_CACHED;
    }

    int32_t rc = ioctl(ion_device_, ION_IOC_ALLOC, &alloc);
    if (rc < 0) {
      qmmf_alg_plugin::Utils::ThrowException(__func__, "ION alloc failed");
    }

    struct ion_fd_data ion_info_fd {};
    ion_info_fd.handle = alloc.handle;
    handle_ = alloc.handle;

    rc = ioctl(ion_device_, ION_IOC_SHARE, &ion_info_fd);
    if (rc < 0) {
      qmmf_alg_plugin::Utils::ThrowException(__func__, "ION map call failed");
    }

    fd_ = ion_info_fd.fd;
    vaddr_ = static_cast<uint8_t *>(
        mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0));
    if (vaddr_ == MAP_FAILED) {
      qmmf_alg_plugin::Utils::ThrowException(__func__, "mmap call failed");
    }
  }

  BufferHolder(const uint8_t *vaddr, const int32_t fd, const uint32_t size,
               const bool cached)
      : vaddr_(const_cast<uint8_t *>(vaddr)),
        fd_(fd),
        size_(size),
        cache_manipulations_(cached && (fd >= 0)),
        imported_(true),
        handle_(-1),
        ion_device_(-1) {
    if (cache_manipulations_) {
      ion_device_ = open("/dev/ion", O_RDONLY);
      if (ion_device_ < 0) {
        qmmf_alg_plugin::Utils::ThrowException(__func__,
                                               "Open ion device failed");
      }

      struct ion_fd_data share_data {};
      share_data.handle = 0;
      share_data.fd = fd_;

      auto res = ioctl(ion_device_, ION_IOC_IMPORT, &share_data);
      if (res < 0) {
        std::stringstream s;
        s << "ION_IOC_IMPORT ioctl command failed: %d[%s]"
          << std::to_string(res);
        qmmf_alg_plugin::Utils::ThrowException(__func__, s.str());
      }
      handle_ = share_data.handle;
    }
  }

 public:
  ~BufferHolder() {
    if (!imported_ && (MAP_FAILED != vaddr_)) {
      munmap(vaddr_, size_);
    }

    if (-1 != handle_) {
      struct ion_fd_data share_data {};
      share_data.handle = handle_;
      ioctl(ion_device_, ION_IOC_FREE, &share_data);
    }

    if (!imported_ && (-1 != fd_)) {
      close(fd_);
    }

    if (ion_device_ >= 0) {
      close(ion_device_);
    }
  }

  static std::shared_ptr<BufferHolder> New(const uint8_t *vaddr,
                                           const int32_t fd,
                                           const uint32_t size,
                                           const bool cached) {
    std::shared_ptr<BufferHolder> new_holder(
        new BufferHolder(vaddr, fd, size, cached));
    return new_holder;
  }

  static std::shared_ptr<BufferHolder> New(const uint32_t size,
                                           const bool cached) {
    std::shared_ptr<BufferHolder> new_holder(new BufferHolder(size, cached));
    return new_holder;
  }

  void CpuAccessStart() const { CacheCommand(ION_IOC_INV_CACHES); }

  void CpuAccessEnd() const { CacheCommand(ION_IOC_CLEAN_INV_CACHES); }

  const uint8_t *GetAddr() const { return const_cast<const uint8_t *>(vaddr_); }

  int32_t GetFd() const { return fd_; }

  uint32_t GetSize() const { return size_; }

 private:
  void CacheCommand(uint32_t cmd) const {
    if (cache_manipulations_) {
      struct ion_flush_data cache_invalidate {};
      struct ion_custom_data cache_data {};

      cache_invalidate.vaddr = const_cast<uint8_t *>(vaddr_);
      cache_invalidate.fd = fd_;
      cache_invalidate.handle = handle_;
      cache_invalidate.length = size_;
      cache_data.cmd = cmd;
      cache_data.arg = (unsigned long)&cache_invalidate;

      if (ioctl(ion_device_, ION_IOC_CUSTOM, &cache_data) < 0) {
        qmmf_alg_plugin::Utils::ThrowException(
            __func__,
            std::string("CacheCommand cmd ") + std::to_string(cmd) + " failed");
      }
    }
  }

 private:
  uint8_t *vaddr_;
  int32_t fd_;
  const uint32_t size_;
  const bool cache_manipulations_;
  const bool imported_;
  int32_t handle_;
  int32_t ion_device_;
};
#endif

/** QmmfAlgoTools
 *
 * Qmmf Algo tools implementation
 *
 **/
class QmmfAlgoTools : public qmmf_alg_plugin::ITools {
 public:
  ~QmmfAlgoTools(){};

  void SetProperty(std::string property, std::string value) {
    Property::Set(property, value);
  }

  void SetProperty(std::string property, int32_t value) {
    Property::Set(property, value);
  }

  const std::string GetProperty(std::string property,
                                std::string default_value) {
    return Property::Get(property, default_value);
  }

  uint32_t GetProperty(std::string property, int32_t default_value) {
    return Property::Get(property, default_value);
  }

  void LogError(const std::string &s) { ALOGE("%s", s.c_str()); }

  void LogWarning(const std::string &s) { ALOGW("%s", s.c_str()); }

  void LogInfo(const std::string &s) { ALOGI("%s", s.c_str()); }

  void LogDebug(const std::string &s) { ALOGD("%s", s.c_str()); }

  void LogVerbose(const std::string &s) { ALOGV("%s", s.c_str()); }

  std::shared_ptr<qmmf_alg_plugin::IBufferHolder> ImportBufferHolder(
      const uint8_t *vaddr, const int32_t fd, const uint32_t size,
      const bool cached) {
    return BufferHolder::New(vaddr, fd, size, cached);
  }

  std::shared_ptr<qmmf_alg_plugin::IBufferHolder> NewBufferHolder(
      const uint32_t size, const bool cached) {
    return BufferHolder::New(size, cached);
  }
};

}  // namespace qmmf
