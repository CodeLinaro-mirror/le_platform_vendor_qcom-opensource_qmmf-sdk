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

#define LOG_TAG "QmmfAlgoConfigurationBuffer"

#include <type_traits>

#include <qmmf-alg/qmmf_alg_utils.h>

#include "qmmf_algo_gtest_configuration_buffer.h"
#include "qmmf_json_helper.h"

namespace qmmf {
namespace qmmf_alg_plugin {

const std::map<std::string, PixelFormat>
    QmmfAlgoConfigurationBuffer::kPixelFormatFromString{
        {"RawBggrMipi8", kRawBggrMipi8},
        {"RawGbrgMipi8", kRawGbrgMipi8},
        {"RawGrbgMipi8", kRawGrbgMipi8},
        {"RawRggbMipi8", kRawRggbMipi8},
        {"RawBggrMipi10", kRawBggrMipi10},
        {"RawGbrgMipi10", kRawGbrgMipi10},
        {"RawGrbgMipi10", kRawGrbgMipi10},
        {"RawRggbMipi10", kRawRggbMipi10},
        {"RawBggrMipi12", kRawBggrMipi12},
        {"RawGbrgMipi12", kRawGbrgMipi12},
        {"RawGrbgMipi12", kRawGrbgMipi12},
        {"RawRggbMipi12", kRawRggbMipi12},
        {"RawBggr10", kRawBggr10},
        {"RawGbrg10", kRawGbrg10},
        {"RawGrbg10", kRawGrbg10},
        {"RawRggb10", kRawRggb10},
        {"RawBggr12", kRawBggr12},
        {"RawGbrg12", kRawGbrg12},
        {"RawGrbg12", kRawGrbg12},
        {"RawRggb12", kRawRggb12},
        {"RawBggr16", kRawBggr16},
        {"RawGbrg16", kRawGbrg16},
        {"RawGrbg16", kRawGrbg16},
        {"RawRggb16", kRawRggb16},
        {"Yuyv422i", kYuyv422i},
        {"Yvyu422i", kYvyu422i},
        {"Uyvy422i", kUyvy422i},
        {"Vyuy422i", kVyuy422i},
        {"Nv12", kNv12},
        {"Nv12UBWC", kNv12UBWC},
        {"Nv21", kNv21},
        {"Nv21UBWC", kNv21UBWC},
        {"Nv16", kNv16},
        {"Nv61", kNv61},
        {"Yuv420", kYuv420},
        {"Yvu420", kYvu420},
        {"Yuv420p", kYuv420p},
        {"Yvu420p", kYvu420p},
        {"Rgb444", kRgb444},
        {"Argb444", kArgb444},
        {"Xrgb444", kXrgb444},
        {"Rgb555", kRgb555},
        {"Argb555", kArgb555},
        {"Xrgb555", kXrgb555},
        {"Rgb565", kRgb565},
        {"Rgb555x", kRgb555x},
        {"Argb555x", kArgb555},
        {"Xrgb555x", kXrgb555},
        {"Rgb565x", kRgb565x},
        {"Bgr666", kBgr666},
        {"Bgr24", kBgr24},
        {"Rgb24", kRgb24},
        {"Bgr32", kBgr32},
        {"Abgr32", kAbgr32},
        {"Xbgr32", kXbgr32},
        {"Rgb32", kRgb32},
        {"Argb32", kArgb32},
        {"Xrgb32", kXrgb32},
        {"BgrFloat", kBgrFloat},
        {"RgbFloat", kRgbFloat},
        {"Jpeg", kJpeg},
        {"Grey", kGrey},
        {"MeshNormFloat", kMeshNormFloat},
    };

/** QmmfAlgoConfigurationBuffer
 *    @v: parsed json configuration
 *
 * creates new instance of QmmfAlgoConfigurationBuffer
 *
 * return: void
 **/
QmmfAlgoConfigurationBuffer::QmmfAlgoConfigurationBuffer(Json::Value &v)
    : limit_byte_size_(0), max_limit_value_(0) {
  FromJson(v);
}

/** FromJson
 *    @r: parsed json value
 *
 * fills buffer configuration
 *
 * return: void
 **/
void QmmfAlgoConfigurationBuffer::FromJson(Json::Value &v) {
  QmmfJsonHelper h(v);
  h.Get("width", width_);
  h.Get("height", height_);
  h.Get("stride", stride_);
  h.Get("scanline", scanline_);
  h.Get("pixel format", pixel_format_, kPixelFormatFromString);
  h.Get("input file name", input_file_name_, false);
  h.Get("output file name", output_file_name_, false);
  h.Get("heap buffer", heap_buffer_);
  h.Get("limit byte size", limit_byte_size_, false);
  h.Get("maximum limit value", max_limit_value_, false);
}

/** New
 *    @v: parsed json configuration
 *
 * returns new instance of QmmfAlgoConfigurationBuffer
 *
 * return: new instance of QmmfAlgoConfigurationBuffer
 **/
std::shared_ptr<QmmfAlgoConfigurationBuffer> QmmfAlgoConfigurationBuffer::New(
    Json::Value &v) {
  std::shared_ptr<QmmfAlgoConfigurationBuffer> new_instance(
      new QmmfAlgoConfigurationBuffer(v));
  return new_instance;
}

};  // namespace qmmf_alg_plugin
};  // namespace qmmf
