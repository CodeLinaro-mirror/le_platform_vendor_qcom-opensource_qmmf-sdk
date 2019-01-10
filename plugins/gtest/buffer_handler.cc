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

#define LOG_TAG "BufferHandler"

#include <utils/Log.h>
#include <chrono>
#include <cmath>

#include <qmmf-alg/qmmf_alg_utils.h>

#include "buffer_handler.h"
#include "heap_buffer.h"
#include "ion_buffer.h"

namespace qmmf {
namespace qmmf_alg_plugin {

/** BufferHandler
 *    @vaddr: buffer virtuall address
 *    @fd: buffer fd
 *    @size: buffer size
 *    @cached: flag indicating whether buffer is cached
 *    @pix_fmt: pixel format
 *    @timestamp: buffer timestamp
 *    @frame_number: buffer frame number
 *    @plane: vector of buffer planes
 *    @input_file_name: input file name
 *    @file_stride_: input and/or output file stride
 *    @file_scanline_: input and/or output file scanline
 *    @border_up: plane[0]'s first row containing actual data
 *                plane[i]'s border_up = border_up / (i+1)
 *    @border_left: the first column in each plane's row, containing actual
 *                  data
 *    @border_down: (plane[0]'s last row) -
 *                  (plane[0]'s last row, containing actual data);
 *                   plane[i]'s border_down = border_down / (i+1)
 *    @border_right: (last column in the row) -
 *                   (last column in the row, containing actual data)
 *    @buffer_holder: buffer holder
 *
 * creates new instance of BufferHandler
 *
 * return: void
 **/
BufferHandler::BufferHandler(
    uint8_t *vaddr, int32_t fd, uint32_t size, bool cached, PixelFormat pix_fmt,
    int64_t timestamp, uint32_t frame_number, std::vector<BufferPlane> &plane,
    const std::string &input_file_name, const std::string &output_file_name,
    uint32_t file_stride, uint32_t file_scanline, uint32_t border_up,
    uint32_t border_left, uint32_t border_down, uint32_t border_right,
    std::shared_ptr<IBufferHolder> &buffer_holder)
    : AlgBuffer(vaddr, fd, size, cached, pix_fmt, timestamp, frame_number,
                plane),
      input_file_name_(input_file_name),
      output_file_name_(output_file_name),
      file_stride_(file_stride),
      file_scanline_(file_scanline),
      border_up_(border_up),
      border_left_(border_left),
      border_down_(border_down),
      border_right_(border_right),
      buffer_is_filled_(false),
      filled_value_(0),
      buffer_holder_(buffer_holder),
      cache_handler_(this->NewCacheHandler(*this)) {}

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
std::list<std::shared_ptr<BufferHandler>> BufferHandler::New(
    const BufferRequirements &requirements,
    const std::list<std::shared_ptr<QmmfAlgoConfigurationBuffer>>
        &buffer_configurations,
    uint32_t border_up, uint32_t border_left, uint32_t border_down,
    uint32_t min_border_right) {
  if (buffer_configurations.size() != requirements.count_) {
    std::string err = std::string("Buffer configuration size ") +
                      std::to_string(buffer_configurations.size()) +
                      (" is different from algo requirements ") +
                      std::to_string(requirements.count_);
    Utils::ThrowException(__func__, err);
  }

  std::list<std::shared_ptr<BufferHandler>> allocated_buffers;
  for (auto &c : buffer_configurations) {
    auto b = New(requirements, c->pixel_format_, c->width_, c->height_,
                 c->stride_, c->scanline_, c->input_file_name_,
                 c->output_file_name_, c->heap_buffer_, border_up, border_left,
                 border_down, min_border_right);
    allocated_buffers.push_back(b);
  }
  return allocated_buffers;
}

/** New
 *    @requirements: buffer requirements
 *    @pix_fmt: pixel format
 *    @width: width
 *    @height: height
 *    @file_stride: input and/or output file stride
 *    @file_scanline: input and/or output file scanline
 *    @input_file_name: input file name
 *    @output_file_name: output file name
 *    @heap_buffer: flag indicating whether buffer is heap
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
std::shared_ptr<BufferHandler> BufferHandler::New(
    const BufferRequirements &requirements, PixelFormat pix_fmt, uint32_t width,
    uint32_t height, uint32_t file_stride, uint32_t file_scanline,
    const std::string &input_file_name, const std::string &output_file_name,
    bool heap_buffer, uint32_t border_up, uint32_t border_left,
    uint32_t border_down, uint32_t min_border_right) {
  uint32_t num_planes = 0;

  // Get number of planes
  num_planes = GetNumPlanes(pix_fmt);

  // The stride is common for all planes of the buffer
  uint32_t buffer_stride = border_left + file_stride + min_border_right;

  buffer_stride =
      Utils::MakeDivisibleBy(buffer_stride, requirements.stride_alignment_);

  uint32_t border_right =
      buffer_stride - (border_left + GetWidthInBytes(width, pix_fmt));

  uint32_t buffer_size = 0;
  uint32_t plane_alignment =
      Utils::LCM(requirements.plane_alignment_, requirements.stride_alignment_);

  std::vector<BufferPlane> planes;

  for (uint32_t i = 0; i < num_planes; i++) {
    uint32_t plane_height = GetHeightInLines(height, pix_fmt, i);
    uint32_t plane_border_up = GetHeightInLines(border_up, pix_fmt, i);
    uint32_t plane_border_down = GetHeightInLines(border_down, pix_fmt, i);
    uint32_t plane_lines_count =
        plane_border_up + plane_height + plane_border_down;

    uint32_t plane_size = plane_lines_count * buffer_stride;

    plane_size = Utils::MakeDivisibleBy(plane_size, plane_alignment);

    buffer_size += plane_size;

    planes.push_back(
        BufferPlane(width, plane_height, buffer_stride, 0, plane_size));
  }

  // After we allocate dynamic memory for the buffer, we may have to add a
  // number in the interval [1, plane_alignment - 1] to the first address
  // of the buffer (vaddr) in order to make every plane start from an address
  // divisible by the plane_alignment
  buffer_size += plane_alignment;

  std::shared_ptr<IBufferHolder> buffer_holder = nullptr;
  if (heap_buffer) {
    buffer_holder = HeapBuffer::New(buffer_size);
  } else {
    buffer_holder = IonBuffer::New(buffer_size, requirements.cached_);
  }

  uint8_t *vaddr = buffer_holder->GetAddr();
  if (nullptr == vaddr) {
    Utils::ThrowException(__func__, "cannot allocate memory");
  }

  size_t addr = reinterpret_cast<size_t>(vaddr);

  // addr is now equal to the starting address of the first plane of the
  // buffer
  addr = Utils::MakeDivisibleBy(addr, plane_alignment);

  for (uint32_t i = 0; i < planes.size(); i++) {
    uint32_t plane_border_up = GetHeightInLines(border_up, pix_fmt, i);
    size_t addr_of_actual_data_in_plane =
        addr + (plane_border_up * buffer_stride) + border_left;

    planes[i].offset_ =
        addr_of_actual_data_in_plane - reinterpret_cast<size_t>(vaddr);

    addr += planes[i].length_;
  }

  int64_t timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                          std::chrono::system_clock::now().time_since_epoch())
                          .count();

  static uint32_t frame_number = 0;
  frame_number++;

  std::shared_ptr<BufferHandler> new_handler(new BufferHandler(
      vaddr, buffer_holder->GetFd(), buffer_size, true, pix_fmt, timestamp,
      frame_number, planes, input_file_name, output_file_name, file_stride,
      file_scanline, border_up, border_left, border_down, border_right,
      buffer_holder));

  return new_handler;
}

/** Compare
 *    @other: other buffer to compare with
 *
 * compares with other buffer
 *
 * return: true is buffers are the same
 **/
bool BufferHandler::Compare(const std::shared_ptr<BufferHandler> &other) const {
  bool rc = false;

  // Compare the number of planes of both buffers
  if (plane_.size() != other->plane_.size()) {
    Utils::ThrowException(__func__,
                          "number of other buffer planes is different "
                          "than number of buffer planes");
  }
  for (uint32_t i = 0; i < plane_.size(); i++) {
    if (plane_[i].height_ != other->plane_[i].height_) {
      Utils::ThrowException(
          __func__, "other buffer height is different than buffer height");
    }
    if (plane_[i].width_ != other->plane_[i].width_) {
      Utils::ThrowException(
          __func__, "other buffer width is different than buffer width");
    }
    if (pix_fmt_ != other->pix_fmt_) {
      Utils::ThrowException(
          __func__,
          "other buffer pixel format is different than buffer pixel format");
    }
  }

  for (uint32_t i = 0; i < plane_.size(); i++) {
    uint8_t *my_p = vaddr_ + plane_[i].offset_;
    uint8_t *other_p = other->vaddr_ + other->plane_[i].offset_;

    for (uint32_t j = 0; j < plane_[i].height_; j++) {
      rc = !std::memcmp(other_p, my_p,
                        GetWidthInBytes(plane_[i].width_, pix_fmt_));
      if (!rc) {
        return rc;
      }
      my_p += plane_[i].stride_;
      other_p += other->plane_[i].stride_;
    }
  }

  return rc;
}

/** Compare
 *    @cb: configuration buffer to compare with
 *
 * compares with configuration buffer
 *
 * return: true is buffers are the same
 **/
bool BufferHandler::Compare(
    const std::shared_ptr<QmmfAlgoConfigurationBuffer> &cb) const {
  return !((pix_fmt_ != cb->pixel_format_) ||
           (plane_[0].width_ != cb->width_) ||
           (plane_[0].height_ != cb->height_) ||
           (file_stride_ != cb->stride_) || (file_scanline_ != cb->scanline_));
}

/** Compare
 *    @fd: fd to compare with
 *
 * compares with fd
 *
 * return: true is buffers are the same
 **/
bool BufferHandler::Compare(int32_t fd) const { return fd == fd_; }

/** ReadInputFile
 *
 * reads input file
 *
 * return: void
 **/
void BufferHandler::ReadInputFile() {
  if (input_file_name_.length()) {
    std::string complete_file_name = Utils::GetDataFolder() + input_file_name_;

    std::ifstream input_file(complete_file_name,
                             std::ios::binary | std::ios::ate);
    if (!input_file.is_open()) {
      Utils::ThrowException(__func__, "cannot open file " + complete_file_name);
    }

    auto file_size = input_file.tellg();
    input_file.seekg(0);

    uint32_t expected_file_size = 0;
    for (uint32_t i = 0; i < plane_.size(); ++i) {
      uint32_t plane_scanline = GetHeightInLines(file_scanline_, pix_fmt_, i);
      expected_file_size +=
          static_cast<uint64_t>(file_stride_) * plane_scanline;
    }

    if (file_size != expected_file_size) {
      std::string err = std::string("Input file size ") +
                        std::to_string(file_size) +
                        " is different than specified file size " +
                        std::to_string(expected_file_size);
      Utils::ThrowException(__func__, err);
    }

    uint8_t *out_p = nullptr;

    for (uint32_t i = 0; i < plane_.size(); i++) {
      out_p = vaddr_ + plane_[i].offset_;

      for (uint32_t j = 0; j < plane_[i].height_; j++) {
        input_file.seekg(file_stride_ * j + i * file_scanline_ * file_stride_);
        input_file.read(reinterpret_cast<char *>(out_p),
                        GetWidthInBytes(plane_[i].width_, pix_fmt_));
        out_p += plane_[i].stride_;
      }
    }
  } else {
    uint8_t *out_p = nullptr;

    for (uint32_t i = 0; i < plane_.size(); i++) {
      out_p = vaddr_ + plane_[i].offset_;
      auto width_in_bytes = GetWidthInBytes(plane_[i].width_, pix_fmt_);

      for (uint32_t j = 0; j < plane_[i].height_; j++) {
        for (uint32_t k = 0; k < width_in_bytes; k++) {
          *out_p++ = j + k;
        }
        out_p += (plane_[i].stride_ - width_in_bytes);
      }
    }
  }
  cache_handler_->CpuAccessEnd();
}

/** WriteOutputFile
 *
 * writes output file
 *
 * return: void
 **/
void BufferHandler::WriteOutputFile() const {
  if (output_file_name_.length()) {
    std::string complete_file_name = Utils::GetDataFolder() + output_file_name_;

    std::ofstream output_file(complete_file_name, std::ios::binary);
    if (!output_file.is_open()) {
      Utils::ThrowException(__func__, "cannot open file " + complete_file_name);
    }

    for (uint32_t i = 0; i < plane_.size(); ++i) {
      uint32_t plane_scanline = GetHeightInLines(file_scanline_, pix_fmt_, i);
      std::vector<uint8_t> dummy(file_stride_ * plane_scanline, 0);

      uint8_t *in_p = vaddr_ + plane_[i].offset_;

      uint8_t *out_p = dummy.data();

      for (uint32_t row = 0; row < plane_[i].height_; ++row) {
        memcpy(out_p, in_p, GetWidthInBytes(plane_[i].width_, pix_fmt_));

        in_p += plane_[i].stride_;
        out_p += file_stride_;
      }
      output_file.write(reinterpret_cast<char *>(dummy.data()), dummy.size());
    }
  }
}

/** FillBufferWith
 *    @value: value to fill buffer with
 *
 * sets all bytes of the buffer to be equal to value
 *
 * return: void
 **/
void BufferHandler::FillBufferWith(uint8_t value) {
  buffer_is_filled_ = true;
  filled_value_ = value;

  std::memset(vaddr_, value, size_);

  cache_handler_->CpuAccessEnd();
}

/** MemoryIsCorrupted
 *
 * check if the additional padding bytes of the buffer have remained
 * unchanged after the last call to FillBufferWith()
 * If FillBufferWith() hasn't been called, the function throws an exception
 *
 * return: true, if the value of an offset byte is corrupted;
 *         false, if all offset bytes have remained unchanged
 **/
bool BufferHandler::MemoryIsCorrupted() const {
  if (false == buffer_is_filled_) {
    Utils::ThrowException(__func__, "The buffer is not filled");
  }

  int cnt = 0;

  for (uint32_t plane_index = 0; plane_index < plane_.size(); ++plane_index) {
    const uint8_t *pi =
        vaddr_ + (plane_[plane_index].offset_ -
                  (border_left_ + border_up_ * plane_[plane_index].stride_ /
                                      (plane_index + 1)));

    // Check the up padding for corruption
    for (uint32_t i = 0;
         i < ((border_up_ / (plane_index + 1)) * plane_[plane_index].stride_);
         ++i) {
      if (*pi++ != filled_value_) {
        ALOGE("corrupted pixel up %d plane_index %d offset %d position %dx%d",
              i, plane_index, (uint32_t)(pi - vaddr_),
              (uint32_t)(pi - vaddr_) % plane_[plane_index].stride_,
              ((uint32_t)(pi - vaddr_) / plane_[plane_index].stride_ -
               plane_index * (plane_[0].height_ + border_up_ + border_down_)) *
                  (plane_index + 1));
        cnt++;
      }
    }

    for (uint32_t row = 0; row < plane_[plane_index].height_; ++row) {
      // Check the left padding for corruption
      for (uint32_t col = 0; col < border_left_; ++col) {
        if (*pi++ != filled_value_) {
          ALOGE(
              "corrupted pixel left %d plane_index %d offset %d position "
              "%dx%d",
              row, plane_index, (uint32_t)(pi - vaddr_),
              (uint32_t)(pi - vaddr_) % plane_[plane_index].stride_,
              ((uint32_t)(pi - vaddr_) / plane_[plane_index].stride_ -
               plane_index * (plane_[0].height_ + border_up_ + border_down_)) *
                  (plane_index + 1));
          cnt++;
        }
      }

      // We don't compare the actual data in the buffer with filled_value_
      pi += GetWidthInBytes(plane_[plane_index].width_, pix_fmt_);

      // Check the right padding for corruption
      for (uint32_t col = 0; col < border_right_; ++col) {
        if (*pi++ != filled_value_) {
          ALOGE(
              "corrupted pixel right %d plane_index %d offset %d position "
              "%dx%d",
              col, plane_index, (uint32_t)(pi - vaddr_),
              (uint32_t)(pi - vaddr_) % plane_[plane_index].stride_,
              ((uint32_t)(pi - vaddr_) / plane_[plane_index].stride_ -
               plane_index * (plane_[0].height_ + border_up_ + border_down_)) *
                  (plane_index + 1));
          cnt++;
        }
      }
    }

    // Check the down padding for corruption
    for (uint32_t i = 0;
         i < ((border_down_ / (plane_index + 1)) * plane_[plane_index].stride_);
         ++i) {
      if (*pi++ != filled_value_) {
        ALOGE("corrupted pixel down %d plane_index %d offset %d position %dx%d",
              i, plane_index, (uint32_t)(pi - vaddr_),
              (uint32_t)(pi - vaddr_) % plane_[plane_index].stride_,
              ((uint32_t)(pi - vaddr_) / plane_[plane_index].stride_ -
               plane_index * (plane_[0].height_ + border_up_ + border_down_)) *
                  (plane_index + 1));
        cnt++;
      }
    }
  }

  return (cnt != 0);
}

/** GetWidthInBytes
 *    @width_in_pixels: width in pixels
 *    @pix_fmt: pixel format
 *
 * returns width in bytes based on pixel format
 *
 * return: width in bytes based on pixel format
 **/
uint32_t BufferHandler::GetWidthInBytes(uint32_t width_in_pixels,
                                        PixelFormat pix_fmt) {
  uint32_t rc = width_in_pixels;

  switch (pix_fmt) {
    case kRawBggrMipi8:
    case kRawGbrgMipi8:
    case kRawGrbgMipi8:
    case kRawRggbMipi8:
    case kNv12:
    case kNv12UBWC:
    case kNv21:
    case kNv21UBWC:
    case kNv16:
    case kNv61:
    case kJpeg:
    case kGrey:
      rc = width_in_pixels;
      break;
    case kBgr24:
    case kRgb24:
      rc = width_in_pixels * 3;
      break;
    case kBgrFloat:
    case kRgbFloat:
      rc = width_in_pixels * 3 * sizeof(float);
      break;
    case kRawBggrMipi10:
    case kRawGbrgMipi10:
    case kRawGrbgMipi10:
    case kRawRggbMipi10:
      rc = width_in_pixels * 5 / 4;
      break;
    case kRawBggrMipi12:
    case kRawGbrgMipi12:
    case kRawGrbgMipi12:
    case kRawRggbMipi12:
      rc = width_in_pixels * 3 / 2;
      break;
    case kRawBggr10:
    case kRawGbrg10:
    case kRawGrbg10:
    case kRawRggb10:
    case kRawBggr12:
    case kRawGbrg12:
    case kRawGrbg12:
    case kRawRggb12:
    case kRawBggr16:
    case kRawGbrg16:
    case kRawGrbg16:
    case kRawRggb16:
      rc = width_in_pixels * 2;
      break;
    default:
      std::stringstream err;
      err << "Not supported pixel format " << std::hex << pix_fmt;
      Utils::ThrowException(__func__, err.str());
  }

  return rc;
}

/** GetHeightInLines
 *    @image_height: image height
 *    @pix_fmt: pixel format
 *    @plane: plane
 *
 * returns height in lines based on pixel format and plane
 *
 * return: height in lines based on pixel format and plane
 **/
uint32_t BufferHandler::GetHeightInLines(uint32_t image_height,
                                         PixelFormat pix_fmt, uint32_t plane) {
  uint32_t rc = image_height;

  if (1 < plane) {
    std::string err =
        std::string("Not supported plane index ") + std::to_string(plane);
    Utils::ThrowException(__func__, err);
  }

  switch (pix_fmt) {
    case kNv16:
    case kNv61:
      if (0 == plane) {
        rc = image_height;
      } else {
        rc = image_height / 4;
      }
      break;
    case kRawBggrMipi8:
    case kRawGbrgMipi8:
    case kRawGrbgMipi8:
    case kRawRggbMipi8:
    case kNv12:
    case kNv12UBWC:
    case kNv21:
    case kNv21UBWC:
    case kJpeg:
    case kGrey:
    case kRawBggrMipi10:
    case kRawGbrgMipi10:
    case kRawGrbgMipi10:
    case kRawRggbMipi10:
    case kRawBggrMipi12:
    case kRawGbrgMipi12:
    case kRawGrbgMipi12:
    case kRawRggbMipi12:
    case kRawBggr10:
    case kRawGbrg10:
    case kRawGrbg10:
    case kRawRggb10:
    case kRawBggr12:
    case kRawGbrg12:
    case kRawGrbg12:
    case kRawRggb12:
    case kRawBggr16:
    case kRawGbrg16:
    case kRawGrbg16:
    case kRawRggb16:
    case kBgr24:
    case kRgb24:
    case kBgrFloat:
    case kRgbFloat:
      if (0 == plane) {
        rc = image_height;
      } else {
        rc = image_height / 2;
      }
      break;
    default:
      std::stringstream err;
      err << "Not supported pixel format " << std::hex << pix_fmt;
      Utils::ThrowException(__func__, err.str());
  }

  return rc;
}

/** GetNumPlanes
 *    @pix_fmt: pixel format
 *
 * returns number of planes based on pixel format
 *
 * return: number of planes based on pixel format
 **/
uint32_t BufferHandler::GetNumPlanes(PixelFormat pix_fmt) {
  uint32_t num_planes = 0;

  switch (pix_fmt) {
    case kRawBggrMipi8:
    case kRawGbrgMipi8:
    case kRawGrbgMipi8:
    case kRawRggbMipi8:
      num_planes = 1;
      break;
    case kRawBggrMipi10:
    case kRawGbrgMipi10:
    case kRawGrbgMipi10:
    case kRawRggbMipi10:
      num_planes = 1;
      break;
    case kRawBggrMipi12:
    case kRawGbrgMipi12:
    case kRawGrbgMipi12:
    case kRawRggbMipi12:
      num_planes = 1;
      break;
    case kRawBggr10:
    case kRawGbrg10:
    case kRawGrbg10:
    case kRawRggb10:
    case kRawBggr12:
    case kRawGbrg12:
    case kRawGrbg12:
    case kRawRggb12:
    case kRawBggr16:
    case kRawGbrg16:
    case kRawGrbg16:
    case kRawRggb16:
      num_planes = 1;
      break;
    case kNv12:
    case kNv12UBWC:
    case kNv21:
    case kNv21UBWC:
    case kNv16:
    case kNv61:
      num_planes = 2;
      break;
    case kJpeg:
    case kGrey:
      num_planes = 1;
      break;
    case kBgr24:
    case kRgb24:
    case kBgrFloat:
    case kRgbFloat:
      num_planes = 1;
      break;
    default:
      std::stringstream err;
      err << "Not supported pixel format " << std::hex << pix_fmt;
      Utils::ThrowException(__func__, err.str());
  }

  return num_planes;
}

};  // namespace qmmf_alg_plugin
};  // namespace qmmf
