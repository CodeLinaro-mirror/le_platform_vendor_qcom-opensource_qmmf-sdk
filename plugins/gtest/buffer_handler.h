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

#include <cstring>
#include <list>
#include <memory>
#include <string>
#include <vector>

#include "qmmf_algo_gtest_configuration_buffer.h"

#ifdef ANDROID
#include "ion_buffer.h"
#else
#include "heap_buffer.h"
#endif

namespace qmmf {
namespace qmmf_alg_plugin {

/** BufferHandler:
 *    @platform_buffer_: platform buffer
 *    @input_file_name_: input file name
 *    @output_file_name_: output file name
 *    @file_stride_: input and/or output file stride
 *    @file_scanline_: input and/or output file scanline
 *    @border_up_: plane[0]'s first row containing actual data
 *                 plane[i]'s border_up = border_up_ / (i+1)
 *    @border_left_: the first column in each plane's row, containing actual
 *                   data
 *    @border_down_: (plane[0]'s last row) -
 *                   (plane[0]'s last row, containing actual data);
 *                    plane[i]'s border_down = border_down_ / (i+1)
 *    @border_right_: (last column in the row) -
 *                    (last column in the row, containing actual data)
 *    @buffer_is_filled_: true if a call to FillBufferWith() has been made
 *    @filled_value_: this value is assigned to each byte of the buffer after a
 *                    call to FillBufferWith()
 *
 *  This class handles qmmf algo buffer
 **/
class BufferHandler : public AlgBuffer {
 private:
  BufferHandler(uint8_t *vaddr, int32_t fd, uint32_t size, bool cached,
                PixelFormat pix_fmt, int64_t timestamp, uint32_t frame_number,
                std::vector<BufferPlane> &plane,
                std::shared_ptr<PlatformBuffer> &platform_buffer,
                const std::string &input_file_name,
                const std::string &output_file_name, uint32_t file_stride,
                uint32_t file_scanline, uint32_t border_up,
                uint32_t border_left, uint32_t border_down,
                uint32_t border_right);

 public:
  /** New
    *    @requirements: buffer requirements
    *    @buffer_configurations: buffer configurations
    *    @border_up: plane[0]'s first row containing actual data
    *                plane[i]'s border_up = border_up / (i+1)
    *    @border_left: the first column in each plane's row, containing actual
    *                  data
    *    @border_down: (plane[0]'s last row) -
    *                  (plane[0]'s last row, containing actual data);
    *                   plane[i]'s border_down = border_down / (i+1)
    *    @min_border_right: the minimal value of the new buffers's border_right
    *
    * creates new instance of qmmf algo buffer handlers
    *
    * return: list of shared pointers of buffer handlers
    **/
  static std::list<std::shared_ptr<BufferHandler>> New(
      const BufferRequirements &requirements,
      const std::list<std::shared_ptr<QmmfAlgoConfigurationBuffer>>
          &buffer_configurations,
      uint32_t border_up = 0, uint32_t border_left = 0,
      uint32_t border_down = 0, uint32_t min_border_right = 0);

  /** New
    *    @requirements: buffer requirements
    *    @pix_fmt: pixel format
    *    @width: width
    *    @height: height
    *    @file_stride: input and/or output file stride
    *    @file_scanline: input and/or output file scanline
    *    @input_file_name: input file name
    *    @output_file_name: output file name
    *    @border_up: plane[0]'s first row containing actual data
    *                plane[i]'s border_up = border_up / (i+1)
    *    @border_left: the first column in each plane's row, containing actual
    *                  data
    *    @border_down: (plane[0]'s last row) -
    *                  (plane[0]'s last row, containing actual data);
    *                   plane[i]'s border_down = border_down / (i+1)
    *    @min_border_right: the minimal value of the new buffer's border_right
    *
    * creates new instance of qmmf algo buffer handler
    *
    * return: shared pointer of buffer handlers
    **/
  static std::shared_ptr<BufferHandler> New(
      const BufferRequirements &requirements, PixelFormat pix_fmt,
      uint32_t width, uint32_t height, uint32_t file_stride,
      uint32_t file_scanline, const std::string &input_file_name,
      const std::string &output_file_name, uint32_t border_up = 0,
      uint32_t border_left = 0, uint32_t border_down = 0,
      uint32_t min_border_right = 0);

  /** GetWidthInBytes
    *    @width_in_pixels: width in pixels
    *    @pix_fmt: pixel format
    *
    * returns width in bytes based on pixel format
    *
    * return: width in bytes based on pixel format
    **/
  static uint32_t GetWidthInBytes(uint32_t width_in_pixels,
                                  PixelFormat pix_fmt);
  /** GetHeightInLines
    *    @image_height: image height
    *    @pix_fmt: pixel format
    *    @plane: plane
    *
    * returns height in lines based on pixel format and plane
    *
    * return: height in lines based on pixel format and plane
    **/
  static uint32_t GetHeightInLines(uint32_t image_height,
                                   PixelFormat pix_fmt,
                                   uint32_t plane);

  /** Compare
    *    @other: other buffer to compare with
    *
    * compares with other buffer
    *
    * return: true is buffers are the same
    **/
  bool Compare(const std::shared_ptr<BufferHandler> &other) const;

  /** Compare
    *    @cb: configuration buffer to compare with
    *
    * compares with configuration buffer
    *
    * return: true is buffers are the same
    **/
  bool Compare(const std::shared_ptr<QmmfAlgoConfigurationBuffer> &cb) const;

  /** Compare
    *    @fd: fd to compare with
    *
    * compares with fd
    *
    * return: true is buffers are the same
    **/
  bool Compare(int32_t fd) const;

  /** ReadInputFile
    *
    * reads input file
    *
    * return: void
    **/
  void ReadInputFile();

  /** WriteOutputFile
    *
    * writes output file
    *
    * return: void
    **/
  void WriteOutputFile() const;

  /** FillBufferWith
    *    @value: value to fill buffer with
    *
    * sets all bytes of the buffer to be equal to value
    *
    * return: void
    **/
  void FillBufferWith(uint8_t value);

  /** MemoryIsCorrupted
    *
    * check if the additional padding bytes of the buffer have remained
    * unchanged after the last call to FillBufferWith()
    * If FillBufferWith() hasn't been called, the function throws an exception
    *
    * return: true, if the value of an offset byte is corrupted;
    *         false, if all offset bytes have remained unchanged
    **/
  bool MemoryIsCorrupted() const;

 private:
  const std::shared_ptr<PlatformBuffer> platform_buffer_;
  const std::string input_file_name_;
  const std::string output_file_name_;

  const uint32_t file_stride_;
  const uint32_t file_scanline_;

 private:
  uint32_t border_up_;
  uint32_t border_left_;
  uint32_t border_down_;
  uint32_t border_right_;

 private:
  bool buffer_is_filled_;
  uint8_t filled_value_;
};

};  // namespace qmmf_alg_plugin
};  // namespace qmmf
