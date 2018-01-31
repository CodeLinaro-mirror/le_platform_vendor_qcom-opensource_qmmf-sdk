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

#include <cstdint>
#include <cerrno>
#include <cassert>
#include <chrono>
#include <mutex>

#include "common/exif-generator/qmmf_exif_base_types.h"

namespace qmmf {

typedef int32_t status_t;

class IExifConverter {
 public:
  virtual ~IExifConverter() {};
  virtual status_t convertExifBinaryToExifInfoStruct(const uint8_t *binary) = 0;
  virtual size_t getExifEntitiesSize() = 0;
  virtual void* getExifEntitiesData() = 0;
  virtual size_t getExifTempBuffSize() = 0;
};


class ExifConverter : public IExifConverter {

 public:

  ExifConverter();

  ~ExifConverter();

  status_t convertExifBinaryToExifInfoStruct(const uint8_t *binary) override;

  size_t getExifEntitiesSize() override;

  void* getExifEntitiesData() override;

  size_t getExifTempBuffSize() override;

 private:

  /**
   * Get tag's data depending on tag's type. When tag's data is larger than 4
   * bytes the tag data field contains a value that is the offset to where the
   * tag data actually is located in the buffer.
   */
  status_t getTagDataByTagType(const uint8_t *binary, uint32_t &offset,
                               qmmf_exif_tag_t *tag);

  uint32_t readU32(const uint8_t *buffer, uint32_t offset);
  uint16_t readU16(const uint8_t *buffer, uint32_t offset);
  void constructExifTag(uint32_t id, uint32_t count, uint16_t type,
                        uint8_t *data);
  void constructExifTag(uint32_t id, uint32_t count, uint16_t type,
                        uint16_t *data);
  void constructExifTag(uint32_t id, uint32_t count, uint16_t type,
                        uint32_t *data);
  void constructExifTag(uint32_t id, uint32_t count, uint16_t type,
                        qmmf_exif_rat_t *data);
  void constructExifTag(uint32_t id, uint32_t count, uint16_t type,
                        char *data);
  uint32_t getTagIdByExifId(uint32_t exif_id);

  status_t parseIfd(const uint8_t *binary, uint32_t &offset);

  std::vector<qmmf_exif_tag_t> exif_entities_;
  uint32_t  exif_ifd_ptr_offset_;
  uint32_t  interop_ifd_ptr_offset_;
  uint32_t  gps_ifd_ptr_offset_;
  uint32_t  tiff_header_offset_;
  static const uint32_t kMaxExifApp1Length = 0xFFFF;
  static const uint32_t kTagSize = 12;
  static const uint32_t kTagDataSize = 4;
  static const uint32_t kMaxExifEntries = 23;
  static const uint32_t kTagIdSize = 2;
  static const uint16_t kTagTypeSize = 2;
  static const uint32_t kTagCountSize = 4;

};

};  // namespace qmmf.
