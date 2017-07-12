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

#include "../modules/camera-hal-jpeg/qmmf_camera_hal_jpeg.h"
#include "../modules/camera-hal-reproc/qmmf_camera_hal_reproc.h"
#include "../modules/jpeg-encoder/qmmf_jpeg.h"
#include "../modules/test/qmmf_postproc_test.h"
#include "../modules/algo/qmmf_postproc_algo.h"

#include "qmmf_postproc_factory.h"

namespace qmmf {

namespace recorder {

sp<PostProcFactory> PostProcFactory::instance_ = NULL;
int32_t PostProcFactory::ids_ = 0x00f00000;

sp<PostProcFactory> PostProcFactory::getInstance() {
  if (instance_.get() == NULL) {
    instance_ = new PostProcFactory();
  }
  return instance_;
}

void PostProcFactory::releaseInstance() {
  QMMF_INFO("%s: destroying instance object.", __func__);
  if (instance_.get() == NULL) {
    QMMF_INFO("%s: Reset reprocess Ids.", __func__);
    ids_ = 0x00f00000;
  }
  instance_.clear();
}

PostProcFactory::PostProcFactory() {
}

PostProcFactory::~PostProcFactory() {
}

int32_t PostProcFactory::GetId() {
  return ids_++;
}

sp<IPostProcModule>
PostProcFactory::getReprocEngine(std::string name, IPostProc* context) {
  sp<IPostProcModule> instance;

  if (name == "JpegEncode") {
    instance = new PostProcJpeg(GetId());
  } else if (name == "HALJpegEncode") {
    instance = new PostProcHalJpeg(context);
  } else if (name == "Test") {
    instance = new PostProcTest(GetId());
  } else if (name == "HazeBuster") {
    instance = new PostProcAlg(GetId(), "libqmmf_alg_hazebuster.so");
  } else if (name == "EdgeSmooth") {
    instance = new PostProcAlg(GetId(), "libqmmf_alg_es.so");
  } else if (name == "HALReprocess") {
    instance = new CameraHalReproc(context);
  } else if (name == "BayerLcac") {
    instance = new PostProcAlg(GetId(), "libqmmf_alg_lcac.so");
  } else {
    QMMF_ERROR("%s: Invalid post process engine: %s", __func__, name.c_str());
  }

  return instance;
}

}; // namespace recoder

}; // namespace qmmf
