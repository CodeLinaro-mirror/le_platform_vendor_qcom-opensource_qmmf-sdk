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

#include <linux/msm_kgsl.h>
#include <linux/msm_ion.h>
#include <adreno/c2d2.h>
#if USE_SKIA
#include <SkCanvas.h>
#endif

namespace qmmf {

namespace overlay {

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

#define OVERLAYITEM_X_MARGIN_PERCENT  0.5
#define OVERLAYITEM_Y_MARGIN_PERCENT  0.5
#define MAX_LEN       128
#define MAX_OVERLAYS  10
//#define DEBUG_BACKGROUND_SURFACE

struct DrawInfo {
    uint32_t width;
    uint32_t height;
    uint32_t x;
    uint32_t y;
    uint32_t c2dSurfaceId;
};

struct C2dObjects {
  C2D_OBJECT objects[MAX_OVERLAYS];
};

//Base class for all types of overlays.
class OverlayItem : public RefBase
{
public:

    OverlayItem(int32_t ionDeviceId);

    ~OverlayItem();

    virtual int32_t init(OverlayParam& param) = 0 ;

    virtual int32_t updateAndDraw() = 0;

    virtual void getDrawInfo(uint32_t targetWidth, uint32_t targetHeight,
        DrawInfo* drawInfo) = 0 ;

    virtual void getParameters(OverlayParam& param) = 0;

    virtual int32_t updateParameters(OverlayParam& param) = 0;

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

    int32_t init(OverlayParam& param) override;

    int32_t updateAndDraw() override;

    void getDrawInfo(uint32_t targetWidth, uint32_t targetHeight,
        DrawInfo* drawInfo) override;

    void getParameters(OverlayParam& param) override;

    int32_t updateParameters(OverlayParam& param) override;
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

    int32_t init(OverlayParam& param) override;

    int32_t updateAndDraw() override;

    void getDrawInfo(uint32_t targetWidth, uint32_t targetHeight,
                     DrawInfo* drawInfo) override;

    void getParameters(OverlayParam& param) override;

    int32_t updateParameters(OverlayParam& param) override;

private:
    int32_t createSurface();

    OverlayDateTimeType mDateAndTimeType;
    uint32_t            mTextColor;
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

    int32_t init(OverlayParam& param) override;

    int32_t updateAndDraw() override;

    void getDrawInfo(uint32_t targetWidth, uint32_t targetHeight,
                     DrawInfo* drawInfo) override;

    void getParameters(OverlayParam& param) override;

    int32_t updateParameters(OverlayParam& param) override;
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

    int32_t init(OverlayParam& param) override;

    int32_t updateAndDraw() override;

    void getDrawInfo(uint32_t targetWidth, uint32_t targetHeight,
        DrawInfo* drawInfo) override;

    void getParameters(OverlayParam& param) override;

    int32_t updateParameters(OverlayParam& param) override;

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

    int32_t init(OverlayParam& param) override;

    int32_t updateAndDraw() override;

    void getDrawInfo(uint32_t targetWidth, uint32_t targetHeight,
                     DrawInfo * drawInfo) override;

    void getParameters(OverlayParam& param) override;

    int32_t updateParameters(OverlayParam& param) override;

private:

    int32_t createSurface();
#if USE_SKIA
    SkCanvas*   mCanvas;
#endif
};

}; // namespace overlay
}; // namespace qmmf