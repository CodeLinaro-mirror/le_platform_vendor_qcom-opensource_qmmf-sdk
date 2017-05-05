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

#define TAG "CameraAlg"

#include <algorithm>
#include <cstdlib>
#include <dlfcn.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "qmmf_camera_alg.h"
#include "common/qmmf_log.h"

namespace qmmf {

namespace recorder {

CameraAlg::CameraAlg(const char *alg_lib_path) {
  assert(alg_lib_path != nullptr);

  void* handle = dlopen(alg_lib_path, RTLD_NOW);
  if (nullptr == handle) {
    QMMF_ERROR("%s:%s: Failed to open %s, error: %s", TAG, __func__,
        alg_lib_path, dlerror());
    assert(0);
  }

  kAlgName_ = alg_lib_path;
  alg_lib_.handle = handle;

  char prop[PROPERTY_VALUE_MAX];
  property_get("persist.qmmfalg.kpi.debug", prop, "0");
  kpi_debug_ = atoi(prop);

  *(void **) &alg_lib_.init       = dlsym(handle, "qmmf_alg_init");
  *(void **) &alg_lib_.deinit     = dlsym(handle, "qmmf_alg_deinit");
  *(void **) &alg_lib_.get_caps   = dlsym(handle, "qmmf_alg_get_caps");
  *(void **) &alg_lib_.set_tuning = dlsym(handle, "qmmf_alg_set_tuning");
  *(void **) &alg_lib_.config     = dlsym(handle, "qmmf_alg_config");
  *(void **) &alg_lib_.flush      = dlsym(handle, "qmmf_alg_flush");
  *(void **) &alg_lib_.process    = dlsym(handle, "qmmf_alg_process");
  *(void **) &alg_lib_.register_bufs =
      dlsym(handle, "qmmf_alg_register_bufs");
  *(void **) &alg_lib_.unregister_bufs =
      dlsym(handle, "qmmf_alg_unregister_bufs");
  *(void **) &alg_lib_.get_debug_info_log =
      dlsym(handle, "qmmf_alg_get_debug_info_log");

  if (!alg_lib_.init || !alg_lib_.deinit ||
      !alg_lib_.get_caps || !alg_lib_.set_tuning ||
      !alg_lib_.get_debug_info_log || !alg_lib_.register_bufs ||
      !alg_lib_.unregister_bufs || !alg_lib_.flush ||
      !alg_lib_.process || !alg_lib_.config) {
    QMMF_ERROR("%s:%s: Unable to link all symbols, Alg: %s", TAG, __func__,
        kAlgName_);
    QMMF_ERROR("%s:%s: qmmf_alg_init %p", TAG, __func__,
        alg_lib_.init);
    QMMF_ERROR("%s:%s: qmmf_alg_deinit %p", TAG, __func__,
        alg_lib_.deinit);
    QMMF_ERROR("%s:%s: qmmf_alg_get_caps %p", TAG, __func__,
        alg_lib_.get_caps);
    QMMF_ERROR("%s:%s: qmmf_alg_set_tuning %p", TAG, __func__,
        alg_lib_.set_tuning);
    QMMF_ERROR("%s:%s: qmmf_alg_config %p", TAG, __func__,
        alg_lib_.config);
    QMMF_ERROR("%s:%s: qmmf_alg_register_bufs %p", TAG, __func__,
        alg_lib_.register_bufs);
    QMMF_ERROR("%s:%s: qmmf_alg_unregister_bufs %p", TAG, __func__,
        alg_lib_.unregister_bufs);
    QMMF_ERROR("%s:%s: qmmf_alg_flush %p", TAG, __func__,
        alg_lib_.flush);
    QMMF_ERROR("%s:%s: qmmf_alg_process %p", TAG, __func__,
        alg_lib_.process);
    QMMF_ERROR("%s:%s: qmmf_alg_get_debug_info_log %p", TAG, __func__,
        alg_lib_.get_debug_info_log);
    assert(0);
  }
}

CameraAlg::~CameraAlg() {
  if (nullptr != alg_lib_.handle) {
    auto ret = dlclose(alg_lib_.handle);
    if (NO_ERROR != ret) {
      QMMF_ERROR("%s:%s: Failed to close Alg: %s error: %s", TAG, __func__,
          kAlgName_, dlerror());
    }
  }
}

}  // namespace recorder ends here

}  // namespace qmmf ends here
