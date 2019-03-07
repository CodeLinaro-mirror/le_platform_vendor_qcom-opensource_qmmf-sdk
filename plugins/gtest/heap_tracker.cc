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

#define LOG_TAG "heap_tracker"

#include <dlfcn.h>
#include <cstddef>
#include <cstring>
#include <mutex>
#include <sstream>
#include <stdexcept>

#include <utils/Log.h>

#include "heap_tracker.h"

#define LOCAL_ALLIGN 7ul
typedef void *(*malloc_fn)(size_t size);
typedef void (*free_fn)(void *ptr);
typedef void *(*realloc_fn)(void *ptr, size_t size);

static std::mutex lock_;

static malloc_fn sys_malloc_ = nullptr;
static free_fn sys_free_ = nullptr;
static realloc_fn sys_realloc_ = nullptr;

// 640K ought to be enough for anybody
#define LOCAL_HEAP_SIZE (640 * 1024)
static uint8_t local_heap_[LOCAL_HEAP_SIZE];
static uint32_t local_heap_top_ = 0;
static uint32_t total_allocations_ = 0;

/** heap_tracker_deinit
 *
 * Deinitialize heap tracker.  Do NOT invoke it directly. Let deinit hook to
 *   invoke it
 *
 * return: void
 **/
extern "C" void heap_tracker_deinit() {
  std::unique_lock<std::mutex> l(lock_);
  sys_realloc_ = nullptr;
  sys_malloc_ = nullptr;
  sys_free_ = nullptr;
}

/** heap_tracker_init
 *
 * Initialize heap tracker. Do NOT invoke it directly. Let init hook to
 *   invoke it
 *
 * return: void
 **/
extern "C" void heap_tracker_init(void) {
  dlerror();

  auto f = (free_fn)dlsym(RTLD_NEXT, "free");
  auto err = dlerror();
  if (err != nullptr) {
    std::stringstream s;
    s << "Loading system symbol for free failed with " << err;
    throw std::runtime_error(s.str());
  }

  auto m = (malloc_fn)dlsym(RTLD_NEXT, "malloc");
  err = dlerror();
  if (err != nullptr) {
    std::stringstream s;
    s << "Loading system symbols for malloc failed with " << err;
    throw std::runtime_error(s.str());
  }

  auto r = (realloc_fn)dlsym(RTLD_NEXT, "realloc");
  err = dlerror();
  if (err != nullptr) {
    std::stringstream s;
    s << "Loading system symbol for realloc failed with " << err;
    throw std::runtime_error(s.str());
  }

  std::unique_lock<std::mutex> l(lock_);
  sys_realloc_ = r;
  sys_malloc_ = m;
  sys_free_ = f;
}

/** malloc_local_memory
 *    @size: size
 *
 * Allocates requested memory block from local "heap". MUST be private for this
 *   file and MUST be protected with mutex from invoking routine
 *
 * return: new pointer
 **/
static void *malloc_local_memory(size_t size) {
  void *rv = nullptr;

  if (!local_heap_top_) {
    local_heap_top_ =
        LOCAL_ALLIGN + 1 -
        (size_t)(&local_heap_[local_heap_top_]) % (LOCAL_ALLIGN + 1);
  }

  size += LOCAL_ALLIGN;
  size &= ~LOCAL_ALLIGN;
  if (local_heap_top_ + size > LOCAL_HEAP_SIZE) {
    std::stringstream s;
    s << "Local heap size too small " << LOCAL_HEAP_SIZE
      << " bytes to allocate new " << size << " bytes";
    throw std::runtime_error(s.str());
  }
  rv = &local_heap_[local_heap_top_];
  local_heap_top_ += size;

  return rv;
}

/** malloc
 *    @size: size
 *
 * Allocates requested memory block
 *
 * return: new pointer
 **/
extern "C" void *malloc(size_t size) {
  void *rv = nullptr;

  if (size == 0) {
    return rv;
  }

  std::unique_lock<std::mutex> l(lock_);

  if (sys_malloc_) {
    rv = (*sys_malloc_)(size);
    total_allocations_++;
  } else {
    rv = malloc_local_memory(size);
  }

  return rv;
}

/** calloc
 *    @num: number of blocks with requested size
 *    @size: size
 *
 * Allocates requested memory block which is already set to 0
 *
 * return: new pointer
 **/
extern "C" void *calloc(size_t num, size_t size) {
  size *= num;

  if (!size) {
    return nullptr;
  }

  auto rv = malloc(size);
  std::memset(rv, 0, size);

  return rv;
}

/** free
 *    @ptr: pointer
 *
 * Releases requested memory block
 *
 * return: void
 **/
extern "C" void free(void *ptr) {
  if (!ptr) {
    return;
  }

  if (sys_free_ &&
      ((ptr < local_heap_) || (ptr > &local_heap_[local_heap_top_]))) {
    (*sys_free_)(ptr);
    std::unique_lock<std::mutex> l(lock_);
    total_allocations_--;
  }
}

/** realloc
 *    @ptr: old pointer
 *    @size: new size
 *
 * Reallocates requested memory block
 *
 * return: new pointer
 **/
extern "C" void *realloc(void *ptr, size_t size) {
  std::unique_lock<std::mutex> l(lock_);

  void *rv = nullptr;

  if (sys_realloc_ &&
      ((ptr < local_heap_) || (ptr > &local_heap_[local_heap_top_]))) {
    rv = (*sys_realloc_)(ptr, size);
    if (!ptr && rv) {
      total_allocations_++;
    } else if (ptr && !rv) {
      total_allocations_--;
    }
  } else {
    if (!size) {
      // Local "heap" doesn;t need to be freed
      return rv;
    }
    rv = malloc_local_memory(size);
    if (ptr) {
      std::memcpy(rv, ptr, size);
    }
  }

  return rv;
}

/** heap_tracker_get_total_allocations
 *
 * Returns number of total allocations
 *
 * return: number of total allocations
 **/
extern "C" uint32_t heap_tracker_get_total_allocations() {
  std::unique_lock<std::mutex> l(lock_);
  return total_allocations_;
}
