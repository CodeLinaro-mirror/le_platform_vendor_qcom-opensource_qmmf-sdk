/*
* Copyright (c) 2018, The Linux Foundation. All rights reserved.
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

#include <pthread.h>
#include <stdbool.h>

namespace qmmf {

namespace neonresizer {

typedef struct resn_prev_s {
  unsigned int src_width;
  unsigned int src_height;
  unsigned int dst_width;
  unsigned int dst_height;
} resn_prev_frame_data_t;

typedef struct {
  pthread_t thread;
  uint32_t tid;
  uint32_t width;
  uint32_t height;
  uint32_t stride;
  uint32_t src_stride;
  uint32_t line_start;
  uint32_t line_end;
  uint32_t w_coef;
  uint32_t h_coef;
  uint8_t* src_luma;
  uint8_t* src_chroma;
  uint8_t* dst_luma;
  uint8_t* dst_chroma;
  uint16_t* y_coefs;
  uint16_t* uv_coefs;
  uint16_t* input_offsets;

  pthread_mutex_t lock;
  pthread_cond_t signal_thread;
  pthread_cond_t signal_base;
  bool thread_started;
  bool thread_ready;
  bool thread_active;
} neon_thrd_args;

typedef struct resn_cnt_s {
  // Input buffer dimensions
  unsigned int src_width;
  unsigned int src_height;

  // Output buffer dimensions
  unsigned int dst_width;
  unsigned int dst_height;

  // Internal buffers
  // Y interpolation coefficients size of dst_width
  uint16_t* y_coefs;
  // UV interpolation coefficients size of dst_width
  uint16_t* uv_coefs;
  // Horizontal input buffer offsets
  uint16_t* input_offsets;

  // Previous Input/output buffers dimensions
  resn_prev_frame_data_t prev_data;
  // Threads data
  uint32_t num_threads;
  neon_thrd_args* neon_work_args;
} resn_cnt_t;

} // namespace neonresizer
} // namespace qmmf
