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
#include <linux/msm_kgsl.h>
#include <linux/msm_ion.h>
#include <adreno/c2d2.h>
#if USE_SKIA
#include <SkCanvas.h>
#endif



/**
OVDBG_INFO, ERROR and WARN logs are enabled all the time by default.
*/
#define OVDBG_INFO(fmt, args...)  ALOGD(fmt, ##args)
#define OVDBG_ERROR(fmt, args...) ALOGE(fmt, ##args)
#define OVDBG_WARN(fmt, args...)  ALOGW(fmt, ##args)

/**
OVDBG_LEVEL1 and LEVEL2 logs will be enabled based on
property value "persist.overlay.debug.level"
level = 1 CCDBG_LEVEL1
level = 2 CCDBG_LEVEL2
*/
#define OVDBG_LEVEL1(fmt, args...)  ALOGD(fmt, ##args)
#define OVDBG_LEVEL2(fmt, args...)  ALOGD(fmt, ##args)

namespace qmmf {

namespace overlay {

#define OVERLAYITEM_X_MARGIN_PERCENT  0.5
#define OVERLAYITEM_Y_MARGIN_PERCENT  0.5
#define MAX_LEN       128
#define MAX_OVERLAYS  10
//#define DEBUG_BACKGROUND_SURFACE

using namespace android;

enum class OverlayType {
    OVERLAYTYPE_DATE_TIME,
    OVERLAYTYPE_USERTEXT,
    OVERLAYTYPE_STATICIMAGE,
    OVERLAYTYPE_BOUNDINGBOX,
    OVERLAYTYPE_PRIVACYMASK,
};

enum class OverlayLocationType {
    OVERLAYLOCATIONTYPE_TOPLEFT,
    OVERLAYLOCATIONTYPE_TOPRIGHT,
    OVERLAYLOCATIONTYPE_CENTER,
    OVERLAYLOCATIONTYPE_BOTTOMLEFT,
    OVERLAYLOCATIONTYPE_BOTTOMRIGHT,
    OVERLAYLOCATIONTYPE_NONE,
};

enum class OverlayTimeType {
    OVERLAYTIMETYPE_HHMMSS_24HR,
    OVERLAYTIMETYPE_HHMMSS_AMPM,
    OVERLAYTIMETYPE_HHMM_24HR,
    OVERLAYTIMETYPE_HHMM_AMPM
};

enum class OverlayDateType {
    OVERLAYDATETYPE_YYYYMMDD,
    OVERLAYDATETYPE_MMDDYYYY
};

typedef struct OverlayDateAndTimeType {
    OverlayTimeType timeType;
    OverlayDateType dateType;
} OverlayDateAndTimeType;

typedef struct BoundingBox {
    int32_t       startX;
    int32_t       startY;
    int32_t       width;
    int32_t       height;
    char          boxName[MAX_LEN];
} BoundingBox;

typedef struct ImageInfo {
    char       imageLocation[MAX_LEN];
    int32_t    width;
    int32_t    height;
} ImageInfo;

typedef struct PrivacyMask{
    int32_t       startX;
    int32_t       startY;
    int32_t       width;
    int32_t       height;
} PrivacyMask;

typedef struct OverlayItemParam {
    OverlayType         type;
    OverlayLocationType location;
    uint32_t            textColor;
    union {
        OverlayDateAndTimeType dateAndTimeType;
        char                   userText[MAX_LEN];
        ImageInfo              imageInfo;
        BoundingBox            boundingBox;
        PrivacyMask            privacyMask;
    };
} OverlayItemParam;

enum class BufferFormat {
    FORMAT_YUV_NV12,
    FORMAT_YUV_NV21,
    FORMAT_RGB_888,
    FORMAT_RGBA_8888,
};

typedef struct TargetBuf {
    BufferFormat format;
    uint32_t      width;
    uint32_t      height;
    uint32_t      ionFd;
    uint32_t      frameLen;
} TargetBuf;

class OverlayItem;

/*
This class provides facility to embed different
Kinds of overlay on topof Camera stream buffers.
*/
class QIPCamOverlay {

public:
    QIPCamOverlay();

   ~QIPCamOverlay();

    /**
    Initialise overlay with format of buffer.
    */
    int32_t init(BufferFormat format);

    /**
    Create overlay item of type static image, date/time, bounding box,
    simple text, or privacy mask. this Api provides overlay item id which
    can be use for further configurartion change to item.
    */
    int32_t createOverlayItem(OverlayItemParam& param, uint8_t* overlayId);

    /**
    Overlay item can be deleted at any point of time after creation.
    */
    int32_t deleteOverlayItem(uint32_t overlayId);

    /**
    Overlay item's parameters can be queried using this Api, it is recommended
    to call get parameters first before setting new parameters using Api
    updateOverlayItem.
    */
    int32_t getOverlayItemParams(uint32_t overlayId, OverlayItemParam& param);

    /**
    Overlay item's configuration can be change at run time using this Api.
    user has to provide overlay Id and updated parameters.
    */
    int32_t updateOverlayItemParams(uint32_t overlayId, OverlayItemParam& param);

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

    uint32_t getC2dColorFormat(const BufferFormat& format);

    bool isOverlayItemValid(uint32_t overlayId);

    DefaultKeyedVector<uint8_t, sp<OverlayItem> > mOverlayItems;

    C2D_OBJECT   mC2dObjects[MAX_OVERLAYS];
    uint32_t     mFrameWidth;
    uint32_t     mFrameHeight;
    uint32_t     mTargetC2dSurfaceId;
    int32_t      mIonDevice;
    uint8_t      mNumActiveOverlays;
    uint32_t     mId;
    Mutex        mLock;
};

typedef struct DrawInfo {
    uint32_t width;
    uint32_t height;
    uint32_t x;
    uint32_t y;
    uint32_t c2dSurfaceId;
} DrawInfo;

//Base class for all types of overlays.
class OverlayItem : public RefBase
{
public:

    OverlayItem(int32_t ionDeviceId);

    ~OverlayItem();

    virtual int32_t init(OverlayItemParam& param) = 0 ;

    virtual int32_t updateAndDraw() = 0;

    virtual void getDrawInfo(uint32_t targetWidth, uint32_t targetHeight,
        DrawInfo* drawInfo) = 0 ;

    virtual void getParameters(OverlayItemParam& param) = 0;

    virtual int32_t updateParameters(OverlayItemParam& param) = 0;

    OverlayType& getItemType() {return mType; }

    void markDirty(bool dirty);

    void activate(bool value);

    bool isActive() { return mIsActive; }

protected:

    struct ionMemInfo {
        uint32_t               size;
        int32_t                fd;
        void *                 vaddr;
        struct ion_handle_data handleData;
    };

    int32_t allocateIonMemory(ionMemInfo& memInfo, uint32_t size);

    int32_t                mX;
    int32_t                mY;
    uint32_t               mWidth;
    uint32_t               mHeight;
    uint32_t               mC2dSurfaceId;
    void *                 mGpuAddr;
    void *                 mVaddr;
    int32_t                mIonFd;
    uint32_t               mSize;
    struct ion_handle_data mHandleData;
    OverlayLocationType    mLocationType;
    bool                   mDirty;
    int32_t                mIonDevice;
    OverlayType            mType;

private:
    bool                   mIsActive;
};

class OverlayItemStaticImage : public OverlayItem
{

public:
    OverlayItemStaticImage(int32_t ionDeviceId);

    virtual ~OverlayItemStaticImage();

    int32_t init(OverlayItemParam& param) override;

    int32_t updateAndDraw() override;

    void getDrawInfo(uint32_t targetWidth, uint32_t targetHeight,
        DrawInfo* drawInfo) override;

    void getParameters(OverlayItemParam& param) override;

    int32_t updateParameters(OverlayItemParam& param) override;
private:
    int32_t createSurface();

    String8 mImagePath;
};

#define DATETIME_TEXT_BUF_WIDTH        240
#define DATETIME_TEXT_BUF_HEIGHT       135
#define DATETIME_TARGET_WIDTH_PERCENT   10
#define DATETIME_TARGET_HEIGHT_PERCENT  10
#define DATETIME_PIXEL_SIZE             42

class OverlayItemDateAndTime: public OverlayItem
{

public:
    OverlayItemDateAndTime(int32_t ionDeviceId);

    virtual ~OverlayItemDateAndTime();

    int32_t init(OverlayItemParam& param) override;

    int32_t updateAndDraw() override;

    void getDrawInfo(uint32_t targetWidth, uint32_t targetHeight,
        DrawInfo* drawInfo) override;

    void getParameters(OverlayItemParam& param) override;

    int32_t updateParameters(OverlayItemParam& param) override;

private:
    int32_t createSurface();

    OverlayDateAndTimeType mDateAndTimeType;
    uint32_t               mTextColor;
#if USE_SKIA
    SkCanvas*              mCanvas;
#endif
};

#define BOUNDING_BOX_BUF_WIDTH     240
#define BOUNDING_BOX_BUF_HEIGHT    135
#define BOUNDING_BOX_STROKE_WIDTH  5
#define BOUNDING_BOX_TEXT_LIMIT    20
#define BOUNDING_BOX_TEXT_SIZE     30
#define BOUNDING_BOX_TEXT_PERCENT  20
#define BOUNDING_BOX_TEXT_MARGIN   5

class OverlayItemBoundingBox: public OverlayItem
{

public:
    OverlayItemBoundingBox(int32_t ionDeviceId);

    virtual ~OverlayItemBoundingBox();

    int32_t init(OverlayItemParam& param) override;

    int32_t updateAndDraw() override;

    void getDrawInfo(uint32_t targetWidth, uint32_t targetHeight,
        DrawInfo* drawInfo) override;

    void getParameters(OverlayItemParam& param) override;

    int32_t updateParameters(OverlayItemParam& param) override;
private:

    int32_t createSurface();

    uint32_t    mBBoxColor;
#if USE_SKIA
    SkCanvas*   mCanvas;
#endif
    String8     mBBoxName;
    uint32_t    mTextHeight;
};

#define TEXT_BUF_WIDTH              480
#define TEXT_BUF_HEIGHT             60
#define TEXT_TARGET_WIDTH_PERCENT   30
#define TEXT_TARGET_HEIGHT_PERCENT  10
#define TEXT_SIZE                   25

class OverlayItemText: public OverlayItem
{

public:
    OverlayItemText(int32_t ionDeviceId);

    virtual ~OverlayItemText();

    int32_t init(OverlayItemParam& param) override;

    int32_t updateAndDraw() override;

    void getDrawInfo(uint32_t targetWidth, uint32_t targetHeight,
        DrawInfo* drawInfo) override;

    void getParameters(OverlayItemParam& param) override;

    int32_t updateParameters(OverlayItemParam& param) override;

private:
    int32_t createSurface();

    uint32_t      mTextColor;
    String8       mText;
#if USE_SKIA
    SkCanvas*     mCanvas;
#endif

};

#define PRIVACY_MASK_COLOR         0xF9838383

class OverlayItemPrivacyMask: public OverlayItem
{

public:

    OverlayItemPrivacyMask(int32_t ionDeviceId);

    virtual ~OverlayItemPrivacyMask();

    int32_t init(OverlayItemParam& param) override;

    int32_t updateAndDraw() override;

    void getDrawInfo(uint32_t targetWidth,uint32_t targetHeight,DrawInfo * drawInfo) override;

    void getParameters(OverlayItemParam& param) override;

    int32_t updateParameters(OverlayItemParam& param) override;

private:

    int32_t createSurface();
#if USE_SKIA
    SkCanvas*   mCanvas;
#endif
};

}; // namespace overlay
}; // namespace qmmf