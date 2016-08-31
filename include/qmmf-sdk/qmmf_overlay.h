/*
* Copyright (c) 2016, The Linux Foundation. All rights reserved.
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

#include <sys/mman.h>
#include <fcntl.h>
#include <utils/Mutex.h>
#include <utils/String8.h>
#include <sys/types.h>
#include <utils/RefBase.h>
#include <utils/KeyedVector.h>

namespace qmmf {

namespace overlay {

#define MAX_STRING_LENGTH 128

using namespace android;

enum class OverlayType {
  kDateType,
  kUserText,
  kStaticImage,
  kBoundingBox,
  kPrivacyMask
};

enum class OverlayLocationType {
  kTopLeft,
  kTopRight,
  kCenter,
  kBottomLeft,
  kBottomRight,
  kNone
};

enum class OverlayTimeType {
  kHHMMSS_24HR,
  kHHMMSS_AMPM,
  kHHMM_24HR,
  kHHMM_AMPM
};

enum class OverlayDateType { kYYYYMMDD, kMMDDYYYY };

struct OverlayDateTimeType {
  OverlayTimeType time_type;
  OverlayDateType date_type;
};

struct BoundingBox {
  int32_t startX;
  int32_t startY;
  int32_t width;
  int32_t height;
  char box_name[MAX_STRING_LENGTH];
};

struct OverlayImageInfo {
  char image_location[MAX_STRING_LENGTH];
  int32_t width;
  int32_t height;
};

struct OverlayParam {
  OverlayType type;
  OverlayLocationType location;
  uint32_t text_color;
  union {
    OverlayDateTimeType date_time_type;
    char user_text[MAX_STRING_LENGTH];
    OverlayImageInfo image_info;
    BoundingBox bounding_box;
  };
};

typedef struct PrivacyMask {
    int32_t       startX;
    int32_t       startY;
    int32_t       width;
    int32_t       height;
} PrivacyMask;

enum class BufFormat {
    FORMAT_YUV_NV12,
    FORMAT_YUV_NV21,
    FORMAT_RGB_888,
    FORMAT_RGBA_8888,
};

typedef struct TargetBuf {
    BufFormat format;
    uint32_t  width;
    uint32_t  height;
    uint32_t  ionFd;
    uint32_t  frameLen;
} TargetBuf;

class OverlayItem;

/*
This class provides facility to embed different
Kinds of overlay on topof Camera stream buffers.
*/
class Overlay {

public:
    Overlay();

   ~Overlay();

    /**
    Initialise overlay with format of buffer.
    */
    int32_t init(const BufFormat& format);

    /**
    Create overlay item of type static image, date/time, bounding box,
    simple text, or privacy mask. this Api provides overlay item id which
    can be use for further configurartion change to item.
    */
    int32_t createOverlayItem(OverlayParam& param, uint32_t* overlayId);

    /**
    Overlay item can be deleted at any point of time after creation.
    */
    int32_t deleteOverlayItem(uint32_t overlayId);

    /**
    Overlay item's parameters can be queried using this Api, it is recommended
    to call get parameters first before setting new parameters using Api
    updateOverlayItem.
    */
    int32_t getOverlayParams(uint32_t overlayId, OverlayParam& param);

    /**
    Overlay item's configuration can be change at run time using this Api.
    user has to provide overlay Id and updated parameters.
    */
    int32_t updateOverlayParams(uint32_t overlayId, OverlayParam& param);

    /**
    Overlay Item can be enable/disable at run time.
    */
    int32_t enableOverlayItem(uint32_t overlayId);
    int32_t disableOverlayItem(uint32_t overlayId);

    /**
    provide input YUV buffer to apply overlay.
    */
    int32_t applyOverlay(const TargetBuf& buffer);

private:

    uint32_t getC2dColorFormat(const BufFormat& format);

    bool isOverlayItemValid(uint32_t overlayId);

    DefaultKeyedVector<uint32_t, sp<OverlayItem> > mOverlayItems;

    uint32_t     mFrameWidth;
    uint32_t     mFrameHeight;
    uint32_t     mTargetC2dSurfaceId;
    int32_t      mIonDevice;
    uint8_t      mNumActiveOverlays;
    uint32_t     mId;
    Mutex        mLock;
};

}; // namespace overlay
}; // namespace qmmf