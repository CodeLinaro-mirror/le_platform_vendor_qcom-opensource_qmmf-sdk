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

#include <sys/prctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "qmmf_camera_thread.h"
#include "common/qmmf_log.h"

namespace qmmf {

namespace recorder {

using ::std::string;

CameraThread::CameraThread() : pid_(0), running_(false) {
  pthread_mutex_init(&thread_lock_, NULL);
}

CameraThread::~CameraThread() {
  RequestExitAndWait();
  pthread_mutex_destroy(&thread_lock_);
}

int32_t CameraThread::Run(const char *name) {
  int32_t res = 0;

  pthread_mutex_lock(&thread_lock_);
  if (running_) {
    QMMF_ERROR("%s: Thread already started!\n", __func__);
    res = -ENOSYS;
    goto exit;
  }

  running_ = true;
  res = pthread_create(&pid_, NULL, MainLoop, this);
  if (0 != res) {
    QMMF_ERROR("%s: Unable to create thread\n", __func__);
    goto exit;
  }

  if (NULL != name) {
    name_ = name;
  } else {
    name_ = "Camera-Reprocess";
  }

exit:
  pthread_mutex_unlock(&thread_lock_);

  return res;
}

void CameraThread::RequestExit() {
  pthread_mutex_lock(&thread_lock_);
  running_ = false;
  pthread_mutex_unlock(&thread_lock_);
}

void CameraThread::RequestExitAndWait() {
  pthread_mutex_lock(&thread_lock_);
  if (0 != pid_) {
    running_ = false;
    pthread_mutex_unlock(&thread_lock_);

    pthread_join(pid_, NULL);

    pthread_mutex_lock(&thread_lock_);
    pid_ = 0;
  }
  pthread_mutex_unlock(&thread_lock_);
}

void *CameraThread::MainLoop(void *userdata) {
  bool run = true;
  CameraThread *pme = reinterpret_cast<CameraThread *>(userdata);
  if (NULL == pme) {
    return NULL;
  }

  prctl(PR_SET_NAME, (unsigned long)pme->name_.c_str(), 0, 0, 0);
  while (run) {
    pthread_mutex_lock(&pme->thread_lock_);
    run = pme->running_;
    pthread_mutex_unlock(&pme->thread_lock_);

    run &= pme->ThreadLoop();
  }

  return NULL;
}

bool CameraThread::ExitPending() {
  pthread_mutex_lock(&thread_lock_);
  bool res = running_ ? false : true;
  pthread_mutex_unlock(&thread_lock_);
  return res;
}

}  // namespace recorder ends here

}  // namespace qmmf ends here
