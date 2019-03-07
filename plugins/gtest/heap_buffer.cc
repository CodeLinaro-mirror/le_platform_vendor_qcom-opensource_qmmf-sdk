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

#define LOG_TAG "HeapBuffer"

#include <cstdlib>

#include "heap_buffer.h"

namespace qmmf {
namespace qmmf_alg_plugin {

/** HeapBuffer
 *    @size: size of the requested buffer
 *
 * Constructs HeapBuffer
 *
 * return: void
 **/
HeapBuffer::HeapBuffer(uint32_t size)
    : data_(size), fd_(-1 * std::abs(reinterpret_cast<int32_t>(this))) {}

/** New
 *    @size: buffer size
 *
 * creates new instance of HeapBuffer
 *
 * return: shared pointer of HeapBuffer
 **/
std::shared_ptr<HeapBuffer> HeapBuffer::New(uint32_t size) {
  std::shared_ptr<HeapBuffer> new_handler(new HeapBuffer(size));
  return new_handler;
}

/** CpuAccessStart
 *
 * Start of CPU access
 *
 * return: nothing
 **/
void HeapBuffer::CpuAccessStart() const {}

/** CpuAccessEnd
 *
 * End of CPU access
 *
 * return: nothing
 **/
void HeapBuffer::CpuAccessEnd() const {}

/** GetAddr
 *
 * returns address
 *
 * return: address
 **/
const uint8_t* HeapBuffer::GetAddr() const { return data_.data(); }

/** GetFd
 *
 * returns fd
 *
 * return: fd
 **/
int32_t HeapBuffer::GetFd() const { return fd_; }

/** GetSize
 *
 * returns size
 *
 * return: size
 **/
uint32_t HeapBuffer::GetSize() const { return data_.size(); }

};  // namespace qmmf_alg_plugin
};  // namespace qmmf
