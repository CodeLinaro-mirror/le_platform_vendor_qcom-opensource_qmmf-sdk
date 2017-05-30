/*
* Copyright (c) 2017, The Linux Foundation. All rights reserved.
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

#include "qmmf_video_track_extra_param.h"

namespace qmmf {

namespace recorder {

enum ParamTag {
  QMMF_SOURCE_SURFACE_PARAM = (1 << 16),
  QMMF_SURFACE_CROP,
  QMMF_MULTICAM_SURFACE_PARAM,
  QMMF_SURFACE_PLACEMENT, // TODO: Not implemented. Do not use!
};

enum class TransformFlags {
  kNone      = 0,
  kHFlip     = (1 << 0),
  kVFlip     = (1 << 1),
  kRotate90  = (1 << 2),
  kRotate180 = (1 << 3),
  kRotate270 = (1 << 4),
};

enum class StitchingMode {
  // Input frames/images are timestamp synchronized but no stitching is
  // applied on them and are passed as separate outputs to the upper layers.
  kNone,
  // Input frames/images are timestamp synchronized and passed to a 360
  // stitching library in which the 1st input surface is placed on the left
  // while each subsequent surface is placed next to it on the right and a
  // 360 algorithm is applied.
  k360Default,
  // Input frames/images are timestamp synchronized and passed to a 360
  // stitching library in which the 1st input surface content will be placed
  // in the center of the equirectangular output, and the second surface is
  // split into 2 halves and a 360 algorithm is applied.
  k360Trifold,
  // Input frames/images are timestamp synchronized and passed to a library
  // which produces one frame/image containing both inputs stitched in a row,
  // next to each other from left to right, as they are.
  kSideBySideRow,
  // Input frames/images are timestamp synchronized and passed to a library
  // which produces one frame/image containing both inputs stitched in a
  // column from top to bottom, as they are.
  kSideBySideColumn,
  // Input frames/images are timestamp synchronized and passed to a library in
  // which the scale and position of each input surface is determined by the
  // client via the QMMF_SURFACE_PLACEMENT tag structure.
  // TODO: Not implemented. Do not use!
  kCustomComposition
};

struct SourceSurfaceParam : DataTagBase {
  // ID of the camera whose surface dimensions will be set.
  int32_t camera_id;    // Default: -1
  // Width in pixels of the source surface.
  uint32_t width;       // Default: 0
  // Height in pixels of the source surface.
  uint32_t height;      // Default: 0
  // Transformations that will be applied on the source surface.
  TransformFlags flags; // Default: TransformFlags::kNone

  SourceSurfaceParam()
    : DataTagBase(QMMF_SOURCE_SURFACE_PARAM),
      camera_id(-1), width(0), height(0), flags(TransformFlags::kNone) {}
};

struct SurfaceCrop : DataTagBase {
  int32_t camera_id;  // Default: -1
  // Y-axis coordinate of the crop rectangle top left starting point.
  // The coordinate system begins from the top left corner of the source.
  uint32_t x;         // Default: 0
  // X-axis coordinate of the crop rectangle top left starting point.
  // The coordinate system begins from the top left corner of the source.
  uint32_t y;         // Default: 0
  // Width in pixels of the crop rectangle.
  uint32_t width;     // Default: 0
  // Height in pixels of the crop rectangle.
  uint32_t height;    // Default: 0

  SurfaceCrop()
    : DataTagBase(QMMF_SURFACE_CROP),
      camera_id(-1), x(0), y(0), width(0), height(0) {}
};

struct MultiCamSurfaceParam : DataTagBase {
  StitchingMode mode;   // Default: StitchingMode::k360DefaultStitch
  TransformFlags flags; // Default: TransformFlags::kNone

  MultiCamSurfaceParam()
      : DataTagBase(QMMF_MULTICAM_SURFACE_PARAM),
        mode(StitchingMode::k360Default),
        flags(TransformFlags::kNone) {}
};

// TODO: Not implemented. Do not use!
struct SurfacePlacement : DataTagBase {
  int32_t camera_id;    // Default: -1
  uint32_t x;           // Default: 0
  uint32_t y;           // Default: 0.
  uint32_t width;       // Default: 0
  uint32_t height;      // Default: 0

  SurfacePlacement()
    : DataTagBase(QMMF_SURFACE_PLACEMENT),
      camera_id(-1), x(0), y(0), width(0), height(0) {}
};

}; //namespace recorder.

}; //namespace qmmf.
