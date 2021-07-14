/*
* Copyright (c) 2021 The Linux Foundation. All rights reserved.
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

#include "qmmf_recorder_common.h"
#include <pthread.h>

typedef void(*BufferCallBack)(void*, int);



class CameraRecorder : public RecorderCommon  {
 public:
  CameraRecorder();

  ~CameraRecorder();

  void StartCamera(BufferCallBack p_func, void* input);

  void StopCamera();

  int GetCameraNum(){
    TEST_INFO("GetCameraNum:%d:this:%p", Camera_Number,this);
    return Camera_Number;
  }


 private:
    VideoStreamInfo     stream;
    uint32_t            video_track_1 = kFirstStreamID;
    uint32_t            width = 0;
    uint32_t            height = 0;
    uint32_t            linkStatus;
    int                 Camera_Number;
    uint32_t            m_processStatus = 0;
    pthread_t           m_processThread;   ///< Thread creation object
    pthread_cond_t      m_processSignal;     ///< Signal to control when buffer is empty or not
    pthread_mutex_t     m_processMutex;      ///< Mutex to protect queue access
    BufferCallBack      m_bufferCallBack;
    void*               input_Buffer;
    streamInfo_t*       in_Buffer;

    void DoProcess();

    void DoBuffer();

    static void* DoThreadFunction(
        void* pThis)
    {
        if (NULL != pThis)
        {
            (static_cast<CameraRecorder *>(pThis))->DoProcess();
        }

        return NULL;
    }

   static void* DoBufferFunction(
        void* pThis)
    {
        if (NULL != pThis)
        {
            (static_cast<CameraRecorder *>(pThis))->DoBuffer();
        }

        return NULL;
    }

};



