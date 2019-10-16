/*
 * Copyright (c) 2018, 2019, The Linux Foundation. All rights reserved.
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

#ifdef TARGET_USES_GRALLOC1
#include <grallocusage/GrallocUsageConversion.h>
#include <libgralloc1/gralloc_priv.h>
#elif TARGET_USES_GBM
#include <gbm_priv.h>
#ifdef __LIBGBM__
typedef const struct gbm_bo* buffer_handle_t;
#else
#include <system/window.h>
#endif

// todo: add and move to platform specific header
#define HAL_PIXEL_FORMAT_RAW8                    0x123
#define HAL_PIXEL_FORMAT_NV12_ENCODEABLE         0x102
#define HAL_PIXEL_FORMAT_NV21_ZSL                0x113
#define GRALLOC_USAGE_PRIVATE_ALLOC_UBWC         0x10000000
#define GRALLOC_USAGE_PRIVATE_UNCACHED           0x02000000
#define HAL_PIXEL_FORMAT_YCbCr_420_SP_VENUS      0x7FA30C04
#define HAL_PIXEL_FORMAT_YCbCr_420_SP_VENUS_UBWC 0x7FA30C06

#ifdef __LIBGBM__
struct private_handle_t : public gbm_bo {
#else
struct private_handle_t : public native_handle {
#endif
  enum {
      PRIV_FLAGS_FRAMEBUFFER = 0x00000001,
      PRIV_FLAGS_VIDEO_ENCODER = 0x00010000
  };

  int fd;
  int flags;
  unsigned int  size;
  unsigned int  offset;
  int bufferType;
  int format;
  int width;   // holds aligned width of the actual buffer allocated
  int height;  // holds aligned height of the  actual buffer allocated
  int unaligned_width;   // holds width client asked to allocate
  int unaligned_height;  // holds height client asked to allocate

  static const int sNumFds = 2;
  static inline int sNumInts() {
#ifdef __LIBGBM__
      return (((sizeof(private_handle_t) - sizeof(struct gbm_bo*)) /
              sizeof(int)) - sNumFds);
#else
      return (((sizeof(private_handle_t) - sizeof(native_handle_t)) /
              sizeof(int)) - sNumFds);
#endif
  }

  private_handle_t(int fd, unsigned int size, int flags, int bufferType,
      int format, int width, int height) :
      fd(fd), flags(flags), size(size), offset(0), bufferType(bufferType),
      format(format), width(width), height(height), unaligned_width(width),
      unaligned_height(height) {
#ifndef __LIBGBM__
    version = (int) sizeof(native_handle);
    numInts = sNumInts();
    numFds = sNumFds;
#endif
  };

  ~private_handle_t() {
  };
};

#else
#include <qcom/display/gralloc_priv.h>
#endif
#include <unordered_map>

/** MemAllocError
* @Fail - error while memory allocator operation
* @Ok - memory allocator operation complete with success
*
* Enumeration type for memory allocator operations result.
*
**/
enum class MemAllocError { kAllocFail = -1, kAllocOk = 0 };

/** MemAllocFlags
* @flags - member to hold actual common flags
*
* Abstract interface for allocator usage flags. The main purpose of this class
* is to avoid confusion of native vs abstracted flags - as typically these are
* kept as bit fields in an integer variable so compiler cannot make difference.
*
**/
class MemAllocFlags {
 public:
  int flags;
  MemAllocFlags() { flags = 0; }
  MemAllocFlags(int32_t new_flags) { flags = new_flags; }

  /** MemAllocFlags::Equals
  *
  * Compares with another set of flags. Returns true if both are same.
  *
  **/
  bool Equals(const MemAllocFlags& to) const { return flags == to.flags; }
};

/** MemAllocFlags
* @kHwCameraZsl - stream will be used for ZSL capture
* @kPrivateAllocUbwc - buffer will contain UBWC formatted data
* @kPrivateIommUHeap - buffer will be mapped to IOMMU
* @kPrivateMmHeap -
* @kPrivateUncached - buffer will be uncached
* @kProtected - buffer will be protected
* @kSwReadOften - buffer will be used for SW read
* @kSwWriteOften - buffer will be used for SW write
* @kHwFb - buffer will be used for HW read/write
* @kVideoEncoder - buffer will be used by encoder
*
* Abstract class providing definitions and convertion methods for usage flags
* Does not contain any variable data.
*
**/
class IMemAllocUsage {
 public:
  static const int kHwCameraZsl;
  static const int kPrivateAllocUbwc;
  static const int kPrivateIommUHeap;
  static const int kPrivateMmHeap;
  static const int kPrivateUncached;
  static const int kProtected;
  static const int kSwReadOften;
  static const int kSwWriteOften;
  static const int kHwFb;
  static const int kVideoEncoder;

  /** IMemAllocUsage::ToLocal
  *
  * Converts common usage flags (as int) to native flags
  *
  **/
  virtual int32_t ToLocal(int32_t common) const = 0;

  /** IMemAllocUsage::ToLocal
  *
  * Converts common usage flags (as MemAllocFlags) to native flags
  *
  **/
  virtual int32_t ToLocal(MemAllocFlags common) const = 0;

  /** IMemAllocUsage::ToLocal
  *
  * Converts native usage flags to common flags
  *
  **/
  virtual MemAllocFlags ToCommon(int32_t local) const = 0;
  virtual ~IMemAllocUsage(){};
};

/** IBufferInterface
*
* Abstract interface for mem buffer objects acquired via some allocator
*
**/
class IBufferInterface {
 public:
  virtual ~IBufferInterface(){};

  /** GetFD
  *
  * Returns file descriptor of the buffer
  *
  **/
  virtual int GetFD() = 0;

  /** GetFormat
  *
  * Returns format of the buffer
  *
  **/
  virtual int GetFormat() = 0;

  /** GetSize
  *
  * Returns size of the buffer
  *
  **/
  virtual uint32_t GetSize() = 0;

  /** GetWidth
  *
  * Returns width of the buffer
  *
  **/
  virtual uint32_t GetWidth() = 0;

  /** GetHeight
  *
  * Returns height of the buffer
  *
  **/
  virtual uint32_t GetHeight() = 0;
};

/** IBufferHandle
*
* Type for buffer objects used by interface classes described here
*
**/
typedef IBufferInterface* IBufferHandle;

/** IAllocDevice
*
* Abstract interface to the memory allocator device.
*
**/
class IAllocDevice {
 public:
  /** AllocDeviceAction
  * @GetStride - reads stride from handle
  * @GetHeight - reads height from handle
  * @GetAlignedWidth - reads aligned width in pixels from handle
  * @GetAlignedHeight - reads aligned height in pixels from handle
  *
  * Enumeration type for performing action on BufferHandler.
  *
  **/
  enum class AllocDeviceAction {GetMetaFd, GetStride, GetHeight,
                                GetAlignedWidth, GetAlignedHeight};

  virtual ~IAllocDevice(){};

  /** IAllocDevice::AllocBuffer
  * @handle - handle to the allocated buffer
  * @width - width of the buffer
  * @height - height of the buffer
  * @format - format of the buffer
  * @usage - usage flags of the buffer
  * @stride - returned: result stride for the allocated buffer according format
  *           width and usage
  *
  * Allocates buffer with given dimensions, format and usage
  *
  * Returns MemAllocError::kAllocOk - buffer is allocated successfully
  *         MemAllocError::kAllocFail - otherwise
  *
  **/
  virtual MemAllocError AllocBuffer(IBufferHandle& handle, int32_t width,
                                    int32_t height, int32_t format,
                                    MemAllocFlags usage,
                                    uint32_t* stride) = 0;

  virtual MemAllocError ImportBuffer(IBufferHandle& handle,
                                     void* native_handle) = 0;

  /** IAllocDevice::FreeBuffer
  * @handle - handle to the allocated buffer
  *
  * Frees buffer with given handle
  *
  * Returns MemAllocError::kAllocOk - buffer is freed successfully
  *         MemAllocError::kAllocFail - otherwise
  *
  **/
  virtual MemAllocError FreeBuffer(IBufferHandle handle) = 0;

  /** IAllocDevice::MapBuffer
  * @handle - handle to the allocated buffer
  * @sx - start horizontal offset of mapped area in the buffer
  * @sy - start vertical offset of mapped area in the buffer
  * @width - width of mapped area in the buffer
  * @height - height of mapped area in the buffer
  * @usage - usage flags of the buffer
  * @vaddr - Returned: virtual address to mapped area
  *
  * Maps area of the buffer for SW access
  *
  * Returns MemAllocError::kAllocOk - buffer is mapped successfully
  *         MemAllocError::kAllocFail - otherwise
  *
  **/
  virtual MemAllocError MapBuffer(const IBufferHandle& handle, int32_t start_x,
                                  int32_t start_y, int32_t width, int32_t height,
                                  MemAllocFlags usage, void** vaddr) = 0;

  /** IAllocDevice::UnmapBuffer
  * @handle - handle to the allocated buffer
  *
  * Unmaps area of the buffer mapped with IAllocDevice::MapBuffer
  *
  * Returns MemAllocError::kAllocOk - buffer is unmapped successfully
  *         MemAllocError::kAllocFail - otherwise
  *
  **/
  virtual MemAllocError UnmapBuffer(const IBufferHandle& handle) = 0;

  /** IAllocDevice::Perform
  * @handle - handle to the allocated buffer
  * @action - generic action to be performed
  * @result - returned: result for given action
  * Performs action and returns result
  *
  * Returns MemAllocError::kAllocOk - operation is successful
  *         MemAllocError::kAllocFail - otherwise
  *
  **/
  virtual MemAllocError Perform(const IBufferHandle& handle,
                                AllocDeviceAction action,
                                void *result) = 0;
};

/** AllocDeviceFactory
*
* Factory class producing new allocator device. Currently type of device is
* determined compile time, however interface can support runtime decision
*
**/
class AllocDeviceFactory {
 public:
  static IAllocDevice* CreateAllocDevice();
  static void DestroyAllocDevice(IAllocDevice* alloc_device_interface);
};

/** AllocUsageFactory
*
* Factory class producing new usage flag type converting class. Currently type
* is determined compile time, however interface can support runtime decision
**/
class AllocUsageFactory {
 public:
  static const IMemAllocUsage& GetAllocUsage();
};

/* APIs to get allocator dependent handles and devices - only if their use
   cannot be avoided (for interfacing outside QMMF)
 */

// Support for code with hard dependency to native handles.
#ifdef TARGET_USES_GRALLOC1
buffer_handle_t &GetAllocBufferHandle(const IBufferHandle &handle);
gralloc1_device_t *GetAllocDeviceHandle(const IAllocDevice &handle);
#elif TARGET_USES_GRALLOC2
buffer_handle_t &GetAllocBufferHandle(const IBufferHandle &handle);
gralloc2_device_t *GetAllocDeviceHandle(const IAllocDevice &handle);
#elif TARGET_USES_GBM
struct gbm_bo *GetAllocBufferHandle(const IBufferHandle &handle);
struct gbm_device *GetAllocDeviceHandle(const IAllocDevice &handle);
buffer_handle_t &GetGrallocBufferHandle(const IBufferHandle &handle);
#else
buffer_handle_t &GetAllocBufferHandle(const IBufferHandle &handle);
alloc_device_t *GetAllocDeviceHandle(const IAllocDevice &handle);
#endif
