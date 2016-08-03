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

#pragma once

#include <utils/List.h>
#include <utils/Mutex.h>
#include <utils/Condition.h>
#include <utils/Log.h>
#include <system/graphics.h>
#include <system/window.h>

#include "common/qmmf_log.h"

namespace qmmf {

using namespace android;

#define MAX_PLANE 3

typedef struct {
  uint32_t stride;
  uint32_t scanline;
  uint32_t width;
  uint32_t height;
} PlaneInfo;

enum class BufferFormat {
  kNV12,
  kNV21,
  kBLOB,
  kRAW10,
  kRAW16
};

typedef struct {
  BufferFormat format;
  uint32_t num_planes;
  PlaneInfo plane_info[MAX_PLANE];
} MetaInfo;

typedef struct {
  MetaInfo info;
  int64_t timestamp;
  int64_t frame_number;
  android_dataspace data_space;
  buffer_handle_t handle;
  int32_t fd;
  uint32_t size;
} StreamBuffer;

// Thread safe Queue
template <class T>
class TSQueue
{
 public:
  typedef typename List<T>::iterator iterator;

  iterator Begin() {
    Mutex::Autolock autoLock(lock_);
    return queue_.begin();
  }

  void PushBack(const T& item) {
    Mutex::Autolock autoLock(lock_);
    queue_.push_back(item);
  }

  int32_t Size() {
    Mutex::Autolock autoLock(lock_);
    return queue_.size();
  }

  bool Empty() {
   Mutex::Autolock autoLock(lock_);
   return queue_.empty();
  }

  iterator End() {
    Mutex::Autolock autoLock(lock_);
    return queue_.end();
  }

  void Erase(iterator it) {
    Mutex::Autolock autoLock(lock_);
    queue_.erase(it);
  }

  void Clear() {
    Mutex::Autolock autoLock(lock_);
    queue_.clear();
  }

 private:
  List<T> queue_;
  Mutex lock_;
};

const nsecs_t kWaitDelay = 500000000; // 0.5s

//TODO: make generic signal queue
template <class T>
class SignalQueue
{
public:

  SignalQueue(uint32_t size):cmd_queue_size(size)
  {
      QMMF_INFO("%s: Enter",__func__);
      QMMF_INFO("%s: Exit",__func__);
  }

  ~SignalQueue()
  {

      QMMF_INFO("%s: Enter",__func__);
      cmd_queue_size = -1;
      QMMF_INFO("%s: Exit",__func__);
  }

  T Pop()
  {
      void* item = NULL;
      status_t ret = 0;
      uint32_t size;

      {
        Mutex::Autolock l(cmd_queue_mutex_);
        size = cmd_queue_.size();
      }
      if(size == 0) {
        // wait for signal or for data to come into queue
        Mutex::Autolock l(lock_);
        while (size == 0) {
            ret = wait_for_cmd_.waitRelative(lock_, kWaitDelay);
            if (TIMED_OUT == ret) {
                QMMF_WARN("%s: Wait for cmd.. timed out", __func__);
                {
                  Mutex::Autolock l(cmd_queue_mutex_);
                  size = cmd_queue_.size();
                }
                continue;
            } else
                break;
        }
      }

      if (ret == 0) {
        {
          Mutex::Autolock l(cmd_queue_mutex_);
          item = *cmd_queue_.begin();
          cmd_queue_.erase(cmd_queue_.begin());
        }
      }

      return item;
  }

  status_t Push(void* item)
  {

    uint32_t size;
    {
      Mutex::Autolock l(cmd_queue_mutex_);
      size = cmd_queue_.size();
    }
    if(cmd_queue_size < size) {
      QMMF_ERROR("%s: command queue size full", __func__);
      return -1;
    }
    {
      Mutex::Autolock l(cmd_queue_mutex_);
      cmd_queue_.push_back(item);

    }

    Mutex::Autolock autoLock(lock_);
    wait_for_cmd_.signal();
    return 0;
  }

  void Clear()
  {
    QMMF_INFO("%s: Enter",__func__);
    Mutex::Autolock l(cmd_queue_mutex_);
    cmd_queue_.clear();
    QMMF_INFO("%s: Exit",__func__);
  }

private:
    Mutex      lock_;
    Condition  wait_for_cmd_;
    Mutex      cmd_queue_mutex_;
    List<T>    cmd_queue_;
    uint32_t   cmd_queue_size;
}; //SignalQueue

}; //namespace qmmf.
