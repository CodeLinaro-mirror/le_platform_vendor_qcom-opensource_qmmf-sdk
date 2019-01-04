/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
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
#include <sys/ioctl.h>
#include <utils/Log.h>

#if TARGET_ION_ABI_VERSION >= 2
#include <linux/dma-buf.h>
#else
#include <fcntl.h>
#include <linux/msm_ion.h>
#endif

#include <qmmf-alg/qmmf_alg_plugin.h>
#include <qmmf-alg/qmmf_alg_utils.h>

namespace qmmf {

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
/** CacheHandler
 *
 * Cache Handler
 *
 **/
class CacheHandler : public qmmf_alg_plugin::ICacheHandler {
 private:
  CacheHandler(const qmmf_alg_plugin::AlgBuffer &alg_buffer)
      : alg_buffer_(alg_buffer),
        active_(alg_buffer.cached_ && (alg_buffer.fd_ >= 0)) {}

 public:
  ~CacheHandler() {}

  static std::shared_ptr<CacheHandler> New(
      const qmmf_alg_plugin::AlgBuffer &alg_buffer) {
    std::shared_ptr<CacheHandler> new_handler(new CacheHandler(alg_buffer));
    return new_handler;
  }

  void CpuAccessStart() { DmaBufCommand(DMA_BUF_SYNC_END | DMA_BUF_SYNC_RW); }

  void CpuAccessEnd() { DmaBufCommand(DMA_BUF_SYNC_START | DMA_BUF_SYNC_RW); }

 private:
  void DmaBufCommand(uint32_t cmd) const {
    if (active_) {
      struct dma_buf_sync buf_sync {};
      buf_sync.flags = cmd;
      if (ioctl(alg_buffer_.fd_, DMA_BUF_IOCTL_SYNC, &buf_sync) < 0) {
        qmmf_alg_plugin::Utils::ThrowException(
            __func__,
            std::string("CacheCommand cmd ") + std::to_string(cmd) + " failed");
      }
    }
  }

 private:
  const qmmf_alg_plugin::AlgBuffer &alg_buffer_;
  const bool active_;
};
#else
/** CacheHandler
 *
 * Cache Handler
 *
 **/
class CacheHandler : public qmmf_alg_plugin::ICacheHandler {
 private:
  CacheHandler(const qmmf_alg_plugin::AlgBuffer &alg_buffer)
      : alg_buffer_(alg_buffer),
        active_(alg_buffer.cached_ && (alg_buffer.fd_ >= 0)) {
    if (active_) {
      ion_device_ = open("/dev/ion", O_RDONLY);
      if (ion_device_ < 0) {
        qmmf_alg_plugin::Utils::ThrowException(__func__,
                                               "Open ion device failed");
      }

      struct ion_fd_data share_data {};
      share_data.handle = 0;
      share_data.fd = alg_buffer_.fd_;

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
  ~CacheHandler() {
    if (active_) {
      struct ion_fd_data share_data {};
      share_data.handle = handle_;
      share_data.fd = 0;

      if (ion_device_ >= 0 && handle_) {
        ioctl(ion_device_, ION_IOC_FREE, &share_data);
        close(ion_device_);
      }
    }
  }

  static std::shared_ptr<CacheHandler> New(
      const qmmf_alg_plugin::AlgBuffer &alg_buffer) {
    std::shared_ptr<CacheHandler> new_handler(new CacheHandler(alg_buffer));
    return new_handler;
  }

  void CpuAccessStart() { CacheCommand(ION_IOC_INV_CACHES); }

  void CpuAccessEnd() { CacheCommand(ION_IOC_CLEAN_INV_CACHES); }

 private:
  void CacheCommand(uint32_t cmd) const {
    if (active_) {
      struct ion_flush_data cache_invalidate {};
      struct ion_custom_data cache_data {};

      cache_invalidate.vaddr = alg_buffer_.vaddr_;
      cache_invalidate.fd = alg_buffer_.fd_;
      cache_invalidate.handle = handle_;
      cache_invalidate.length = alg_buffer_.size_;
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
  const qmmf_alg_plugin::AlgBuffer &alg_buffer_;
  const bool active_;
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

  std::shared_ptr<qmmf_alg_plugin::ICacheHandler> NewCacheHandler(
      const qmmf_alg_plugin::AlgBuffer &alg_buffer) {
    return CacheHandler::New(alg_buffer);
  }
};

}  // namespace qmmf
