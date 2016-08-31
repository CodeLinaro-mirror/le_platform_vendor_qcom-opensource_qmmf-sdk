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

#define LOG_TAG "Overlay"

#include <utils/Log.h>
#include <algorithm>
#include <fcntl.h>
#include <dirent.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <media/msm_media_info.h>
#include <string.h>
#include <cstring>
#if USE_SKIA
#include <SkSurface.h>
#include <SkString.h>
#include <SkBitmap.h>
#include <SkBlurMaskFilter.h>
#endif

#include "qmmf-sdk/qmmf_overlay.h"
#include "qmmf_overlay_item.h"

namespace qmmf {

namespace overlay {

Overlay::Overlay()
                :mFrameWidth(0),
                 mFrameHeight(0),
                 mTargetC2dSurfaceId(-1),
                 mIonDevice(-1),
                 mNumActiveOverlays(0),
                 mId(0)
{ }

Overlay::~Overlay()
{
    OVDBG_INFO("%s: Enter ",__func__);

    for(size_t i = 0; i < mOverlayItems.size(); i++) {
        mOverlayItems.removeItem(mOverlayItems.keyAt(i));
    }
    mOverlayItems.clear();

    if(mTargetC2dSurfaceId) {
        c2dDestroySurface(mTargetC2dSurfaceId);
        mTargetC2dSurfaceId = 0;
        OVDBG_INFO("%s: Destroyed c2d Target Surface", __func__);
    }

    if(mIonDevice)
        close(mIonDevice);

    if(!mOverlayItems.isEmpty())
        mOverlayItems.clear();

    OVDBG_INFO("%s: Exit ",__func__);
}

int32_t Overlay::init(const BufFormat& format)
{
    OVDBG_LEVEL2("%s:Enter",__func__);

    uint32_t c2dColotFormat = getC2dColorFormat(format);
    //Create dummy C2D surface, it is required to initialize
    //C2D driver before calling any c2d Apis.
    C2D_YUV_SURFACE_DEF surface_def = {
        c2dColotFormat,
        1 * 4,
        1 * 4,
        (void*)0xaaaaaaaa,
        (void*)0xaaaaaaaa,
        1 * 4,
        (void*)0xaaaaaaaa,
        (void*)0xaaaaaaaa,
        1 * 4,
        (void*)0xaaaaaaaa,
        (void*)0xaaaaaaaa,
        1 * 4,
    };

    auto ret = c2dCreateSurface(&mTargetC2dSurfaceId, C2D_TARGET,
                             (C2D_SURFACE_TYPE)(C2D_SURFACE_YUV_HOST
                             |C2D_SURFACE_WITH_PHYS
                             |C2D_SURFACE_WITH_PHYS_DUMMY),
                             &surface_def);
    if(ret != C2D_STATUS_OK) {
       OVDBG_ERROR("%s: c2dCreateSurface failed!",__func__);
       return ret;
    }

    mIonDevice = open("/dev/ion", O_RDONLY);
    if (mIonDevice < 0) {
        OVDBG_ERROR("%s: Ion dev open failed %s\n", __func__,strerror(errno));
        c2dDestroySurface(mTargetC2dSurfaceId);
        mTargetC2dSurfaceId = 0;
        return -1;
    }

    OVDBG_LEVEL2("%s: Exit",__func__);
    return ret;
}

int32_t Overlay::createOverlayItem(OverlayParam& param, uint32_t* overlayId)
{
    OVDBG_LEVEL2("%s:Enter ", __func__);

    OverlayItem* overlayItem = NULL;
    switch(param.type) {
        case OverlayType::kDateType:
            overlayItem = new OverlayItemDateAndTime(mIonDevice);
            break;
        case OverlayType::kUserText:
            overlayItem = new OverlayItemText(mIonDevice);
            break;
        case OverlayType::kStaticImage:
            overlayItem = new OverlayItemStaticImage(mIonDevice);
            break;
        case OverlayType::kBoundingBox:
            overlayItem = new OverlayItemBoundingBox(mIonDevice);
            break;
        case OverlayType::kPrivacyMask:
            overlayItem = new OverlayItemPrivacyMask(mIonDevice);
            break;
        default:
            OVDBG_ERROR("%s: OverlayType(%d) not supported!", __func__,
                 param.type);
            break;
    }

    if(!overlayItem) {
        OVDBG_ERROR("%s: OverlayItem type(%d) failed!", __func__, param.type);
        return NO_INIT;
    }

    auto ret = overlayItem->init(param);
    if(ret != C2D_STATUS_OK) {
        OVDBG_ERROR("%s:OverlayItem failed of type(%d)", __func__, param.type);
        delete overlayItem;
        return ret;
    }

    /**
    StaticImage and PrivacyMask type overlayItems never be dirty
    Their contents are static, all other items are dirty at init
    time and will be marked as dirty whenever their configuration
    changes at run time after first draw.
    */
    if((param.type == OverlayType::kStaticImage)
        || (param.type == OverlayType::kPrivacyMask)) {
        overlayItem->markDirty(false);
    } else {
        overlayItem->markDirty(true);
    }

    *overlayId = ++mId;
    mOverlayItems.add(*overlayId, overlayItem);
    OVDBG_INFO("%s:OverlayItem Type(%d) Id(%d) Created Successfully !",__func__,
        param.type, *overlayId);

    OVDBG_LEVEL2("%s:Exit ", __func__);
    return ret;
}

int32_t Overlay::deleteOverlayItem(uint32_t overlayId)
{
    OVDBG_LEVEL2("%s:Enter ", __func__);
    Mutex::Autolock lock(mLock);

    int32_t ret = 0;
    if(!isOverlayItemValid(overlayId)) {
        OVDBG_ERROR("%s: overlayId(%d) is not valid!",__func__, overlayId);
        return BAD_VALUE;
    }
    sp<OverlayItem> overlayItem = mOverlayItems.valueFor(overlayId);
    assert(overlayItem.get() != NULL);

    mOverlayItems.removeItem(overlayId);
    OVDBG_INFO("%s: overlayId(%d) & overlayItem=%p Removed from map",__func__
                                                , overlayId, overlayItem.get());
    OVDBG_LEVEL2("%s:Exit ", __func__);
    return ret;
}

int32_t Overlay::getOverlayParams(uint32_t overlayId,
         OverlayParam& param)
{
    int32_t ret = 0;

    if(!isOverlayItemValid(overlayId)) {
        OVDBG_ERROR("%s: overlayId(%d) is not valid!",__func__, overlayId);
        return BAD_VALUE;
    }
    sp<OverlayItem> overlayItem = mOverlayItems.valueFor(overlayId);
    assert(overlayItem.get() != NULL);

    memset(&param, 0x0, sizeof param);
    overlayItem->getParameters(param);

    return ret;
}

int32_t Overlay::updateOverlayParams(uint32_t overlayId,
                             OverlayParam& param)
{
    OVDBG_LEVEL2("%s:Enter ", __func__);
    Mutex::Autolock lock(mLock);

    if(!isOverlayItemValid(overlayId)) {
        OVDBG_ERROR("%s: overlayId(%d) is not valid!",__func__, overlayId);
        return BAD_VALUE;
    }
    sp<OverlayItem> overlayItem = mOverlayItems.valueFor(overlayId);
    assert(overlayItem.get() != NULL);

    OVDBG_LEVEL2("%s:Exit ", __func__);
    return overlayItem->updateParameters(param);
}

int32_t Overlay::enableOverlayItem(uint32_t overlayId)
{
    OVDBG_LEVEL2("%s: Enter", __func__);
    Mutex::Autolock lock(mLock);

    int32_t ret = 0;
    if(!isOverlayItemValid(overlayId)) {
        OVDBG_ERROR("%s: overlayId(%d) is not valid!",__func__, overlayId);
        return BAD_VALUE;
    }
    sp<OverlayItem> overlayItem = mOverlayItems.valueFor(overlayId);
    assert(overlayItem.get() != NULL);

    overlayItem->activate(true);
    OVDBG_LEVEL1("%s: OverlayItem Id(%d) Activated", __func__, overlayId);

    OVDBG_LEVEL2("%s: Exit", __func__);
    return ret;
}

int32_t Overlay::disableOverlayItem(uint32_t overlayId)
{
    OVDBG_LEVEL2("%s: Enter", __func__);
    Mutex::Autolock lock(mLock);

    int32_t ret = 0;
    if(!isOverlayItemValid(overlayId)) {
        OVDBG_ERROR("%s: overlayId(%d) is not valid!",__func__, overlayId);
        return BAD_VALUE;
    }
    sp<OverlayItem> overlayItem = mOverlayItems.valueFor(overlayId);
    assert(overlayItem.get() != NULL);

    overlayItem->activate(false);
    OVDBG_LEVEL1("%s: OverlayItem Id(%d) Deactivated", __func__, overlayId);

    OVDBG_LEVEL2("%s: Exit", __func__);
    return ret;
}

int32_t Overlay::applyOverlay(const TargetBuf& buffer)
{
    OVDBG_LEVEL2("%s: Enter", __func__);
    int32_t ret = 0;
    Mutex::Autolock lock(mLock);

    size_t numActiveOverlays = 0;
    bool isItemsActive = false;
    for(size_t i = 0; i < mOverlayItems.size(); i++) {
        if(mOverlayItems.valueAt(i)->isActive()) {
            isItemsActive = true;
        }
    }
    if(!isItemsActive) {
        OVDBG_LEVEL2("%s: No overlayItem is Active!", __func__);
        return ret;
    }

    assert(buffer.ionFd != 0);
    assert(buffer.width != 0 && buffer.height != 0);
    assert(buffer.frameLen != 0);

    OVDBG_LEVEL2("%s: TargetBuf: ionFd = %d",__func__, buffer.ionFd);
    OVDBG_LEVEL2("%s: TargetBuf: Width = %d & Height = %d & frameLength =% d"
                ,__func__, buffer.width, buffer.height, buffer.frameLen);
    OVDBG_LEVEL2("%s: TargetBuf: format = %d", __func__, buffer.format);

    void* bufVaddr = mmap(NULL, buffer.frameLen, PROT_READ  | PROT_WRITE,
                                                MAP_SHARED, buffer.ionFd, 0);
    if(!bufVaddr) {
        OVDBG_ERROR("%s: mmap failed!", __func__);
        return UNKNOWN_ERROR;
    }

    //Map Camera stream buffer to GPU.
    void *gpuAddr = NULL;
    ret = c2dMapAddr(buffer.ionFd, bufVaddr, buffer.frameLen, 0,
            KGSL_USER_MEM_TYPE_ION, &gpuAddr);
    if(ret != C2D_STATUS_OK) {
        OVDBG_ERROR("%s: c2dMapAddr failed!",__func__);
        goto EXIT;
    }

    //Target surface format.
    C2D_YUV_SURFACE_DEF surface_def;
    surface_def.format  = getC2dColorFormat(buffer.format);
    surface_def.width   = buffer.width;
    surface_def.height  = buffer.height;
    int32_t planeYLen;
    switch (surface_def.format) {
        case C2D_COLOR_FORMAT_420_NV12:
            //Y plane stride.
            surface_def.stride0 = VENUS_Y_STRIDE(COLOR_FMT_NV12,
                    surface_def.width);

            //UV plane stride.
            surface_def.stride1 = VENUS_UV_STRIDE(COLOR_FMT_NV12,
                    surface_def.width);

            //UV plane hostptr.
            planeYLen = surface_def.stride0 * VENUS_Y_SCANLINES(COLOR_FMT_NV12,
                    surface_def.height);

            break;
        case C2D_COLOR_FORMAT_420_NV21:
            //Y plane stride.
            surface_def.stride0 = VENUS_Y_STRIDE(COLOR_FMT_NV21,
                    surface_def.width);

            //UV plane stride.
            surface_def.stride1 = VENUS_UV_STRIDE(COLOR_FMT_NV21,
                    surface_def.width);

            //UV plane hostptr.
            planeYLen = surface_def.stride0 * VENUS_Y_SCANLINES(COLOR_FMT_NV21,
                    surface_def.height);

            break;
        default:
            OVDBG_ERROR("%s: Unknown format: %d", __func__, surface_def.format);
            goto EXIT;
    }

    OVDBG_LEVEL1("%s: surface_def.stride0 = %d ",__func__, surface_def.stride0);
    OVDBG_LEVEL1("%s: planeYLen = %d",__func__, planeYLen);

    //Y plane hostptr.
    surface_def.plane0  = (void*)bufVaddr;
    //Y plane Gpu address.
    surface_def.phys0   = (void*)gpuAddr;

    surface_def.plane1  = (void*)((intptr_t)bufVaddr + planeYLen);

    //UV plane Gpu address.
    surface_def.phys1 = (void*)((intptr_t)gpuAddr + planeYLen);

    //Create C2d target surface outof camera buffer. camera buffer
    //is target surface where c2d blits different types of overlays
    //static logo, system time and date.
    ret = c2dUpdateSurface(mTargetC2dSurfaceId, C2D_SOURCE,
                            (C2D_SURFACE_TYPE)(C2D_SURFACE_YUV_HOST
                            |C2D_SURFACE_WITH_PHYS), &surface_def);
    if(ret != C2D_STATUS_OK) {
       OVDBG_ERROR("%s: c2dUpdateSurface failed!",__func__);
       goto EXIT;
    }

    //Iterate all dirty overlay Items, and update them.
    for(size_t i = 0; i < mOverlayItems.size(); i++) {
        if(mOverlayItems.valueAt(i)->isActive()) {
            ret = mOverlayItems.valueAt(i)->updateAndDraw();
            if(ret != 0) {
                OVDBG_ERROR("%s: Update & Draw failed for Item=%d", __func__,
                    mOverlayItems.keyAt(i));
            }
        }
    }

    C2dObjects c2d_objects;
    memset(&c2d_objects, 0x0, sizeof c2d_objects);
    //Iterate all updated overlayItems, and get coordinates.
    for(size_t i = 0, j = 0; i < mOverlayItems.size(); i++) {
        DrawInfo drawInfo;
        memset(&drawInfo, 0x0, sizeof drawInfo);

        if(mOverlayItems.valueAt(i)->isActive()) {
            mOverlayItems.valueAt(i)->getDrawInfo(buffer.width, buffer.height,
                &drawInfo);
            c2d_objects.objects[j].surface_id  = drawInfo.c2dSurfaceId;
            c2d_objects.objects[j].config_mask = C2D_ALPHA_BLEND_SRC_ATOP
                                        |C2D_TARGET_RECT_BIT;
            c2d_objects.objects[j].target_rect.x       = drawInfo.x << 16;
            c2d_objects.objects[j].target_rect.y       = drawInfo.y << 16;
            c2d_objects.objects[j].target_rect.width   = drawInfo.width << 16;
            c2d_objects.objects[j].target_rect.height  = drawInfo.height << 16;

            OVDBG_LEVEL2("%s: c2d_objects[%d].surface_id=%d", __func__, j,
                c2d_objects.objects[j].surface_id);
            OVDBG_LEVEL2("%s: c2d_objects[%d].target_rect.x=%d", __func__, j,
                drawInfo.x);
            OVDBG_LEVEL2("%s: c2d_objects[%d].target_rect.y=%d", __func__, j,
                drawInfo.y);
            OVDBG_LEVEL2("%s: c2d_objects[%d].target_rect.width=%d", __func__,
                j, drawInfo.width);
            OVDBG_LEVEL2("%s: c2d_objects[%d].target_rect.height=%d", __func__,
                j, drawInfo.height);
            ++numActiveOverlays;
            ++j;
        }
    }

    OVDBG_LEVEL2("%s: numActiveOverlays=%d", __func__, numActiveOverlays);
    for(size_t i = 0; i < (numActiveOverlays-1); i++) {
        c2d_objects.objects[i].next = &c2d_objects.objects[i+1];
    }

    ret = c2dDraw(mTargetC2dSurfaceId, 0, 0, 0, 0, c2d_objects.objects,
                  numActiveOverlays);
    if(ret != C2D_STATUS_OK) {
       OVDBG_ERROR("%s: c2dDraw failed!",__func__);
       goto EXIT;
    }

    ret = c2dFinish(mTargetC2dSurfaceId);
    if(ret != C2D_STATUS_OK) {
       OVDBG_ERROR("%s: c2dFinish failed!",__func__);
       goto EXIT;
    }
    //Unmap camera buffer from GPU after draw is completed.
    ret = c2dUnMapAddr(gpuAddr);
    if(ret != C2D_STATUS_OK) {
       OVDBG_ERROR("%s: c2dUnMapAddr failed!",__func__);
       goto EXIT;
    }

EXIT:
    if (bufVaddr) {
        munmap(bufVaddr, buffer.frameLen);
    }

    OVDBG_LEVEL2("%s: Exit ",__func__);
    return ret;
}

uint32_t Overlay::getC2dColorFormat(const BufFormat& format)
{
    uint32_t c2dColorFormat = C2D_COLOR_FORMAT_420_NV12;
    switch (format) {
        case BufFormat::FORMAT_YUV_NV12:
            c2dColorFormat = C2D_COLOR_FORMAT_420_NV12;
            break;
        case BufFormat::FORMAT_YUV_NV21:
            c2dColorFormat = C2D_COLOR_FORMAT_420_NV21;
            break;
        case BufFormat::FORMAT_RGB_888:
            c2dColorFormat = C2D_COLOR_FORMAT_888_RGB;
            break;
        case BufFormat::FORMAT_RGBA_8888:
            c2dColorFormat = C2D_COLOR_FORMAT_8888_RGBA;
            break;
        default:
            OVDBG_ERROR("%s: Unsupported buffer format: %d", __func__, format);
            break;
    }
    OVDBG_LEVEL2("%s:Selected C2D ColorFormat=%d",__func__, c2dColorFormat);
    return c2dColorFormat;
}

bool Overlay::isOverlayItemValid(uint32_t overlayId)
{
    OVDBG_LEVEL1("%s: Enter overlayId(%d)",__func__, overlayId);
    bool valid = false;
    for(size_t i = 0; i < mOverlayItems.size(); i++) {
        if (overlayId == mOverlayItems.keyAt(i)) {
            valid = true;
            break;
        }
    }
    OVDBG_LEVEL1("%s: Exit overlayId(%d)",__func__, overlayId);
    return valid;
}

OverlayItem::OverlayItem(int32_t ionDeviceId)
            :mX(0), mY(0), mWidth(0), mHeight(0),
             mC2dSurfaceId(-1), mGpuAddr(NULL),
             mVaddr(NULL), mIonFd(0), mSize(0),
             mDirty(false), mIonDevice(ionDeviceId),
             mIsActive(false)

{
    OVDBG_LEVEL2("%s:Enter ", __func__);
    memset(&mHandleData, 0x0, sizeof mHandleData);
    mLocationType = OverlayLocationType::kBottomLeft;
    OVDBG_LEVEL2("%s:Exit ", __func__);
}

OverlayItem::~OverlayItem()
{
    //Unmap overlay gpu address.
    if(mGpuAddr) {
        c2dUnMapAddr(mGpuAddr);
        mGpuAddr = NULL;
        OVDBG_INFO("%s: Unmapped GPU address type(%d)", __func__, mType);
    }
    if(mVaddr) {
        munmap(mVaddr, mSize);
        mVaddr = NULL;
    }
    //Destroy source overlay surface.
    if(mC2dSurfaceId) {
        c2dDestroySurface(mC2dSurfaceId);
        mC2dSurfaceId = -1;
        OVDBG_INFO("%s: Destroyed c2d Surface type(%d)",__func__, mType);
    }
    //Free overlay ION memory.
    if(mIonFd) {
        ioctl(mIonDevice, ION_IOC_FREE, &mHandleData);
        close(mIonFd);
        mIonFd = -1;
        OVDBG_INFO("%s: Destroyed ION buffer type(%d)",__func__, mType);
    }
}

void OverlayItem::markDirty(bool dirty)
{
    mDirty = dirty;
    OVDBG_LEVEL2("%s: OverlayItem Type(%d) marked dirty!", __func__, mType);
}

void OverlayItem::activate(bool value)
{
    mIsActive = value;
    OVDBG_LEVEL2("%s: OverlayItem Type(%d) Activated!", __func__, mType);
}

int32_t OverlayItem::allocateIonMemory(ionMemInfo& memInfo, uint32_t size)
{
    OVDBG_LEVEL2("%s:Enter",__func__);

    struct ion_allocation_data alloc;
    struct ion_fd_data ionFdData;
    void *data = NULL;
    int ionType = 0x1 << ION_IOMMU_HEAP_ID;
    int32_t ret = 0;

    memset(&alloc, 0, sizeof(ion_allocation_data));
    alloc.len = size;
    alloc.len = (alloc.len + 4095) & (~4095);
    alloc.align = 4096;
    alloc.flags = ION_FLAG_CACHED;
    alloc.heap_id_mask = ionType;
    ret = ioctl(mIonDevice, ION_IOC_ALLOC, &alloc);
    if (ret < 0) {
        OVDBG_ERROR("%s:ION allocation failed\n",__func__);
        goto ION_ALLOC_FAILED;
    }

    memset(&ionFdData, 0, sizeof(ion_fd_data));
    ionFdData.handle = alloc.handle;
    ret = ioctl(mIonDevice, ION_IOC_SHARE, &ionFdData);
    if (ret < 0) {
        OVDBG_ERROR("%s:ION map failed %s\n",__func__,strerror(errno));
        goto ION_MAP_FAILED;
    }

    data = mmap(NULL, alloc.len,
                PROT_READ  | PROT_WRITE,
                MAP_SHARED,ionFdData.fd, 0);

    if (data == MAP_FAILED) {
        OVDBG_ERROR("%s:ION mmap failed: %s (%d)\n",__func__, strerror(errno),
            errno);
        goto ION_MAP_FAILED;
    }

    memset(&memInfo.handleData, 0, sizeof(memInfo.handleData));
    memInfo.handleData.handle  = ionFdData.handle;
    memInfo.fd                 = ionFdData.fd;
    memInfo.size               = alloc.len;
    memInfo.vaddr              = data;

    OVDBG_LEVEL2("%s:Exit ",__func__);
    return ret;

ION_MAP_FAILED:
    memset(&memInfo.handleData, 0, sizeof(memInfo.handleData));
    memInfo.handleData.handle = ionFdData.handle;
    ioctl(mIonDevice, ION_IOC_FREE, &memInfo.handleData);
ION_ALLOC_FAILED:
    close(mIonDevice);
    return -1;
}

OverlayItemStaticImage::OverlayItemStaticImage(int32_t ionDeviceId)
                       :OverlayItem(ionDeviceId)
                       , mImagePath()
{
    OVDBG_LEVEL2("%s: Enter", __func__);
    mType = OverlayType::kStaticImage;
    OVDBG_LEVEL2("%s: Exit", __func__);
}

OverlayItemStaticImage::~OverlayItemStaticImage()
{
    OVDBG_LEVEL2("%s: Enter", __func__);
    mImagePath.clear();
    OVDBG_LEVEL2("%s: Exit", __func__);
}

int32_t OverlayItemStaticImage::init(OverlayParam& param)
{
    OVDBG_LEVEL2("%s: Enter", __func__);

    int32_t ret = 0;

    if(param.image_info.width <= 0 || param.image_info.height <= 0) {
        OVDBG_ERROR("%s: Image Width & Height is not correct!", __func__);
        return BAD_VALUE;
    }

    mLocationType = param.location;
    mWidth        = param.image_info.width;
    mHeight       = param.image_info.height;

    mImagePath.setTo(param.image_info.image_location,
             strlen(param.image_info.image_location) + 1);

    ret = createSurface();
    if(ret != 0) {
        OVDBG_ERROR("%s: createLogoSurface failed!", __func__);
        return ret;
    }
    OVDBG_LEVEL2("%s: Exit", __func__);
    return ret;
}

int32_t OverlayItemStaticImage::updateAndDraw()
{
    //Nothing to update, contents are static.
    //Never marked as dirty.
    return OK;
}

void OverlayItemStaticImage::getDrawInfo(uint32_t targetWidth,
        uint32_t targetHeight, DrawInfo* drawInfo)
{
    OVDBG_LEVEL2("%s: Enter", __func__);
    drawInfo->width  = mWidth;
    drawInfo->height = mHeight;
    int32_t xMargin = targetWidth * OVERLAYITEM_X_MARGIN_PERCENT/100;
    int32_t yMargin = targetHeight * OVERLAYITEM_Y_MARGIN_PERCENT/100;
    int32_t x = 0;
    int32_t y = 0;

    switch (mLocationType) {
        case OverlayLocationType::kTopLeft:
            x = xMargin;
            y = yMargin;
            break;
        case OverlayLocationType::kTopRight:
            x = targetWidth - (mWidth + xMargin);
            y = yMargin;
            break;
        case OverlayLocationType::kCenter:
            x = (targetWidth - mWidth)/2;
            y = (targetHeight - mHeight)/2;
            break;
        case OverlayLocationType::kBottomLeft:
            x = xMargin;
            y = targetHeight - (mHeight + yMargin);
            break;
        case OverlayLocationType::kBottomRight:
            x = targetWidth - (mWidth + xMargin);
            y = targetHeight - (mHeight + yMargin);
            break;
        case OverlayLocationType::kNone:
        default:
            x = mX;
            y = mY;
            break;
    }
    drawInfo->x            = x;
    drawInfo->y            = y;
    drawInfo->c2dSurfaceId = mC2dSurfaceId;

    OVDBG_LEVEL2("%s: Exit", __func__);
}

void OverlayItemStaticImage::getParameters(OverlayParam& param)
{
    OVDBG_LEVEL2("%s:Enter ",__func__);
    param.type             = OverlayType::kStaticImage;
    param.location         = mLocationType;
    param.image_info.width  = mWidth;
    param.image_info.height = mHeight;
    std::string str(mImagePath.string());
    str.copy(param.image_info.image_location, mImagePath.length());
    OVDBG_LEVEL2("%s:Exit ",__func__);
}

int32_t OverlayItemStaticImage::updateParameters(OverlayParam& param)
{
    OVDBG_LEVEL2("%s:Enter ",__func__);
    int32_t ret = 0;

    if(strcmp(mImagePath.string(), param.image_info.image_location) != 0) {
        OVDBG_ERROR("%s: Image Path Can't be changed at run time!!", __func__);
        return BAD_VALUE;
    }

    if(param.image_info.width <= 0 || param.image_info.height <= 0) {
        OVDBG_ERROR("%s: Image Width & Height is not correct!", __func__);
        return BAD_VALUE;
    }

    mLocationType = param.location;
    mWidth        = param.image_info.width;
    mHeight       = param.image_info.height;

    OVDBG_LEVEL2("%s:Exit ",__func__);
    return ret;
}

int32_t OverlayItemStaticImage::createSurface()
{
    OVDBG_LEVEL2("%s:Enter ",__func__);

    int32_t   ret = 0;
    uint32_t size = mWidth * mHeight * 4;

    ionMemInfo memInfo;
    memset(&memInfo, 0x0, sizeof(ionMemInfo));

    ret = allocateIonMemory(memInfo, size);
    if(0 != ret) {
       OVDBG_ERROR("%s:allocateIonMemory failed",__func__);
       return ret;
    }
    uint32_t* pixels = (uint32_t*)memInfo.vaddr;

    //Load raw logo image file.
    FILE *file = 0;
    size_t bytes;

    file = fopen(mImagePath.string(), "rb");
    if(file) {
        bytes = fread(pixels, 1, size, file);
        OVDBG_INFO("%s: Total btyes = %d",__func__,bytes);
        if(bytes != size) {
            OVDBG_ERROR("%s: Raw file format is not correct",__func__);
            fclose(file);
            goto ERROR;
        }
        fclose(file);
    } else {
        OVDBG_ERROR("%s: (%s)File open Failed!!",__func__, mImagePath.string());
        goto ERROR;
    }

    //Map ARGB ION buffer to GPU.
    ret = c2dMapAddr(memInfo.fd, memInfo.vaddr, memInfo.size,
                       0, KGSL_USER_MEM_TYPE_ION, &mGpuAddr);
    if(ret != C2D_STATUS_OK) {
       OVDBG_ERROR("%s: c2dMapAddr failed!",__func__);
       goto ERROR;
    }

    C2D_RGB_SURFACE_DEF c2dSurfaceDef;
    c2dSurfaceDef.format = C2D_FORMAT_SWAP_ENDIANNESS | C2D_COLOR_FORMAT_8888_RGBA;
    c2dSurfaceDef.width  = mWidth;
    c2dSurfaceDef.height = mHeight;
    c2dSurfaceDef.buffer = memInfo.vaddr;
    c2dSurfaceDef.phys   = mGpuAddr;
    c2dSurfaceDef.stride = mWidth * 4;

    //Create source c2d surface.
    ret = c2dCreateSurface(&mC2dSurfaceId, C2D_SOURCE,
                          (C2D_SURFACE_TYPE)(C2D_SURFACE_RGB_HOST
                           |C2D_SURFACE_WITH_PHYS), &c2dSurfaceDef);
    if(ret != C2D_STATUS_OK) {
       OVDBG_ERROR("%s: c2dCreateSurface failed!",__func__);
       goto ERROR;
    }
    mIonFd        = memInfo.fd;
    mVaddr        = memInfo.vaddr;
    mSize         = memInfo.size;
    mHandleData   = memInfo.handleData;

    OVDBG_LEVEL2("%s: Exit ",__func__);
    return ret;
ERROR:
    ioctl(mIonDevice, ION_IOC_FREE, &mHandleData);
    close(mIonFd);
    mIonFd = -1;
    return ret;
}

OverlayItemDateAndTime::OverlayItemDateAndTime(int32_t ionDeviceId)
                :OverlayItem(ionDeviceId)
{
    OVDBG_LEVEL2("%s:Enter ", __func__);
    memset(&mDateAndTimeType, 0x0, sizeof mDateAndTimeType);
    mDateAndTimeType.time_type = OverlayTimeType::kHHMM_24HR;
    mDateAndTimeType.date_type = OverlayDateType::kMMDDYYYY;
    mType                     = OverlayType::kDateType;
    OVDBG_LEVEL2("%s:Exit", __func__);
}

OverlayItemDateAndTime::~OverlayItemDateAndTime()
{
    OVDBG_LEVEL2("%s:Enter ", __func__);
    OVDBG_LEVEL2("%s:Exit ", __func__);
}

int32_t OverlayItemDateAndTime::init(OverlayParam& param)
{
    OVDBG_LEVEL2("%s: Enter", __func__);

    mLocationType = param.location;
    mTextColor    = param.text_color;

    mDateAndTimeType.date_type = param.date_time_type.date_type;
    mDateAndTimeType.time_type = param.date_time_type.time_type;
    mWidth  = DATETIME_TEXT_BUF_WIDTH;
    mHeight = DATETIME_TEXT_BUF_HEIGHT;

    auto ret = createSurface();
    if(ret != 0) {
        OVDBG_ERROR("%s: createLogoSurface failed!", __func__);
        return ret;
    }
    OVDBG_LEVEL2("%s: Exit", __func__);
    return ret;
}

int32_t OverlayItemDateAndTime::updateAndDraw()
{
    OVDBG_LEVEL2("%s: Enter", __func__);
    int32_t ret = 0;

    if(!mDirty)
        return ret;
#if USE_SKIA

#ifndef DEBUG_BACKGROUND_SURFACE
    mCanvas->clear(SK_AlphaOPAQUE);
#else
    mCanvas->clear(SK_ColorDKGRAY);
#endif

    SkPaint paint;
    paint.setColor(mTextColor);
    paint.setTextSize(SkIntToScalar(DATETIME_PIXEL_SIZE));
    paint.setAntiAlias(true);
    paint.setTextScaleX(1);

    struct timeval tv;
    time_t nowtime;
    struct tm *time;
    char dateBuf[40];
    char timeBuf[40];

    gettimeofday(&tv, NULL);
    nowtime = tv.tv_sec;
    time = localtime(&nowtime);

    switch(mDateAndTimeType.date_type) {
        case OverlayDateType::kYYYYMMDD:
            strftime(dateBuf, sizeof dateBuf, "%Y/%m/%d", time);
            break;
        case OverlayDateType::kMMDDYYYY:
        default:
            strftime(dateBuf, sizeof dateBuf, "%m/%d/%Y", time);
            break;
    }
    switch(mDateAndTimeType.time_type) {
        case OverlayTimeType::kHHMMSS_24HR:
            strftime(timeBuf, sizeof timeBuf, "%H:%M:%S", time);
            break;
        case OverlayTimeType::kHHMMSS_AMPM:
            strftime(timeBuf, sizeof timeBuf, "%r", time);
            break;
        case OverlayTimeType::kHHMM_24HR:
            strftime(timeBuf, sizeof timeBuf, "%H:%M", time);
            break;
        case OverlayTimeType::kHHMM_AMPM:
        default:
            strftime(timeBuf, sizeof timeBuf, "%I:%M %p", time);
            break;
    }
    OVDBG_LEVEL2("%s: date:time (%s:%s)", __func__, dateBuf, timeBuf);

    int32_t dateLen = strlen(dateBuf);
    int32_t timeLen = strlen(timeBuf);

    //(0,0) is at topleft corner of skia buffer.
    int32_t xDate = 0;
    int32_t yDate = DATETIME_TEXT_BUF_HEIGHT/2;
    SkString dateText(dateBuf, dateLen);
    mCanvas->drawText(dateText.c_str(), dateText.size(), xDate, yDate, paint);

    SkString timeText(timeBuf, timeLen);
    int32_t perCharSize = DATETIME_TEXT_BUF_WIDTH/dateText.size();
    int32_t xTime = (DATETIME_TEXT_BUF_WIDTH - (timeText.size() * perCharSize));
    xTime = xTime > 0 ? (xTime) : 0;
    int32_t yTime = DATETIME_TEXT_BUF_HEIGHT - DATETIME_PIXEL_SIZE/2;
    mCanvas->drawText(timeText.c_str(), timeText.size(), xTime, yTime, paint);
    mCanvas->flush();
    usleep(1000);
#endif
    markDirty(true);
    OVDBG_LEVEL2("%s: Exit", __func__);
    return ret;
}

void OverlayItemDateAndTime::getDrawInfo(uint32_t targetWidth,
        uint32_t targetHeight, DrawInfo* drawInfo)
{
    OVDBG_LEVEL2("%s:Enter ",__func__);
    drawInfo->width  = targetWidth * DATETIME_TARGET_WIDTH_PERCENT/100;
    drawInfo->height = targetHeight * DATETIME_TARGET_HEIGHT_PERCENT/100;

    int32_t xMargin = targetWidth * OVERLAYITEM_X_MARGIN_PERCENT/100;
    int32_t yMargin = targetHeight * OVERLAYITEM_Y_MARGIN_PERCENT/100;
    int32_t x = 0;
    int32_t y = 0;

    //(0,0) is at topleft corner.
    switch (mLocationType) {
        case OverlayLocationType::kTopLeft:
            x = xMargin;
            y = yMargin;
            break;
        case OverlayLocationType::kTopRight:
            x = targetWidth - (drawInfo->width + xMargin);
            y = yMargin;
            break;
        case OverlayLocationType::kCenter:
            x = (targetWidth - drawInfo->width)/2;
            y = (targetHeight - drawInfo->height)/2;
            break;
        case OverlayLocationType::kBottomLeft:
            x = xMargin;
            y = targetHeight - (drawInfo->height + yMargin);
            break;
        case OverlayLocationType::kBottomRight:
            x = targetWidth - (drawInfo->width + xMargin);
            y = targetHeight - (drawInfo->height + yMargin);
            break;
        case OverlayLocationType::kNone:
        default:
            break;
    }
    drawInfo->x            = x;
    drawInfo->y            = y;
    drawInfo->c2dSurfaceId = mC2dSurfaceId;
    OVDBG_LEVEL2("%s:Exit ",__func__);
}

void OverlayItemDateAndTime::getParameters(OverlayParam& param)
{
    OVDBG_LEVEL2("%s:Enter ",__func__);
    param.type      = OverlayType::kDateType;
    param.location  = mLocationType;
    param.text_color = mTextColor;
    param.date_time_type.date_type = mDateAndTimeType.date_type;
    param.date_time_type.time_type = mDateAndTimeType.time_type;
    OVDBG_LEVEL2("%s:Exit ",__func__);
}

int32_t OverlayItemDateAndTime::updateParameters(OverlayParam& param)
{
    OVDBG_LEVEL2("%s:Enter ",__func__);
    int32_t ret = 0;
    mLocationType = param.location;
    mTextColor    = param.text_color;

    mDateAndTimeType.date_type = param.date_time_type.date_type;
    mDateAndTimeType.time_type = param.date_time_type.time_type;
    OVDBG_LEVEL2("%s:Exit ",__func__);
    return ret;
}

int32_t OverlayItemDateAndTime::createSurface()
{
    OVDBG_LEVEL2("%s: Enter", __func__);

    int32_t ret = 0;
    int32_t size = mWidth * mHeight * 4;
    ionMemInfo memInfo;
    memset(&memInfo, 0x0, sizeof(ionMemInfo));

    ret = allocateIonMemory(memInfo, size);
    if(0 != ret) {
       OVDBG_ERROR("%s:allocateIonMemory failed",__func__);
       return ret;
    }
    void* pixels = memInfo.vaddr;
    OVDBG_LEVEL1("%s: ION memory allocated fd = %d",__func__,memInfo.fd);

#if USE_SKIA
    //Create Skia canvas outof ION memory.
    SkImageInfo imageInfo;
    memset(&imageInfo, 0x0, sizeof(imageInfo));
    imageInfo.fWidth     = mWidth;
    imageInfo.fHeight    = mHeight;
    imageInfo.fColorType = kRGBA_8888_SkColorType;
    imageInfo.fAlphaType = kPremul_SkAlphaType;

    mCanvas = SkCanvas::NewRasterDirect(imageInfo, memInfo.vaddr,
                                        mWidth *4);
    if(!mCanvas) {
        OVDBG_ERROR("%s: Skia Creation failed!!",__func__);
        goto ERROR;
    }
#endif
    //Draw system time on Skia canvas.
    updateAndDraw();

    //Setup c2d.
    ret = c2dMapAddr(memInfo.fd, memInfo.vaddr, memInfo.size,
                       0, KGSL_USER_MEM_TYPE_ION, &mGpuAddr);
    if(ret != C2D_STATUS_OK) {
       OVDBG_ERROR("%s: c2dMapAddr failed!",__func__);
       goto ERROR;
    }

    C2D_RGB_SURFACE_DEF c2dSurfaceDef;
    c2dSurfaceDef.format = C2D_FORMAT_SWAP_ENDIANNESS | C2D_COLOR_FORMAT_8888_RGBA;
    c2dSurfaceDef.width  = mWidth;
    c2dSurfaceDef.height = mHeight;
    c2dSurfaceDef.buffer = memInfo.vaddr;
    c2dSurfaceDef.phys   = mGpuAddr;
    c2dSurfaceDef.stride = mWidth * 4;

    //Create source c2d surface.
    ret = c2dCreateSurface(&mC2dSurfaceId, C2D_SOURCE,
                          (C2D_SURFACE_TYPE)(C2D_SURFACE_RGB_HOST
                           |C2D_SURFACE_WITH_PHYS), &c2dSurfaceDef);
    if(ret != C2D_STATUS_OK) {
       OVDBG_ERROR("%s: c2dCreateSurface failed!",__func__);
       goto ERROR;
    }

    mIonFd        = memInfo.fd;
    mVaddr        = memInfo.vaddr;
    mSize         = memInfo.size;
    mHandleData   = memInfo.handleData;

    OVDBG_LEVEL2("%s: Exit", __func__);
    return ret;
ERROR:
    ioctl(mIonDevice, ION_IOC_FREE, &mHandleData);
    close(mIonFd);
    mIonFd = -1;
    return ret;
}

OverlayItemBoundingBox::OverlayItemBoundingBox(int32_t ionDeviceId)
                        :OverlayItem(ionDeviceId)
                        , mBBoxName()
                        , mTextHeight(0)

{
    OVDBG_INFO("%s: Enter", __func__);
    mType = OverlayType::kBoundingBox;
    OVDBG_INFO("%s: Exit", __func__);
}

OverlayItemBoundingBox::~OverlayItemBoundingBox()
{
    OVDBG_INFO("%s: Enter", __func__);
    mBBoxName.clear();
    OVDBG_INFO("%s: Exit", __func__);
}

int32_t OverlayItemBoundingBox::init(OverlayParam& param)
{
    OVDBG_LEVEL2("%s: Enter", __func__);

    if((param.bounding_box.width <= 0) || (param.bounding_box.height <= 0)) {
        return BAD_VALUE;
    }
    if(param.bounding_box.startX < 0 || param.bounding_box.startY < 0) {
        return BAD_VALUE;
    }

    mX          = param.bounding_box.startX;
    mY          = param.bounding_box.startY;
    mWidth      = param.bounding_box.width;
    mHeight     = param.bounding_box.height;
    mBBoxColor  = param.text_color;

    int32_t textLen = strlen(param.bounding_box.box_name);

    int32_t textLimit = std::min(textLen + 1, BOUNDING_BOX_TEXT_LIMIT);
    mBBoxName.setTo(param.bounding_box.box_name, textLimit);
    auto ret = createSurface();
    if(ret != 0) {
        OVDBG_ERROR("%s: createSurface failed!", __func__);
        return NO_INIT;
    }
    OVDBG_LEVEL2("%s: Exit", __func__);
    return ret;
}

int32_t OverlayItemBoundingBox::updateAndDraw()
{
    OVDBG_LEVEL2("%s: Enter ", __func__);
    int32_t ret = 0;

    if(!mDirty) {
        OVDBG_LEVEL1("%s: Item is not dirty! Don't draw!", __func__);
        return ret;
    }
#if USE_SKIA
    if (mWidth > 0 && mHeight > 0) {

#ifndef DEBUG_BACKGROUND_SURFACE
        mCanvas->clear(SK_AlphaOPAQUE);
#else
        mCanvas->clear(SK_ColorDKGRAY);
#endif
        /**
        Bounding Box and text are drawn on same Skia buffer.
        ----------
        | TEXT   |
        ----------
        |        |
        |  BOX   |
        |        |
        ----------
        */
        SkPaint paintBox, paintText;
        paintText.setColor(mBBoxColor);
        paintBox.setColor(mBBoxColor);

        paintText.setTextSize(SkIntToScalar(BOUNDING_BOX_TEXT_SIZE));
        paintText.setAntiAlias(true);

        paintBox.setStrokeWidth(BOUNDING_BOX_STROKE_WIDTH);
        paintBox.setStyle(SkPaint::kStroke_Style);

        int32_t xText = 0, yText = 0;
        int32_t xBBox = 0, yBBox = 0;
        if(mBBoxName.length() > 1) {
            SkString text(mBBoxName.string(), mBBoxName.length());
            //Text size is always 20% of buffer height.
            yText  = BOUNDING_BOX_BUF_HEIGHT * BOUNDING_BOX_TEXT_PERCENT/100;
            //Margin between text and bouding box rect.
            yText  = yText - BOUNDING_BOX_TEXT_MARGIN;
            mCanvas->drawText(text.c_str(), text.size(), 0, yText, paintText);
        }
        yBBox = yText > 0 ? BOUNDING_BOX_TEXT_SIZE : 0;
        int32_t boxWidth  = BOUNDING_BOX_BUF_WIDTH;
        int32_t boxHeight = BOUNDING_BOX_BUF_HEIGHT - yBBox;
        mTextHeight = yText;
        mCanvas->drawRect(SkRect::MakeXYWH(xBBox, yBBox, boxWidth, boxHeight),
                     paintBox);
        mCanvas->flush();
        usleep(3000);
    }
#endif
    markDirty(false);
    OVDBG_LEVEL2("%s: Exit", __func__);
    return ret;
}

void OverlayItemBoundingBox::getDrawInfo(uint32_t targetWidth,
                                         uint32_t targetHeight,
                                         DrawInfo* drawInfo)
{
    OVDBG_LEVEL2("%s: Enter", __func__);
    //Cut Text portion while scaling up bounding box to stream size.
    int32_t textPortion = BOUNDING_BOX_BUF_HEIGHT * BOUNDING_BOX_TEXT_PERCENT/100;
    textPortion        += BOUNDING_BOX_TEXT_MARGIN;
    int32_t ratio          = mHeight / BOUNDING_BOX_BUF_HEIGHT;
    drawInfo->x            = mX;
    drawInfo->y            = mY - (ratio * textPortion);
    drawInfo->width        = mWidth;
    drawInfo->height       = mHeight + (ratio * textPortion);
    drawInfo->c2dSurfaceId = mC2dSurfaceId;
    OVDBG_LEVEL2("%s: Exit", __func__);
}

void OverlayItemBoundingBox::getParameters(OverlayParam& param)
{
    OVDBG_LEVEL2("%s:Enter ",__func__);
    param.type      = OverlayType::kBoundingBox;
    param.location  = OverlayLocationType::kNone;
    param.text_color = mBBoxColor;
    param.bounding_box.startX = mX;
    param.bounding_box.startY = mY;
    param.bounding_box.width  = mWidth;
    param.bounding_box.height = mHeight;
    std::string str(mBBoxName.string());
    str.copy(param.bounding_box.box_name, mBBoxName.length());
    OVDBG_LEVEL2("%s:Exit ",__func__);
}

int32_t OverlayItemBoundingBox::updateParameters(OverlayParam& param)
{
    OVDBG_LEVEL2("%s:Enter ",__func__);
    int32_t ret = 0;

    if((param.bounding_box.width <= 0) || (param.bounding_box.height <= 0)) {
        return BAD_VALUE;
    }
    if(param.bounding_box.startX < 0 || param.bounding_box.startY < 0) {
        return BAD_VALUE;
    }
    mX          = param.bounding_box.startX;
    mY          = param.bounding_box.startY;
    mWidth      = param.bounding_box.width;
    mHeight     = param.bounding_box.height;
    mBBoxColor  = param.text_color;

    mBBoxName.clear();
    int32_t textLen = strlen(param.bounding_box.box_name);

    int32_t textLimit = std::min(textLen + 1, BOUNDING_BOX_TEXT_LIMIT);
    mBBoxName.setTo(param.bounding_box.box_name, textLimit);
    markDirty(true);
    OVDBG_LEVEL2("%s:Exit ",__func__);
    return ret;
}

int32_t OverlayItemBoundingBox::createSurface()
{
    OVDBG_LEVEL2("%s: Enter", __func__);
    int32_t size = BOUNDING_BOX_BUF_WIDTH * BOUNDING_BOX_BUF_HEIGHT * 4;

    ionMemInfo memInfo;
    memset(&memInfo, 0x0, sizeof(ionMemInfo));
    auto ret = allocateIonMemory(memInfo, size);
    if(0 != ret) {
       OVDBG_ERROR("%s:allocateIonMemory failed",__func__);
       return ret;
    }
    void* pixels = memInfo.vaddr;
    OVDBG_LEVEL1("%s: Ion memory allocated fd(%d)", __func__, memInfo.fd);
#if USE_SKIA
    //Create Skia canvas outof ION memory.
    SkImageInfo imageInfo;
    memset(&imageInfo, 0x0, sizeof(imageInfo));
    imageInfo.fWidth     = BOUNDING_BOX_BUF_WIDTH;
    imageInfo.fHeight    = BOUNDING_BOX_BUF_HEIGHT;
    imageInfo.fColorType = kRGBA_8888_SkColorType;
    imageInfo.fAlphaType = kPremul_SkAlphaType;

    mCanvas = SkCanvas::NewRasterDirect(imageInfo, memInfo.vaddr,
                                        BOUNDING_BOX_BUF_WIDTH *4);
    if(!mCanvas) {
        OVDBG_ERROR("%s: Skia Creation failed!!", __func__);
        goto ERROR;
    }
#endif
    //Setup c2d.
    ret = c2dMapAddr(memInfo.fd, memInfo.vaddr, memInfo.size,
                       0, KGSL_USER_MEM_TYPE_ION, &mGpuAddr);
    if(ret != C2D_STATUS_OK) {
       OVDBG_ERROR("%s: c2dMapAddr failed!", __func__);
       goto ERROR;
    }

    C2D_RGB_SURFACE_DEF c2dSurfaceDef;
    c2dSurfaceDef.format = C2D_FORMAT_SWAP_ENDIANNESS | C2D_COLOR_FORMAT_8888_RGBA;
    c2dSurfaceDef.width  = BOUNDING_BOX_BUF_WIDTH;
    c2dSurfaceDef.height = BOUNDING_BOX_BUF_HEIGHT;
    c2dSurfaceDef.buffer = memInfo.vaddr;
    c2dSurfaceDef.phys   = mGpuAddr;
    c2dSurfaceDef.stride = BOUNDING_BOX_BUF_WIDTH * 4;

    //Create source c2d surface.
    ret = c2dCreateSurface(&mC2dSurfaceId, C2D_SOURCE,
                          (C2D_SURFACE_TYPE)(C2D_SURFACE_RGB_HOST
                           |C2D_SURFACE_WITH_PHYS), &c2dSurfaceDef);
    if(ret != C2D_STATUS_OK) {
       OVDBG_ERROR("%s: c2dCreateSurface failed!", __func__);
       goto ERROR;
    }

    mIonFd        = memInfo.fd;
    mVaddr        = memInfo.vaddr;
    mSize         = memInfo.size;
    mHandleData   = memInfo.handleData;

    OVDBG_LEVEL2("%s: Exit", __func__);
    return ret;
ERROR:
    ioctl(mIonDevice, ION_IOC_FREE, &mHandleData);
    close(mIonFd);
    mIonFd = -1;
    return ret;
}

OverlayItemText::OverlayItemText(int32_t ionDeviceId)
                :OverlayItem(ionDeviceId)
                , mText()
{
    OVDBG_LEVEL2("%s:Enter ", __func__);
    mType = OverlayType::kUserText;
    OVDBG_LEVEL2("%s:Exit ", __func__);
}

OverlayItemText::~OverlayItemText()
{
    OVDBG_LEVEL2("%s:Enter ", __func__);
    mText.clear();
    OVDBG_LEVEL2("%s:Exit ", __func__);
}

int32_t OverlayItemText::init(OverlayParam& param)
{
    OVDBG_LEVEL2("%s: Enter", __func__);

    mLocationType = param.location;
    mTextColor    = param.text_color;

    mText.setTo(param.user_text, strlen(param.user_text) + 1);
    mWidth  = TEXT_BUF_WIDTH;
    mHeight = TEXT_BUF_HEIGHT;

    auto ret = createSurface();
    if(ret != 0) {
        OVDBG_ERROR("%s: createSurface failed!", __func__);
        return ret;
    }
    OVDBG_LEVEL2("%s: Exit", __func__);
    return ret;
}

int32_t OverlayItemText::updateAndDraw()
{
    OVDBG_LEVEL2("%s: Enter", __func__);
    int32_t ret = 0;

    if(!mDirty)
        return ret;
#if USE_SKIA

#ifndef DEBUG_BACKGROUND_SURFACE
    mCanvas->clear(SK_AlphaOPAQUE);
#else
    mCanvas->clear(SK_ColorDKGRAY);
#endif

    SkPaint paint;
    paint.setColor(mTextColor);
    paint.setTextSize(SkIntToScalar(TEXT_SIZE));
    paint.setAntiAlias(true);

    int32_t x = 0;
    int32_t y = TEXT_BUF_HEIGHT - TEXT_SIZE/2;
    SkString skText(mText.string(), mText.length());
    mCanvas->drawText(skText.c_str(), skText.size(), 0, y, paint);
    mCanvas->flush();
    usleep(1000);
#endif
    mDirty = false;
    OVDBG_LEVEL2("%s: Exit", __func__);
    return ret;
}

void OverlayItemText::getDrawInfo(uint32_t targetWidth,
        uint32_t targetHeight, DrawInfo* drawInfo)
{
    OVDBG_LEVEL2("%s: Enter", __func__);
    drawInfo->width  = targetWidth * TEXT_TARGET_WIDTH_PERCENT/100;
    drawInfo->height = targetHeight * TEXT_TARGET_HEIGHT_PERCENT/100;

    int32_t xMargin = targetWidth * OVERLAYITEM_X_MARGIN_PERCENT/100;
    int32_t yMargin = targetHeight * OVERLAYITEM_Y_MARGIN_PERCENT/100;
    int32_t x = 0;
    int32_t y = 0;

    //(0,0) is at topleft corner.
    switch (mLocationType) {
        case OverlayLocationType::kTopLeft:
            x = xMargin;
            y = yMargin;
            break;
        case OverlayLocationType::kTopRight:
            x = targetWidth - (drawInfo->width + xMargin);
            y = yMargin;
            break;
        case OverlayLocationType::kCenter:
            x = (targetWidth - drawInfo->width)/2;
            y = (targetHeight - drawInfo->height)/2;
            break;
        case OverlayLocationType::kBottomLeft:
            x = xMargin;
            y = targetHeight - (drawInfo->height + yMargin);
            break;
        case OverlayLocationType::kBottomRight:
            x = targetWidth - (drawInfo->width + xMargin);
            y = targetHeight - (drawInfo->height + yMargin);
            break;
        case OverlayLocationType::kNone:
        default:
            x = mX;
            y = mY;
            break;
    }
    drawInfo->x            = x;
    drawInfo->y            = y;
    drawInfo->c2dSurfaceId = mC2dSurfaceId;

    OVDBG_LEVEL2("%s: Exit", __func__);
}

void OverlayItemText::getParameters(OverlayParam& param)
{
    OVDBG_LEVEL2("%s:Enter ",__func__);
    param.type      = OverlayType::kUserText;
    param.location  = mLocationType;
    param.text_color = mTextColor;
    std::string str(mText.string());
    str.copy(param.user_text, mText.length());
    OVDBG_LEVEL2("%s:Exit ",__func__);
}

int32_t OverlayItemText::updateParameters(OverlayParam& param)
{
    OVDBG_LEVEL2("%s:Enter ",__func__);
    int32_t ret = 0;
    mLocationType = param.location;
    mTextColor    = param.text_color;
    mText.clear();
    mText.setTo(param.user_text, strlen(param.user_text) + 1);
    markDirty(true);
    OVDBG_LEVEL2("%s:Exit ",__func__);
    return ret;
}

int32_t OverlayItemText::createSurface()
{
    OVDBG_LEVEL2("%s: Enter", __func__);
    int32_t size = mWidth * mHeight * 4;
    ionMemInfo memInfo;
    memset(&memInfo, 0x0, sizeof(ionMemInfo));

    auto ret = allocateIonMemory(memInfo, size);
    if(0 != ret) {
       OVDBG_ERROR("%s:allocateIonMemory failed",__func__);
       return ret;
    }
    void* pixels = memInfo.vaddr;
    OVDBG_INFO("%s: Ion memory allocated fd = %d", __func__, memInfo.fd);
#if USE_SKIA
    //Create Skia canvas outof ION memory.
    SkImageInfo imageInfo;
    memset(&imageInfo, 0x0, sizeof(imageInfo));
    imageInfo.fWidth     = mWidth;
    imageInfo.fHeight    = mHeight;
    imageInfo.fColorType = kRGBA_8888_SkColorType;
    imageInfo.fAlphaType = kPremul_SkAlphaType;

    mCanvas = SkCanvas::NewRasterDirect(imageInfo, memInfo.vaddr,
                                        mWidth *4);
    if(!mCanvas) {
        OVDBG_ERROR("%s: Skia Creation failed!!",__func__);
        goto ERROR;
    }
#endif
    //Draw system time on Skia canvas.
    updateAndDraw();

    //Setup c2d.
    ret = c2dMapAddr(memInfo.fd, memInfo.vaddr, memInfo.size,
                       0, KGSL_USER_MEM_TYPE_ION, &mGpuAddr);
    if(ret != C2D_STATUS_OK) {
       OVDBG_ERROR("%s: c2dMapAddr failed!",__func__);
       goto ERROR;
    }

    C2D_RGB_SURFACE_DEF c2dSurfaceDef;
    c2dSurfaceDef.format = C2D_FORMAT_SWAP_ENDIANNESS | C2D_COLOR_FORMAT_8888_RGBA;
    c2dSurfaceDef.width  = mWidth;
    c2dSurfaceDef.height = mHeight;
    c2dSurfaceDef.buffer = memInfo.vaddr;
    c2dSurfaceDef.phys   = mGpuAddr;
    c2dSurfaceDef.stride = mWidth * 4;

    //Create source c2d surface.
    ret = c2dCreateSurface(&mC2dSurfaceId, C2D_SOURCE,
                          (C2D_SURFACE_TYPE)(C2D_SURFACE_RGB_HOST
                           |C2D_SURFACE_WITH_PHYS), &c2dSurfaceDef);
    if(ret != C2D_STATUS_OK) {
       OVDBG_ERROR("%s: c2dCreateSurface failed!",__func__);
       goto ERROR;
    }

    mIonFd        = memInfo.fd;
    mVaddr        = memInfo.vaddr;
    mSize         = memInfo.size;
    mHandleData   = memInfo.handleData;

    OVDBG_INFO("%s: Exit", __func__);
    return ret;

ERROR:
    ioctl(mIonDevice, ION_IOC_FREE, &mHandleData);
    close(mIonFd);
    mIonFd = -1;
    return ret;
}


OverlayItemPrivacyMask::OverlayItemPrivacyMask(int32_t ionDeviceId)
                        :OverlayItem(ionDeviceId)
{
    OVDBG_LEVEL2("%s: Enter", __func__);
    mType = OverlayType::kPrivacyMask;
    OVDBG_LEVEL2("%s: Exit", __func__);
}

OverlayItemPrivacyMask::~OverlayItemPrivacyMask()
{
    OVDBG_LEVEL2("%s: Enter", __func__);
    OVDBG_LEVEL2("%s: Exit", __func__);
}

int32_t OverlayItemPrivacyMask::init(OverlayParam& param)
{
    OVDBG_LEVEL2("%s: Enter", __func__);

    if((param.bounding_box.width <= 0) || (param.bounding_box.height <= 0)) {
        return BAD_VALUE;
    }
    if(param.bounding_box.startX < 0 || param.bounding_box.startY < 0) {
        return BAD_VALUE;
    }

    mX          = param.bounding_box.startX;
    mY          = param.bounding_box.startY;
    mWidth      = param.bounding_box.width;
    mHeight     = param.bounding_box.height;

    auto ret = createSurface();
    if(ret != 0) {
        OVDBG_ERROR("%s: createSurface failed!", __func__);
        return NO_INIT;
    }
    OVDBG_LEVEL2("%s: Exit", __func__);
    return ret;
}

int32_t OverlayItemPrivacyMask::updateAndDraw()
{
    //Nothing to update, contents are static.
    //Never marked as dirty.
    return OK;
}

void OverlayItemPrivacyMask::getDrawInfo(uint32_t targetWidth,
                                          uint32_t targetHeight,
                                          DrawInfo* drawInfo)
{
    OVDBG_LEVEL2("%s: Enter", __func__);
    drawInfo->x            = mX;
    drawInfo->y            = mY;
    drawInfo->width        = mWidth;
    drawInfo->height       = mHeight;
    drawInfo->c2dSurfaceId = mC2dSurfaceId;
    OVDBG_LEVEL2("%s: Exit", __func__);
}

void OverlayItemPrivacyMask::getParameters(OverlayParam& param)
{
    OVDBG_LEVEL2("%s:Enter ",__func__);
    param.type      = OverlayType::kPrivacyMask;
    param.location  = OverlayLocationType::kNone;
    param.bounding_box.startX = mX;
    param.bounding_box.startY = mY;
    param.bounding_box.width  = mWidth;
    param.bounding_box.height = mHeight;
    OVDBG_LEVEL2("%s:Exit ",__func__);
}

int32_t OverlayItemPrivacyMask::updateParameters(OverlayParam& param)
{
    OVDBG_LEVEL2("%s:Enter ",__func__);
    int32_t ret = 0;

    if((param.bounding_box.width <= 0) || (param.bounding_box.height <= 0)) {
        return BAD_VALUE;
    }
    if(param.bounding_box.startX < 0 || param.bounding_box.startY < 0) {
        return BAD_VALUE;
    }
    mX          = param.bounding_box.startX;
    mY          = param.bounding_box.startY;
    mWidth      = param.bounding_box.width;
    mHeight     = param.bounding_box.height;

    OVDBG_LEVEL2("%s:Exit ",__func__);
    return ret;
}

int32_t OverlayItemPrivacyMask::createSurface()
{
    OVDBG_LEVEL2("%s: Enter", __func__);

    int32_t size = mWidth * mHeight * 4;
    uint32_t color = PRIVACY_MASK_COLOR;
    ionMemInfo memInfo;

    memset(&memInfo, 0x0, sizeof(ionMemInfo));
    auto ret = allocateIonMemory(memInfo, size);
    if(0 != ret) {
        OVDBG_ERROR("%s:allocateIonMemory failed",__func__);
        return ret;
    }
    void* pixels = memInfo.vaddr;
    OVDBG_LEVEL1("%s: Ion memory allocated fd(%d)", __func__, memInfo.fd);

#if USE_SKIA
    //Create Skia canvas outof ION memory.
    SkPaint paintBox;
    SkImageInfo imageInfo;
    memset(&imageInfo, 0x0, sizeof(imageInfo));
    imageInfo.fWidth     = mWidth;
    imageInfo.fHeight    = mHeight;
    imageInfo.fColorType = kRGBA_8888_SkColorType;
    imageInfo.fAlphaType = kPremul_SkAlphaType;

    mCanvas = SkCanvas::NewRasterDirect(imageInfo, memInfo.vaddr,
        mWidth *4);
    if(!mCanvas) {
        OVDBG_ERROR("%s: Skia Creation failed!!", __func__);
        goto ERROR;
    }

#ifndef DEBUG_BACKGROUND_SURFACE
        mCanvas->clear(SK_AlphaOPAQUE);
#else
        mCanvas->clear(SK_ColorDKGRAY);
#endif

    paintBox.setColor(color);
    paintBox.setStyle(SkPaint::kFill_Style);
    //For blurring effect
    paintBox.setMaskFilter(SkBlurMaskFilter::Create(kNormal_SkBlurStyle,5.0f, 0));
    OVDBG_LEVEL2(" mX %d mY %d mWidth %d mHeight %d",mX,mY,mWidth,mHeight);
    mCanvas->drawRect(SkRect::MakeXYWH(0,0, mWidth, mHeight), paintBox);
    mCanvas->flush();
#endif
    //Setup c2d.
    ret = c2dMapAddr(memInfo.fd, memInfo.vaddr, memInfo.size,
        0, KGSL_USER_MEM_TYPE_ION, &mGpuAddr);
    if(ret != C2D_STATUS_OK) {
        OVDBG_ERROR("%s: c2dMapAddr failed!", __func__);
        goto ERROR;
    }

    C2D_RGB_SURFACE_DEF c2dSurfaceDef;
    c2dSurfaceDef.format = C2D_FORMAT_SWAP_ENDIANNESS | C2D_COLOR_FORMAT_8888_RGBA;
    c2dSurfaceDef.width  = mWidth;
    c2dSurfaceDef.height = mHeight;
    c2dSurfaceDef.buffer = memInfo.vaddr;
    c2dSurfaceDef.phys   = mGpuAddr;
    c2dSurfaceDef.stride = mWidth*4;

    //Create source c2d surface.
    ret = c2dCreateSurface(&mC2dSurfaceId, C2D_SOURCE,
        (C2D_SURFACE_TYPE)(C2D_SURFACE_RGB_HOST
        |C2D_SURFACE_WITH_PHYS), &c2dSurfaceDef);
    if(ret != C2D_STATUS_OK) {
        OVDBG_ERROR("%s: c2dCreateSurface failed!", __func__);
        goto ERROR;
     }

    mIonFd        = memInfo.fd;
    mVaddr        = memInfo.vaddr;
    mSize         = memInfo.size;
    mHandleData   = memInfo.handleData;

    OVDBG_LEVEL2("%s: Exit", __func__);
    return ret;

ERROR:
    ioctl(mIonDevice, ION_IOC_FREE, &mHandleData);
    close(mIonFd);
    mIonFd = -1;
    return ret;
}

}; // namespace overlay

}; // namespace qmmf
