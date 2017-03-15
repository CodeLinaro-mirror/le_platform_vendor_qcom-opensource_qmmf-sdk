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

#define TAG "ReprocessFactory"

#include "../modules/camera-hal/qmmf_camera_hal.h"
#include "../modules/jpeg-encoder/qmmf_camera_jpeg.h"
#include "../modules/simple/qmmf_camera_simple.h"
#include "../modules/copy/qmmf_camera_copy.h"
#include "../modules/haze-buster/qmmf_camera_haze_buster.h"

#include "qmmf_camera_factory.h"

namespace qmmf {

namespace recorder {

sp<ReprocessFactory> ReprocessFactory::instance_ = NULL;
int32_t ReprocessFactory::ids_ = 0x00f00000;

sp<ReprocessFactory> ReprocessFactory::getInstance() {
  if (instance_.get() == NULL) {
    instance_ = new ReprocessFactory();
  }
  return instance_;
}

void ReprocessFactory::releaseInstance() {
  QMMF_INFO("%s: destroying instance object.", __func__);
  if (instance_.get() == NULL) {
    QMMF_INFO("%s: Reset reprocess Ids.", __func__);
    ids_ = 0x00f00000;
  }
  instance_.clear();
}

ReprocessFactory::ReprocessFactory() {
}

ReprocessFactory::~ReprocessFactory() {
}

int32_t ReprocessFactory::GetId() {
  return ids_++;
}

sp<ICameraModule>
ReprocessFactory::getReprocEngine(String8 name,
                                  IPostProcCameraContext* context) {
  ICameraModule* instance = nullptr;

  if (name == "JpegEncode") {
    instance = new CameraJpeg(GetId());
  } else if (name == "HALJpegEncode") {
    instance = new CameraHal(context);
  } else if (name == "Simple") {
    instance = new CameraSimple(GetId());
  } else if (name == "Copy") {
    instance = new CameraCopy(GetId());
  } else if (name == "HazeBuster") {
    instance = new CameraHazeBuster(GetId());
  } else {
    QMMF_ERROR("%s: Invalid reprocess engine!", __func__);
  }

  if (instance != nullptr) {
    return sp<ICameraModule>(instance);
  } else {
    return nullptr;
  }
}

}; // namespace recoder

}; // namespace qmmf
