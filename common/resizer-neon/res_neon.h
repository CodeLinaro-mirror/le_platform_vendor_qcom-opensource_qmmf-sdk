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

namespace qmmf {

namespace neonresizer {

/** resn_status_t:
 *    @RESN_ERR_HW_PLATFORM_NOT_AVAILABLE: HW Platform not available
 *    @RESN_ERR_HW_DEVICE_NOT_AVAILABLE: HW device not available
 *    @RESN_ERR_GET_DEVICE_INFO: failed to get device information
 *    @RESN_FILE_DOESNT_EXIST: designated file doesn't exist
 *    @RESN_UNABLE_TO_DETERMINE_FILESIZE: unable to determine the
 *      size of the designated file
 *    @RESN_ERR_FAILED_TO_LOAD_IMGPROC_KERNEL: failed to enqueue
 *      image processing kernel
 *    @RESN_ERR_SET_IMGPROC_KERNEL_ARG: error at setting image processing
 *      kernel argument
 *    @RESN_FAILED_TO_CREATE_IMGPROC_KERNEL: error at creating
 *     image processing kernel from program
 *    @RESN_UNABLE_TO_BUILD_IMGPROC_PROGRAM: error at compiling
 *      image processing kernel
 *    @RESN_ERR_FAILED_TO_FLUSH_CQ: failed to flush command queue
 *    @RESN_UNABLE_TO_READ_FILE: failed to read designated file
 *    @RESN_WRONG_STRIDE_OUTBUF: output buffer stride and argument
 *      stride don't match
 *    @RESN_WRONG_STRIDE_INBUF: Input buffer stride and argument
 *      stride don't match
 *    @RESN_WRONG_WIDTH_OUTBUF: Error, input buffer value does not match
 *    @RESN_WRONG_WIDTH_INBUF: Error, output buffer value does not match
 *    @RESN_ERR_UNABLE_TO_MAP_OUTBUF: error at mapping output buffer
 *    @RESN_ERR_UNABLE_TO_MAP_INBUF: : error at mapping input buffer
 *    @RESN_ERR_INVALID_INPUT: invalid input
 *    @RESN_ERR_INPUT_BUFFER_NOT_MAPPED: input buffer not mapped
 *    @RESN_ERR_OUTPUT_BUFFER_NOT_MAPPED: output buffer not mapped
 *    @RESN_FAILED_TO_CREATE_PROG_WITH_BIN: error at creating image processing
 *      program from bin file
 *    @RESN_FAILED_TO_CREATE_PROG_WITH_SRC: error at creating compiling image
 *      processing src
 *    @RESN_FAILED_TO_CREATE_CONTEXT: error at creating image processing context
 *    @RESN_FAILED_TO_CREATE_CQUEUE: error at creating image processing
 *      command queue
 *    @RESN_NOT_INITIALIZED: error, resizer library is not initialized
 *    @RESN_ERR_NO_MEMORY: error, not enough heap memory
 *    @RESN_FAIL: generic error
 *    @RESN_UNABLE_TO_OPEN_FILE: error, failed to open desinated file
 *    @RESN_UNABLE_TO_WRITE_FILE: error, failed to write designated file
 *    @RESN_FAILED_TO_GET_BLD_LOG_SIZE: failed to get size of build log
 *    @RESN_FAILED_TO_GET_BLD_LOG: failed to get pointer to build log
 *    @RESN_FAILED_TO_GET_DEV_ID_FROM_PROG_INFO: Failed to get image processing
 *      device info from program ID
 *    @RESN_PROGRAM_NOT_BUILT_FOR_DEV: program built for different device
 *    @RESN_FAILED_TO_GET_PROG_BIN_SIZE: could not get size of image processing
 *      program binary
 *    @RESN_FAILED_TO_GET_PROG_BINNARY: could not get image processing
 *      program binary
 *    @RESN_SUCCESS: success
 *
 *  This enum defines the return status from resizer neon library interface
 */
typedef enum {
  RESN_ERR_HW_PLATFORM_NOT_AVAILABLE = -1024,
  RESN_ERR_HW_DEVICE_NOT_AVAILABLE,          // 1023
  RESN_ERR_GET_DEVICE_INFO,                  // 1022
  RESN_FILE_DOESNT_EXIST,                    // 1021
  RESN_UNABLE_TO_DETERMINE_FILESIZE,         // 1020
  RESN_ERR_FAILED_TO_LOAD_IMGPROC_KERNEL,    // 1019
  RESN_ERR_SET_IMGPROC_KERNEL_ARG,           // 1018
  RESN_FAILED_TO_CREATE_IMGPROC_KERNEL,      // 1017
  RESN_UNABLE_TO_BUILD_IMGPROC_PROGRAM,      // 1016
  RESN_ERR_FAILED_TO_FLUSH_CQ,               // 1015
  RESN_UNABLE_TO_READ_FILE,                  // 1014
  RESN_WRONG_STRIDE_OUTBUF,                  // 1013
  RESN_WRONG_STRIDE_INBUF,                   // 1012
  RESN_WRONG_WIDTH_OUTBUF,                   // 1011
  RESN_WRONG_WIDTH_INBUF,                    // 1010
  RESN_ERR_UNABLE_TO_MAP_OUTBUF,             // 1009
  RESN_ERR_UNABLE_TO_MAP_INBUF,              // 1008
  RESN_ERR_INVALID_INPUT,                    // 1007
  RESN_ERR_INPUT_BUFFER_NOT_MAPPED,          // 1006
  RESN_ERR_OUTPUT_BUFFER_NOT_MAPPED,         // 1005
  RESN_FAILED_TO_CREATE_PROG_WITH_BIN,       // 1004
  RESN_FAILED_TO_CREATE_PROG_WITH_SRC,       // 1003
  RESN_FAILED_TO_CREATE_CONTEXT,             // 1002
  RESN_FAILED_TO_CREATE_CQUEUE,              // 1001
  RESN_NOT_INITIALIZED,                      // 1000
  RESN_ERR_NO_MEMORY,                        // 999
  RESN_FAIL,                                 // 998
  RESN_UNABLE_TO_OPEN_FILE,                  // 997
  RESN_UNABLE_TO_WRITE_FILE,                 // 996
  RESN_ERR_SET_PERF_HINT,                    // 995
  RESN_FAILED_TO_GET_BLD_LOG_SIZE,           // 994
  RESN_FAILED_TO_GET_BLD_LOG,                // 993
  RESN_FAILED_TO_GET_DEV_ID_FROM_PROG_INFO,  // 992
  RESN_PROGRAM_NOT_BUILT_FOR_DEV,            // 991
  RESN_FAILED_TO_GET_PROG_BIN_SIZE,          // 990
  RESN_FAILED_TO_GET_PROG_BINNARY,           // 989
  RESN_FAILED_TO_CREATE_WORK_THREADS,        // 988
  RESN_SUCCESS = 0,
} resn_status_t;

typedef struct { int enable; } chromatix_resn_type;
typedef enum {
  RES_BILINEAR_V_SKIP = 0,
  RES_BILINEAR = 1,
  RES_NUMBER
} res_method_t;

typedef struct resn_s {
  // Tuning data parameters
  chromatix_resn_type* resn_tuning;

  // Input data pointers
  unsigned char* src_luma;
  unsigned char* src_chroma;

  // Output data pointers
  unsigned char* dst_luma;
  unsigned char* dst_chroma;

  // Input buffer dimensions
  unsigned int src_width;
  unsigned int src_height;
  unsigned int src_stride;

  // Output buffer dimensions
  unsigned int dst_width;
  unsigned int dst_height;
  unsigned int dst_stride;
  // Resizer method
  res_method_t res_method;
} resn_t;

#ifdef __cplusplus
extern "C" {
#endif

extern char* resn_get_version();
extern resn_status_t resn_init(void** handle);
extern resn_status_t resn_process(void* handle, resn_t* params);
extern void resn_deinit(void* handle);

#ifdef __cplusplus
}

} // namespace neonresizer
} // namespace qmmf

#endif
