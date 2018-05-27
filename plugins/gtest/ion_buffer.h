/*
* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
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

#include <memory>

namespace qmmf {
namespace qmmf_alg_plugin {

/** PlatformBuffer:
 *    @addr_: pointer to allocated buffer
 *    @size_: buffer size
 *    @fd_: fd
 *    @handle_: handle
 *    @ion_fd_: ion driver fd
 *    @cached_: flag indicating whether buffer is cached
 *
 *  This class implements ion platform buffer
 **/
class PlatformBuffer {
 private:
  PlatformBuffer(uint32_t size, bool cached);

 public:
  ~PlatformBuffer();

  /** New
    *    @size: buffer size
    *    @cached: flag indicating whether buffer is cached
    *
    * creates new instance of PlatformBuffer
    *
    * return: shared pointer of PlatformBuffer
    **/
  static std::shared_ptr<PlatformBuffer> New(uint32_t size, bool cached);

  /** GetAddr
    *
    * returns addres
    *
    * return: address
    **/
  uint8_t* GetAddr() const;

  /** GetFd
    *
    * returns fd
    *
    * return: fd
    **/
  int32_t GetFd() const;

  /** CacheFlush
    *
    * flushes cache
    *
    * return: void
    **/
  void CacheFlush() const;

  /** CacheInvalidate
    *
    * flushes cache
    *
    * return: void
    **/
  void CacheInalidate() const;

 private:
  /** Cache
    *    @cmd: cache cmd
    *
    * aplies cache cmd
    *
    * return: void
    **/
  void Cache(uint32_t cmd) const;

  uint8_t* addr_;
  uint32_t size_;
  int32_t fd_;
  int32_t handle_;
  int32_t ion_fd_;
  bool cached_;
};

};  // namespace qmmf_alg_plugin
};  // namespace qmmf
