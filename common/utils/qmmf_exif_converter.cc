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

#include <vector>
#include <cstring>

#include <utils/Errors.h>
#include <utils/Log.h>

#include "qmmf_log.h"
#include "qmmf_exif_converter.h"

namespace qmmf {

using namespace android;

ExifConverter::ExifConverter() {
}

ExifConverter::~ExifConverter() {
}

size_t ExifConverter::getExifTempBuffSize() {
  return kMaxExifApp1Length;
}

status_t ExifConverter::parseIfd(const uint8_t *binary, uint32_t &offset) {
  if (offset >= kMaxExifApp1Length) {
    QMMF_ERROR("%s buffer overflow", __func__);
    return BAD_VALUE;
  }
  uint32_t exif_id;
  qmmf_exif_tag_t tag{};

  uint32_t parsed_tags_count = 0;
  uint32_t tags_count = readU16(binary, offset);
  offset += 2;
  while (parsed_tags_count < tags_count) {
    exif_id = readU16(binary, offset);
    offset += kTagIdSize;
    tag.id = getTagIdByExifId(exif_id);
    tag.entry.type = (qmmf_exif_tag_type_t) readU16(binary, offset);
    offset += kTagTypeSize;
    tag.entry.count = readU32(binary, offset);
    offset += kTagCountSize;
    status_t res = getTagDataByTagType(binary, offset, &tag);
    if (res != NO_ERROR) {
      return BAD_VALUE;
    }
    parsed_tags_count++;
  }
  return NO_ERROR;
}

uint32_t ExifConverter::getTagIdByExifId(uint32_t exif_id) {
  switch (exif_id) {
  case QMMF_ID_ORIENTATION:
    return QMMF_EXIFTAGID_ORIENTATION;
  case QMMF_ID_EXIF_IFD_PTR:
    return QMMF_EXIFTAGID_EXIF_IFD_PTR;
  case QMMF_ID_GPS_IFD_PTR:
    return QMMF_EXIFTAGID_GPS_IFD_PTR;
  case QMMF_ID_EXPOSURE_TIME:
    return QMMF_EXIFTAGID_EXPOSURE_TIME;
  case QMMF_ID_F_NUMBER:
    return QMMF_EXIFTAGID_F_NUMBER;
  case QMMF_ID_ISO_SPEED_RATING:
    return QMMF_EXIFTAGID_ISO_SPEED_RATING;
  case QMMF_ID_APERTURE:
    return QMMF_EXIFTAGID_APERTURE;
  case QMMF_ID_FOCAL_LENGTH:
    return QMMF_EXIFTAGID_FOCAL_LENGTH;
  case QMMF_ID_EXIF_PIXEL_X_DIMENSION:
    return QMMF_EXIFTAGID_EXIF_PIXEL_X_DIMENSION;
  case QMMF_ID_EXIF_PIXEL_Y_DIMENSION:
    return QMMF_EXIFTAGID_EXIF_PIXEL_Y_DIMENSION;
  case QMMF_ID_INTEROP_IFD_PTR:
    return QMMF_EXIFTAGID_INTEROP_IFD_PTR;
  case QMMF_CONSTRUCT_TAGID(QMMF_EXIF_TAG_MAX_OFFSET, 0x0001):
    return QMMF_CONSTRUCT_TAGID(QMMF_EXIF_TAG_MAX_OFFSET, 0x0001);
  case QMMF_CONSTRUCT_TAGID(QMMF_EXIF_TAG_MAX_OFFSET, 0x0002):
    return QMMF_CONSTRUCT_TAGID(QMMF_EXIF_TAG_MAX_OFFSET, 0x0002);
  case QMMF_ID_WHITE_BALANCE:
    return QMMF_EXIFTAGID_WHITE_BALANCE;
  case QMMF_ID_EXPOSURE_MODE:
    return QMMF_EXIFTAGID_EXPOSURE_MODE;
  case QMMF_ID_DATE_TIME:
    return QMMF_EXIFTAGID_DATE_TIME;
  case QMMF_ID_EXIF_DATE_TIME_ORIGINAL:
    return QMMF_EXIFTAGID_EXIF_DATE_TIME_ORIGINAL;
  case QMMF_ID_EXIF_DATE_TIME_DIGITIZED:
    return QMMF_EXIFTAGID_EXIF_DATE_TIME_DIGITIZED;
  case QMMF_ID_SUBSEC_TIME:
    return QMMF_EXIFTAGID_SUBSEC_TIME;
  case QMMF_ID_SUBSEC_TIME_ORIGINAL:
    return QMMF_EXIFTAGID_SUBSEC_TIME_ORIGINAL;
  case QMMF_ID_SUBSEC_TIME_DIGITIZED:
    return QMMF_EXIFTAGID_SUBSEC_TIME_DIGITIZED;
  case QMMF_ID_MAKE:
    return QMMF_EXIFTAGID_MAKE;
  case QMMF_ID_MODEL:
    return QMMF_EXIFTAGID_MODEL;
  case QMMF_ID_SOFTWARE:
    return QMMF_EXIFTAGID_SOFTWARE;
  case QMMF_ID_GPS_LATITUDE_REF:
    return QMMF_EXIFTAGID_GPS_LATITUDE_REF;
  case QMMF_ID_GPS_LATITUDE:
    return QMMF_EXIFTAGID_GPS_LATITUDE;
  case QMMF_ID_GPS_LONGITUDE_REF:
    return QMMF_EXIFTAGID_GPS_LONGITUDE_REF;
  case QMMF_ID_GPS_LONGITUDE:
    return QMMF_EXIFTAGID_GPS_LONGITUDE;
  case QMMF_ID_GPS_ALTITUDE_REF:
    return QMMF_EXIFTAGID_GPS_ALTITUDE_REF;
  case QMMF_ID_GPS_ALTITUDE:
    return QMMF_EXIFTAGID_GPS_ALTITUDE;
  case QMMF_ID_GPS_TIMESTAMP:
    return QMMF_EXIFTAGID_GPS_TIMESTAMP;
  case QMMF_ID_GPS_DATESTAMP:
    return QMMF_EXIFTAGID_GPS_DATESTAMP;
  case QMMF_ID_GPS_PROCESSINGMETHOD:
    return QMMF_EXIFTAGID_GPS_PROCESSINGMETHOD;
  default: {
    QMMF_ERROR("%s Unexpected exifId: %d", __func__, exif_id);
    return 0;
  }
  }
}

status_t ExifConverter::convertExifBinaryToExifInfoStruct(const uint8_t *binary) {
  if (binary == nullptr) {
    QMMF_ERROR("%s No exif buffer found,", __func__);
    return BAD_VALUE;
  }
  exif_entities_.clear();
  exif_entities_.resize(kMaxExifEntries);
  exif_ifd_ptr_offset_ = 0;
  interop_ifd_ptr_offset_ = 0;
  gps_ifd_ptr_offset_ = 0;
  tiff_header_offset_ = 0;
  uint32_t offset = 0;

  uint32_t tmp = readU16(binary, offset);
  if (tmp == (0xFF00 | APP1_MARKER)) {
    offset += 2;
  } else {
    QMMF_ERROR("%s Error: APP1 marker not found in exif section!",
        __func__);
    return BAD_VALUE;
  }

  tmp = readU16(binary, offset);
  offset += 2;
  QMMF_ERROR("%s Exif section size : %d", __func__, tmp);

  tmp = readU32(binary, offset);
  if (tmp == EXIF_HEADER) {
    /* Offset for EXIF_HEADER + 2 bytes that seperate it from TIFF header*/
    offset += 6;
  } else {
    QMMF_ERROR("%s Error: EXIF_HEADER marker not found in exif section!",
        __func__);
    return BAD_VALUE;
  }
  tiff_header_offset_ = offset;

  tmp = readU16(binary, offset);
  if (tmp == TIFF_BIG_ENDIAN) {
      offset += 2;
  } else {
    QMMF_ERROR("%s Error: TIFF_BIG_ENDIAN marker not found in exif section",
        __func__);
    return BAD_VALUE;
  }

  tmp = readU16(binary, offset);
  if (tmp == TIFF_HEADER) {
    offset += 2;
  } else {
    QMMF_ERROR("%s Error: TIFF_HEADER marker not found in exif section!",
        __func__);
    return BAD_VALUE;
  }

  tmp = readU32(binary, offset);
  if (tmp == ZERO_IFD_OFFSET) {
    offset += 4;
  } else {
    QMMF_ERROR("%s Error: ZERO_IFD_OFFSET not found in exif section!",
        __func__);
    return BAD_VALUE;
  }

  /* Parse 0th IFD*/
  status_t res = parseIfd(binary, offset);
  if (res != NO_ERROR) {
    return BAD_VALUE;
  }
  /* Parse Exif IFD*/
  offset = exif_ifd_ptr_offset_;
  res = parseIfd(binary, offset);
  if (res != NO_ERROR) {
    return BAD_VALUE;
  }
  /* Parse Gps IFD*/
  offset = gps_ifd_ptr_offset_;
  res = parseIfd(binary, offset);
  if (res != NO_ERROR) {
    return BAD_VALUE;
  }
  return NO_ERROR;
}

size_t ExifConverter::getExifEntitiesSize() {
  return exif_entities_.size();
}

void* ExifConverter::getExifEntitiesData() {
  return (void*)(&exif_entities_[0]);
}

status_t ExifConverter::getTagDataByTagType(const uint8_t *binary,
                                         uint32_t &offset,
                                         qmmf_exif_tag_t *tag) {
  if (offset >= kMaxExifApp1Length || tag == nullptr) {
    QMMF_ERROR("%s buffer overflow", __func__);
    return BAD_VALUE;
  }
  status_t res = NO_ERROR;
  uint32_t tag_data_offset;

  switch (tag->entry.type) {
  case QMMF_EXIF_SHORT:
    tag->entry.data._short = readU16(binary, offset);
    constructExifTag(tag->id, tag->entry.count, tag->entry.type,
        &tag->entry.data._short);
    offset += kTagDataSize;
    break;
  case QMMF_EXIF_LONG:
    tag->entry.data._long = readU32(binary, offset);
    if (tag->id == QMMF_EXIFTAGID_EXIF_IFD_PTR) {
      exif_ifd_ptr_offset_ = tag->entry.data._long + 10;
    } else if (tag->id == QMMF_EXIFTAGID_GPS_IFD_PTR) {
      gps_ifd_ptr_offset_ = tag->entry.data._long + 10;
    } else if (tag->id == QMMF_EXIFTAGID_INTEROP_IFD_PTR) {
      interop_ifd_ptr_offset_ = tag->entry.data._long + 10;
    } else {
      constructExifTag(tag->id, tag->entry.count, tag->entry.type,
        &tag->entry.data._long);
    }
    offset += kTagDataSize;
    break;
  case QMMF_EXIF_RATIONAL:
    tag_data_offset = readU32(binary, offset);
    tag_data_offset += tiff_header_offset_;
    if (tag->entry.count > 1) {
      tag->entry.data._rats = new qmmf_exif_rat_t[tag->entry.count];
      for (uint32_t i = 0; i < tag->entry.count; i++) {
        tag->entry.data._rats[i].num = readU32(binary, tag_data_offset);
        tag_data_offset += kTagDataSize;
        tag->entry.data._rats[i].denom = readU32(binary, tag_data_offset);
        tag_data_offset += kTagDataSize;
      }
      constructExifTag(tag->id, tag->entry.count, tag->entry.type,
        tag->entry.data._rats);
    } else {
      tag->entry.data._rat.num = readU32(binary, tag_data_offset);
      tag_data_offset += kTagDataSize;
      tag->entry.data._rat.denom = readU32(binary, tag_data_offset);
      constructExifTag(tag->id, tag->entry.count, tag->entry.type,
          &tag->entry.data._rat);
    }
    offset += kTagDataSize;
    break;
  case QMMF_EXIF_ASCII:
    if (tag->entry.count <= 4) {
      /* QTI encoder requires allocation even for ASCII symbols smaller
       * than 4 bytes. This is not aligned with the exif standard, but
       * we dont want to change the encoder code.
       * So only option is to keep this workaround inplace */
      tag->entry.data._ascii = new char[tag->entry.count];
      memcpy(tag->entry.data._ascii, binary + offset, tag->entry.count);
    } else {
      tag_data_offset = readU32(binary, offset);
      tag_data_offset += tiff_header_offset_;
      tag->entry.data._ascii = new char[tag->entry.count];
      memcpy(tag->entry.data._ascii, binary + tag_data_offset, tag->entry.count);
    }
    constructExifTag(tag->id, tag->entry.count, tag->entry.type,
        tag->entry.data._ascii);
    offset += kTagDataSize;
    break;
  case QMMF_EXIF_BYTE:
    tag->entry.data._byte = (uint8_t) ((uint8_t) binary[offset] << 8);
    constructExifTag(tag->id, tag->entry.count, tag->entry.type,
        &tag->entry.data._byte);
    offset += kTagDataSize;
    break;
  case QMMF_EXIF_UNDEFINED:
  case QMMF_EXIF_SLONG:
  case QMMF_EXIF_SRATIONAL:
    QMMF_ERROR("%s There should not be a tag with this type!",
        __func__);
    res = BAD_VALUE;
    break;
  default:
    QMMF_ERROR("%s Unknown tag type.", __func__);
    res = BAD_VALUE;
    break;
  }
  return res;
}

uint32_t ExifConverter::readU32(const uint8_t *buffer, uint32_t offset) {
  return (uint32_t) (((uint32_t) buffer[offset] << 24)
      + ((uint32_t) buffer[offset + 1] << 16)
      + ((uint32_t) buffer[offset + 2] << 8)
      + (uint32_t) buffer[offset + 3]);
}

uint16_t ExifConverter::readU16(const uint8_t *buffer, uint32_t offset) {
  return (uint16_t) (((uint16_t) buffer[offset] << 8)
      + (uint16_t) buffer[offset + 1]);
}

void ExifConverter::constructExifTag(uint32_t id, uint32_t count,
    uint16_t type, uint8_t *data) {
  qmmf_exif_tag_t tag{};
  tag.id = id;
  tag.entry.type = (qmmf_exif_tag_type_t) type;
  tag.entry.count = count;
  tag.entry.data._byte = *data;
  exif_entities_.push_back(tag);
}

void ExifConverter::constructExifTag(uint32_t id, uint32_t count,
    uint16_t type, uint16_t *data) {
  qmmf_exif_tag_t tag{};
  tag.id = id;
  tag.entry.type = (qmmf_exif_tag_type_t) type;
  tag.entry.count = count;
  tag.entry.data._short = *data;
  exif_entities_.push_back(tag);
}

void ExifConverter::constructExifTag(uint32_t id, uint32_t count,
                                  uint16_t type, uint32_t *data) {
  qmmf_exif_tag_t tag{};
  tag.id = id;
  tag.entry.type = (qmmf_exif_tag_type_t) type;
  tag.entry.count = count;
  tag.entry.data._long = *data;
  exif_entities_.push_back(tag);
}

void ExifConverter::constructExifTag(uint32_t id, uint32_t count,
                                  uint16_t type, qmmf_exif_rat_t *data) {
  qmmf_exif_tag_t tag{};
  tag.id = id;
  tag.entry.type = (qmmf_exif_tag_type_t) type;
  tag.entry.count = count;
  if (count > 1) {
    tag.entry.data._rats = data;
  } else {
    tag.entry.data._rat = *data;
  }
  exif_entities_.push_back(tag);
}

void ExifConverter::constructExifTag(uint32_t id, uint32_t count,
                                  uint16_t type, char *data) {
  qmmf_exif_tag_t tag{};
  tag.id = id;
  tag.entry.type = (qmmf_exif_tag_type_t) type;
  tag.entry.count = count;
  tag.entry.data._ascii = data;
  exif_entities_.push_back(tag);
}

};  //namespace qmmf
