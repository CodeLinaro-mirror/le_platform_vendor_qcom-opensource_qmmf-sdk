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

#define LOG_TAG "res_neon"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <time.h>

#include <utils/Log.h>

#include "res_neon.h"
#include "res_neon_prv.h"

#define _ARM_NEON_SUPPORT_

namespace qmmf {

namespace neonresizer {

static void* neon_thread_y_pass(void* arg);
static void* neon_thread_uv_pass(void* arg);

static char version[] = {
#include "lib_version.txt"
};

/** resn_get_version
 *
 * Returns lib version
 *
 * return: lib version
 **/
char* resn_get_version() { return version; }

#ifdef _ARM_NEON_SUPPORT_

/** LumaProcessBilinearVSkikNEON
 *
 * @n_thrd_arg: struct with input thread parameters
 *
 * Resize Luma Process function NEON
 *
 * return:
 **/
static void LumaProcessBilinearVSkipNEON(neon_thrd_args* n_thrd_arg) {
  uint32_t row, col;
  uint8_t *src, *src0, *src1, *src2, *src3, *src4, *src5, *src6, *src7;
  uint16_t* pcoef;
  uint32_t h_ind;
  uint16_t h_rem;
  uint16_t one_val = 256;

  uint8_t* src_luma = n_thrd_arg->src_luma;
  uint8_t* dst =
      n_thrd_arg->dst_luma + n_thrd_arg->line_start * n_thrd_arg->stride;
  uint16_t* offsets = n_thrd_arg->input_offsets;
  uint16_t* coefs = n_thrd_arg->y_coefs;
  uint32_t h_coef = n_thrd_arg->h_coef;
  uint32_t line_start = n_thrd_arg->line_start;
  uint32_t line_end = n_thrd_arg->line_end;
  uint32_t width = n_thrd_arg->width;
  uint32_t stride = n_thrd_arg->stride;
  uint32_t src_stride = n_thrd_arg->src_stride;

  for (row = line_start; row < line_end; row++) {
    h_ind = row * h_coef;
    h_rem = h_ind - ((h_ind >> 8) << 8);
    h_ind = ((h_ind - h_rem) >> 8);
    src = src_luma + h_ind * src_stride;
    for (col = 0; col < (width / 8); col++) {
      src0 = src + offsets[col * 8];
      src1 = src + offsets[col * 8 + 1];
      src2 = src + offsets[col * 8 + 2];
      src3 = src + offsets[col * 8 + 3];
      src4 = src + offsets[col * 8 + 4];
      src5 = src + offsets[col * 8 + 5];
      src6 = src + offsets[col * 8 + 6];
      src7 = src + offsets[col * 8 + 7];
      pcoef = coefs + col * 8;

      asm volatile(
          "vld1.8     {d0}, [%0]                     \n"  // load up 4
          "vld1.8     {d1}, [%1]                     \n"
          "vld1.8     {d2}, [%2]                     \n"
          "vld1.8     {d3}, [%3]                     \n"
          "vld1.8     {d4}, [%4]                     \n"
          "vld1.8     {d5}, [%5]                     \n"
          "vld1.8     {d6}, [%6]                     \n"
          "vld1.8     {d7}, [%7]                     \n"
          "vld1.16    {q6}, [%9]                     \n"
          "vext.8 d8, d0, d0, #2                     \n"
          "vext.8 d8, d8, d1, #2                     \n"
          "vext.8 d8, d8, d2, #2                     \n"
          "vext.8 d8, d8, d3, #2                     \n"
          "vext.8 d9, d4, d4, #2                     \n"
          "vext.8 d9, d9, d5, #2                     \n"
          "vext.8 d9, d9, d6, #2                     \n"
          "vext.8 d9, d9, d7, #2                     \n"
          "vuzp.u8 q4, q5                            \n"
          "vmovl.u8 q0, d8                           \n"
          "vmovl.u8 q1, d10                          \n"
          "vdup.16 q2, %10                           \n"
          "vsub.i16 q3, q2, q6                       \n"
          "vmull.u32 q4, d0, d6                      \n"
          "vmlal.u32 q4, d2, d12                     \n"
          "vmull.u32 q5, d1, d7                      \n"
          "vmlal.u32 q5, d3, d13                     \n"
          "vrshrn.u16 d0, q4 , #8                    \n"
          "vrshrn.u16 d1, q5 , #8                    \n"
          "vmovn.u16 d10, q0                         \n"
          "vst1.u8    {d10}, [%8]                    \n"
          : "+r"(src0),    // %0
            "+r"(src1),    // %1
            "+r"(src2),    // %2
            "+r"(src3),    // %3
            "+r"(src4),    // %4
            "+r"(src5),    // %5
            "+r"(src6),    // %6
            "+r"(src7),    // %7
            "+r"(dst),     // %8
            "+r"(pcoef),   // %9
            "+r"(one_val)  // %10
          :
          : "q0", "q1", "q2", "q3", "q4", "q5", "q6", "memory", "cc");
      dst += 8;
    }
    dst += (stride - width);
  }
}

/** ChromaProcessBilinearVSkipNEON
 *
 * @n_thrd_arg: struct with input thread parameters
 *
 * Resize Luma Process function
 *
 * return:
 **/
static void ChromaProcessBilinearVSkipNEON(neon_thrd_args* n_thrd_arg) {
  uint32_t row, col;
  uint8_t *src, *src0, *src1, *src2, *src3;
  uint16_t* pcoef;
  uint32_t h_ind;
  uint16_t h_rem;
  uint16_t one_val = 256;

  uint8_t* src_chroma = n_thrd_arg->src_chroma;
  uint8_t* dst =
      n_thrd_arg->dst_chroma + n_thrd_arg->line_start * n_thrd_arg->stride / 2;
  uint16_t* offsets = n_thrd_arg->input_offsets;
  uint16_t* coefs = n_thrd_arg->uv_coefs;
  uint32_t h_coef = n_thrd_arg->h_coef;
  uint32_t line_start = n_thrd_arg->line_start;
  uint32_t line_end = n_thrd_arg->line_end;
  uint32_t width = n_thrd_arg->width;
  uint32_t stride = n_thrd_arg->stride;
  uint32_t src_stride = n_thrd_arg->src_stride;

  for (row = (line_start / 2); row < (line_end / 2); row++) {
    h_ind = row * h_coef;
    h_rem = h_ind - ((h_ind >> 8) << 8);
    h_ind = ((h_ind - h_rem) >> 8);
    src = src_chroma + h_ind * src_stride;
    for (col = 0; col < (width / 8); col++) {
      src0 = src + offsets[col * 4] * 2;
      src1 = src + offsets[col * 4 + 1] * 2;
      src2 = src + offsets[col * 4 + 2] * 2;
      src3 = src + offsets[col * 4 + 3] * 2;
      pcoef = coefs + col * 8;

      asm volatile(
          "vld1.8     {d0}, [%0]                     \n"  // load up 4
          "vld1.8     {d1}, [%1]                     \n"
          "vld1.8     {d2}, [%2]                     \n"
          "vld1.8     {d3}, [%3]                     \n"
          "vld1.16    {q6}, [%5]                     \n"
          "vext.8 d4, d0, d0, #4                     \n"
          "vext.8 d4, d4, d1, #4                     \n"
          "vext.8 d5, d2, d2, #4                     \n"
          "vext.8 d5, d5, d3, #4                     \n"
          "vuzp.u16 d4, d5                           \n"
          "vmovl.u8 q0, d4                           \n"
          "vmovl.u8 q1, d5                           \n"
          "vdup.16 q4, %6                            \n"
          "vsub.i16 q5, q4, q6                       \n"
          "vmull.u32 q2, d0, d10                     \n"
          "vmlal.u32 q2, d2, d12                     \n"
          "vmull.u32 q3, d1, d11                     \n"
          "vmlal.u32 q3, d3, d13                     \n"
          "vrshrn.u16 d0, q2 , #8                    \n"
          "vrshrn.u16 d1, q3 , #8                    \n"
          "vmovn.u16 d10, q0                         \n"
          "vst1.u8    {d10}, [%4]                    \n"
          : "+r"(src0),    // %0
            "+r"(src1),    // %1
            "+r"(src2),    // %2
            "+r"(src3),    // %3
            "+r"(dst),     // %4
            "+r"(pcoef),   // %5
            "+r"(one_val)  // %6
          :
          : "q0", "q1", "q2", "q3", "q4", "q5", "q6", "q7", "memory", "cc");
      dst += 8;
    }
    dst += (stride - width);
  }
}
#else

/** LumaProcessBilinearVSkip
 *
 * @n_thrd_arg: struct with input thread parameters
 *
 * Resize Luma Process function
 *
 * return:
 **/
static void LumaProcessBilinearVSkip(neon_thrd_args* n_thrd_arg) {
  uint32_t row, col;
  uint32_t res_val;
  uint32_t w_ind, h_ind;
  uint32_t offset;
  uint16_t w_rem, h_rem;
  uint8_t* src;

  uint8_t* src_luma = n_thrd_arg->src_luma;
  uint8_t* dst =
      n_thrd_arg->dst_luma + n_thrd_arg->line_start * n_thrd_arg->stride;
  uint32_t w_coef = n_thrd_arg->w_coef;
  uint32_t h_coef = n_thrd_arg->h_coef;
  uint32_t line_start = n_thrd_arg->line_start;
  uint32_t line_end = n_thrd_arg->line_end;
  uint32_t width = n_thrd_arg->width;
  uint32_t stride = n_thrd_arg->stride;
  uint32_t src_stride = n_thrd_arg->src_stride;

  for (row = line_start; row < line_end; row++) {
    h_ind = row * h_coef;
    h_rem = h_ind - ((h_ind >> 8) << 8);
    h_ind = ((h_ind - h_rem) >> 8);
    for (col = 0; col < width; col++) {
      w_ind = col * w_coef;
      w_rem = w_ind - ((w_ind >> 8) << 8);
      w_ind = ((w_ind - w_rem) >> 8);
      offset = h_ind * src_stride + w_ind;
      src = src_luma + offset;
      res_val = ((src[0] * (256 - w_rem) + src[1] * w_rem + (1 << 4)) >> 8);

      if (res_val > 255) res_val = 255;

      *dst_luma++ = (uint8_t)res_val;
    }
    dst_luma += (stride - width);
  }
}

/** ChromaProcessBilinearVSkip
 *
 * @n_thrd_arg: struct with input thread parameters
 *
 * Resize Luma Process function
 *
 * return:
 **/
static void ChromaProcessBilinearVSkip(neon_thrd_args* n_thrd_arg) {
  uint32_t row, col;
  uint32_t res_val[2];
  uint32_t w_ind, h_ind;
  uint32_t offset;
  uint16_t w_rem, h_rem;

  uint8_t* src_chroma = n_thrd_arg->src_chroma;
  uint8_t* dst =
      n_thrd_arg->dst_chroma + n_thrd_arg->line_start * n_thrd_arg->stride / 2;
  uint32_t w_coef = n_thrd_arg->w_coef;
  uint32_t h_coef = n_thrd_arg->h_coef;
  uint32_t line_start = n_thrd_arg->line_start;
  uint32_t line_end = n_thrd_arg->line_end;
  uint32_t width = n_thrd_arg->width;
  uint32_t stride = n_thrd_arg->stride;
  uint32_t src_stride = n_thrd_arg->src_stride;

  for (row = (line_start / 2); row < (line_end / 2); row++) {
    for (col = 0; col < (width / 2); col++) {
      w_ind = col * w_coef;
      h_ind = row * h_coef;
      w_rem = w_ind - ((w_ind >> 8) << 8);
      w_ind = ((w_ind - w_rem) >> 8);
      h_rem = h_ind - ((h_ind >> 8) << 8);
      h_ind = ((h_ind - h_rem) >> 8);
      offset = h_ind * src_stride + w_ind * 2;
      res_val[0] = ((src_chroma[offset] * (256 - w_rem) +
                     src_chroma[offset + 2] * w_rem + (1 << 4)) >>
                    8);
      res_val[1] = ((src_chroma[offset + 1] * (256 - w_rem) +
                     src_chroma[offset + 3] * w_rem + (1 << 4)) >>
                    8);

      if (res_val[0] > 255) res_val[0] = 255;
      if (res_val[1] > 255) res_val[1] = 255;

      dst_chroma[row * stride + col * 2] = (uint8_t)res_val[0];
      dst_chroma[row * stride + col * 2 + 1] = (uint8_t)res_val[1];
    }
  }
}

#endif  //_ARM_NEON_SUPPORT_

/** UpdateInternalBuffs
*
* @ctx: internal data pointer
* @resn: resize input data structure
* Update internal buffers
*
* return:
**/
static void UpdateInternalBuffs(resn_cnt_t* ctx, resn_t* resn) {
  bool update_coefs = false;
  if ((resn->src_width != ctx->src_width) ||
      (resn->dst_width != ctx->dst_width)) {
    ctx->src_width = resn->src_width;
    ctx->src_height = resn->src_height;
    ctx->dst_width = resn->dst_width;
    ctx->dst_height = resn->dst_height;
    update_coefs = true;
  }

  if (update_coefs) {
    free(ctx->y_coefs);
    free(ctx->uv_coefs);
    free(ctx->input_offsets);

    ctx->y_coefs = (uint16_t*)malloc(ctx->dst_width * sizeof(uint16_t));
    ctx->uv_coefs = (uint16_t*)malloc(ctx->dst_width * sizeof(uint16_t));
    ctx->input_offsets = (uint16_t*)malloc(ctx->dst_width * sizeof(uint16_t));

    uint32_t w_coef = (ctx->src_width * 256) / ctx->dst_width;
    uint32_t w_ind;
    uint16_t w_rem, col;
    for (col = 0; col < ctx->dst_width; col++) {
      w_ind = col * w_coef;
      w_rem = w_ind - ((w_ind >> 8) << 8);
      w_ind = ((w_ind - w_rem) >> 8);
      ctx->y_coefs[col] = w_rem;
      ctx->input_offsets[col] = (uint16_t)w_ind;
    }

    for (col = 0; col < (ctx->dst_width / 2); col++) {
      w_ind = col * w_coef;
      w_rem = w_ind - ((w_ind >> 8) << 8);
      w_ind = ((w_ind - w_rem) >> 8);
      ctx->uv_coefs[col * 2] = w_rem;
      ctx->uv_coefs[col * 2 + 1] = w_rem;
    }
  }
}

/** resn_init
   *    @handle: resize internal data pointer
   *
   * Main Resizer Init function
   *
   * return: resn_status_t
   **/
resn_status_t resn_init(void** handle) {
  resn_status_t status = RESN_SUCCESS;
  int rc = 0;
  uint32_t i;

  if (!handle) {
    return RESN_FAILED_TO_CREATE_CONTEXT;
  }

  resn_cnt_t* ctx = (resn_cnt_t*)calloc(1, sizeof(resn_cnt_t));
  if (!ctx) {
    return RESN_FAILED_TO_CREATE_CONTEXT;
  }

  ctx->num_threads = 4 * 2;

  ctx->neon_work_args =
      (neon_thrd_args*)calloc(1, ctx->num_threads * sizeof(neon_thrd_args));

  for (i = 0; i < ctx->num_threads; i++) {
    ctx->neon_work_args[i].tid = i;
    ctx->neon_work_args[i].thread_started = false;
    ctx->neon_work_args[i].thread_ready = false;
    ctx->neon_work_args[i].thread_active = true;
    pthread_cond_init(&ctx->neon_work_args[i].signal_base, NULL);
    pthread_cond_init(&ctx->neon_work_args[i].signal_thread, NULL);
    pthread_mutex_init(&ctx->neon_work_args[i].lock, NULL);
  }

  for (i = 0; i < ctx->num_threads / 2; i++) {
    rc = pthread_create(&ctx->neon_work_args[i].thread, NULL,
                        neon_thread_y_pass, (void*)&ctx->neon_work_args[i]);
    if (rc) {
      ALOGE("failed to create y pass thread %d, rc = %d\n",
            ctx->neon_work_args[i].tid, rc);
      ctx->num_threads = i;
      resn_deinit(ctx);
      return RESN_FAILED_TO_CREATE_WORK_THREADS;
    }
  }

  for (i = ctx->num_threads / 2; i < ctx->num_threads; i++) {
    rc = pthread_create(&ctx->neon_work_args[i].thread, NULL,
                        neon_thread_uv_pass, (void*)&ctx->neon_work_args[i]);
    if (rc) {
      ALOGE("failed to create uv pass thread %d, rc = %d\n",
            ctx->neon_work_args[i].tid, rc);
      ctx->num_threads = i;
      resn_deinit(ctx);
      return RESN_FAILED_TO_CREATE_WORK_THREADS;
    }
  }

  *handle = (void*)ctx;
  return status;
}

/** resn_deinit
   *    @handle: resize internal data pointer
   *
   * Main Resizer deinit function
   *
   * return:
   **/
void resn_deinit(void* handle) {
  int rc = 0;
  uint32_t i;

  if (handle) {
    resn_cnt_t* ctx = (resn_cnt_t*)handle;

    for (i = 0; i < ctx->num_threads; i++) {
      pthread_mutex_lock(&ctx->neon_work_args[i].lock);
      ctx->neon_work_args[i].thread_active = false;
      pthread_cond_signal(&ctx->neon_work_args[i].signal_thread);
      pthread_mutex_unlock(&ctx->neon_work_args[i].lock);
      rc = pthread_join(ctx->neon_work_args[i].thread, NULL);
      if (rc) {
        ALOGE("error: failed to join y pass thread %d, rc = %d\n",
              ctx->neon_work_args[i].tid, rc);
      }

      pthread_cond_destroy(&ctx->neon_work_args[i].signal_thread);
      pthread_cond_destroy(&ctx->neon_work_args[i].signal_base);
      pthread_mutex_destroy(&ctx->neon_work_args[i].lock);
    }

    free(ctx->neon_work_args);
    free(ctx->y_coefs);
    free(ctx->uv_coefs);
    free(ctx->input_offsets);
    free(ctx);
    handle = NULL;
  }
}

/** resn_process
*
* @handle: internal data structure
* @resn: resize input data structure
*
* Main Resizer function
*
* return: resn_status_t
**/
resn_status_t resn_process(void* handle, resn_t* resn) {
  resn_status_t status = RESN_SUCCESS;
  uint32_t i;

  resn_cnt_t* ctx = (resn_cnt_t*)handle;

  if (!resn) {
    return (RESN_NOT_INITIALIZED);
  }

  if (resn->src_width > resn->src_stride) {
    return RESN_WRONG_WIDTH_INBUF;
  }

  if (resn->dst_width > resn->dst_stride) {
    return RESN_WRONG_WIDTH_OUTBUF;
  }

  UpdateInternalBuffs(ctx, resn);

  uint32_t width, height, stride, src_stride;
  uint32_t num_threads;
  neon_thrd_args* neon_work_args = ctx->neon_work_args;
  num_threads = ctx->num_threads;

  uint8_t* src_luma = resn->src_luma;
  uint8_t* src_chroma = resn->src_chroma;

  uint8_t* dst_luma = resn->dst_luma;
  uint8_t* dst_chroma = resn->dst_chroma;

  width = resn->dst_width;
  height = resn->dst_height;
  stride = resn->dst_stride;

  uint32_t src_width = resn->src_width;
  uint32_t src_height = resn->src_height;
  src_stride = resn->src_stride;

  uint32_t w_coef, h_coef;

  w_coef = (src_width * 256) / width;
  h_coef = (src_height * 256) / height;

  for (i = 0; i < (num_threads / 2); i++) {
    pthread_mutex_lock(&neon_work_args[i].lock);
    neon_work_args[i].width = width;
    neon_work_args[i].height = height;
    neon_work_args[i].src_stride = src_stride;
    neon_work_args[i].stride = stride;
    neon_work_args[i].src_luma = src_luma;
    neon_work_args[i].src_chroma = src_chroma;
    neon_work_args[i].dst_luma = dst_luma;
    neon_work_args[i].dst_chroma = dst_chroma;
    neon_work_args[i].y_coefs = ctx->y_coefs;
    neon_work_args[i].uv_coefs = ctx->uv_coefs;
    neon_work_args[i].input_offsets = ctx->input_offsets;
    neon_work_args[i].w_coef = w_coef;
    neon_work_args[i].h_coef = h_coef;
    neon_work_args[i].line_start = i * ((height * 2 / num_threads) & (~1u));
    neon_work_args[i].line_end =
        neon_work_args[i].line_start + ((height * 2 / num_threads) & (~1u));
    pthread_mutex_unlock(&neon_work_args[i].lock);
  }
  neon_work_args[i - 1].line_end = height;

  for (; i < num_threads; i++) {
    pthread_mutex_lock(&neon_work_args[i].lock);
    neon_work_args[i].width = width;
    neon_work_args[i].height = height;
    neon_work_args[i].src_stride = src_stride;
    neon_work_args[i].stride = stride;
    neon_work_args[i].src_luma = src_luma;
    neon_work_args[i].src_chroma = src_chroma;
    neon_work_args[i].dst_luma = dst_luma;
    neon_work_args[i].dst_chroma = dst_chroma;
    neon_work_args[i].y_coefs = ctx->y_coefs;
    neon_work_args[i].uv_coefs = ctx->uv_coefs;
    neon_work_args[i].input_offsets = ctx->input_offsets;
    neon_work_args[i].w_coef = w_coef;
    neon_work_args[i].h_coef = h_coef;
    neon_work_args[i].line_start =
        (i - num_threads / 2) * ((height * 2 / num_threads) & (~1u));
    neon_work_args[i].line_end =
        neon_work_args[i].line_start + ((height * 2 / num_threads) & (~1u));
    pthread_mutex_unlock(&neon_work_args[i].lock);
  }

  neon_work_args[i - 1].line_end = height;

  for (i = 0; i < num_threads / 2; i++) {
    pthread_mutex_lock(&neon_work_args[i].lock);
    neon_work_args[i].thread_started = true;
    pthread_cond_signal(&neon_work_args[i].signal_thread);
    pthread_mutex_unlock(&neon_work_args[i].lock);
  }

  for (i = 0; i < num_threads / 2; i++) {
    pthread_mutex_lock(&neon_work_args[i].lock);
    while (!neon_work_args[i].thread_ready) {
      pthread_cond_wait(&neon_work_args[i].signal_base,
                        &neon_work_args[i].lock);
    }
    neon_work_args[i].thread_ready = false;
    pthread_mutex_unlock(&neon_work_args[i].lock);
  }

  for (i = num_threads / 2; i < num_threads; i++) {
    pthread_mutex_lock(&neon_work_args[i].lock);
    neon_work_args[i].thread_started = true;
    pthread_cond_signal(&neon_work_args[i].signal_thread);
    pthread_mutex_unlock(&neon_work_args[i].lock);
  }

  for (i = num_threads / 2; i < num_threads; i++) {
    pthread_mutex_lock(&neon_work_args[i].lock);
    while (!neon_work_args[i].thread_ready) {
      pthread_cond_wait(&neon_work_args[i].signal_base,
                        &neon_work_args[i].lock);
    }
    neon_work_args[i].thread_ready = false;
    pthread_mutex_unlock(&neon_work_args[i].lock);
  }

  return (status);
}

static void* neon_thread_y_pass(void* arg) {
  neon_thrd_args* n_thrd_arg = (neon_thrd_args*)arg;
  neon_thrd_args thrd_arg;
  bool thread_active = true;

  while (thread_active) {
    pthread_mutex_lock(&n_thrd_arg->lock);
    while (!n_thrd_arg->thread_started && n_thrd_arg->thread_active) {
      pthread_cond_wait(&n_thrd_arg->signal_thread, &n_thrd_arg->lock);
    }
    n_thrd_arg->thread_started = false;
    thrd_arg = *n_thrd_arg;
    thread_active = n_thrd_arg->thread_active;
    if (!thread_active) {
      break;
    }
    pthread_mutex_unlock(&n_thrd_arg->lock);

#ifdef _ARM_NEON_SUPPORT_
    LumaProcessBilinearVSkipNEON(&thrd_arg);
#else
    LumaProcessBilinearVSkip(&thrd_arg);
#endif

    pthread_mutex_lock(&n_thrd_arg->lock);
    n_thrd_arg->thread_ready = true;
    pthread_cond_signal(&n_thrd_arg->signal_base);
    pthread_mutex_unlock(&n_thrd_arg->lock);
  }

  return NULL;
}

static void* neon_thread_uv_pass(void* arg) {
  neon_thrd_args* n_thrd_arg = (neon_thrd_args*)arg;
  neon_thrd_args thrd_arg;
  bool thread_active = true;

  while (thread_active) {
    pthread_mutex_lock(&n_thrd_arg->lock);
    while (!n_thrd_arg->thread_started && n_thrd_arg->thread_active) {
      pthread_cond_wait(&n_thrd_arg->signal_thread, &n_thrd_arg->lock);
    }
    n_thrd_arg->thread_started = false;
    thrd_arg = *n_thrd_arg;
    thread_active = n_thrd_arg->thread_active;
    if (!thread_active) {
      break;
    }
    pthread_mutex_unlock(&n_thrd_arg->lock);

#ifdef _ARM_NEON_SUPPORT_
    ChromaProcessBilinearVSkipNEON(&thrd_arg);
#else
    ChromaProcessBilinearVSkip(&thrd_arg);
#endif

    pthread_mutex_lock(&n_thrd_arg->lock);
    n_thrd_arg->thread_ready = true;
    pthread_cond_signal(&n_thrd_arg->signal_base);
    pthread_mutex_unlock(&n_thrd_arg->lock);
  }

  return NULL;
}

}; // namespace neonresizer

}; // namespace qmmf
