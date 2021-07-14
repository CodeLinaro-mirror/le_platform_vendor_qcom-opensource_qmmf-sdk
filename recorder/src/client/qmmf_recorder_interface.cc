/*
* Copyright (c) 2021, The Linux Foundation. All rights reserved.
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

#define LOG_TAG "CameraRecorder"

#include "qmmf-sdk/qmmf_recorder_interface.h"



CameraRecorder::CameraRecorder()
{
    TEST_INFO("%s: Enter....", __func__);
    pthread_mutex_init(&m_processMutex, NULL);
    pthread_cond_init(&m_processSignal, NULL);

    pthread_mutex_init(&m_bufferMutex, NULL);
    pthread_cond_init(&m_bufferSignal, NULL);

    pthread_mutex_lock(&m_processMutex);
    if (processInit != m_processStatus)
    {
        m_processStatus  = processInit;
    }
    pthread_mutex_unlock(&m_processMutex);

    int ret = pthread_create(&m_processThread, NULL, DoThreadFunction, this);
    assert(ret == 0);

    ret = pthread_create(&m_bufferThread, NULL, DoBufferFunction, this);
    assert(ret == 0);
    TEST_INFO("%s: Exit....", __func__);


}


CameraRecorder::~CameraRecorder()
{
    TEST_INFO("%s: Enter....", __func__);
    pthread_mutex_lock(&m_processMutex);
    if (processStop == m_processStatus || processInit == m_processStatus){
        m_processStatus = processDestroy;
    }else if (processStart == m_processStatus) {
        pthread_mutex_lock(&m_processMutex);
        m_processStatus = processStop;
        pthread_cond_signal(&m_processSignal);
        pthread_mutex_unlock(&m_processMutex);
        sleep(5);
        m_processStatus = processDestroy;
    }
    pthread_cond_signal(&m_processSignal);
    pthread_mutex_unlock(&m_processMutex);

    pthread_mutex_lock(&m_bufferMutex);
    pthread_cond_signal(&m_bufferSignal);
    pthread_mutex_unlock(&m_bufferMutex);

    pthread_join(m_processThread, NULL);
    pthread_join(m_bufferThread, NULL);

    pthread_mutex_destroy(&m_bufferMutex);
    pthread_cond_destroy(&m_bufferSignal);
    pthread_mutex_destroy(&m_processMutex);
    pthread_cond_destroy(&m_processSignal);
    TEST_INFO("%s: Exit....", __func__);

}


void CameraRecorder::DoBuffer(){
    int i;
    TEST_INFO("%s: Enter....", __func__);
    while (m_processStatus != processDestroy && m_processStatus != processStop){
        if(m_processStatus == processStart){
            for(i = 0; i < in_Buffer->bufMemNum; i++){
                if(in_Buffer->recorderBuffers[i].reference == 1){
                    m_bufferCallBack(input_Buffer, i);
                }
            }
        }
        else{
            TEST_INFO("%s: Create buffer thread \n", __func__);
        }
        pthread_mutex_lock(&m_bufferMutex);
        pthread_cond_wait(&m_bufferSignal, &m_bufferMutex);
        pthread_mutex_unlock(&m_bufferMutex);
    }

    if(m_processStatus == processDestroy || m_processStatus == processStop)
    {
        TEST_INFO("%s: Do buffer thread Destroy of Stop\n", __func__);
    }
    TEST_INFO("%s: Exit....", __func__);
}

void CameraRecorder::DoProcess(){
    VideoFormat         format;
    float               fps;
    uint32_t            session_id;
    CameraExtraParam    camera_extra_param;
    int32_t             ret;

    TEST_INFO("%s: Enter....", __func__);
    while (m_processStatus != processDestroy){
        if(m_processStatus == processStart)
        {
            TEST_INFO("%s: Start Camera recorder  width：%d, height：%d, fps:%f", __func__, width,
                      height, fps);

            if(width == 0 || height == 0){
               int CamNum =  GetCameraNum();
            }
            GetBufferNumber(in_Buffer->bufMemNum);
            GetSensorNumber(in_Buffer->Camera_ID);
            SessionCb session_status_cb = CreateSessionStatusCb();

            ret = recorder_.CreateSession(session_status_cb, &session_id);
            assert(session_id > 0);
            assert(ret == NO_ERROR);

            if (dump_bitstream_.IsEnabled() &&
                (format == VideoFormat::kAVC || format == VideoFormat::kHEVC)) {
              // Dump Encoded Streams
              StreamDumpInfo dumpinfo = {format, session_id, video_track_1, width,
                                         height};
              ret = dump_bitstream_.SetUp(dumpinfo);
              assert(ret == NO_ERROR);
            }

            // Configure Single Video Stream
            VideoTrackCreateParam video_track_param{camera_id_, format, width, height,
                                                    fps};

            TrackCb video_track_cb;
            video_track_cb.data_cb = [&, session_id](
                uint32_t track_id, std::vector<BufferDescriptor> buffers,
                std::vector<MetaData> meta_buffers) {
              if (format == VideoFormat::kYUY2) {
                VideoTrackYUVDataCb(session_id, track_id, buffers, meta_buffers, in_Buffer);
              }
            };

            video_track_cb.event_cb = [&](uint32_t track_id, EventType event_type,
                                          void *event_data, size_t event_data_size) {
              VideoTrackEventCb(track_id, event_type, event_data, event_data_size);
            };

            ret = recorder_.CreateVideoTrack(session_id, video_track_1,
                                             video_track_param, video_track_cb);
            assert(ret == NO_ERROR);

            std::vector<uint32_t> track_ids;
            track_ids.push_back(video_track_1);
            sessions_.insert(std::make_pair(session_id, track_ids));


            // Start Session
            ret = recorder_.StartSession(session_id);
            assert(ret == NO_ERROR);
            TEST_INFO("Opened Camera recorder");

        }
        else if(m_processStatus == processStop)
        {
            TEST_INFO("%s:Stop Camera recorder  Camera id%d", __func__, camera_id_);
            ret = recorder_.StopSession(session_id, false);
            assert(ret == NO_ERROR);

            ret = recorder_.DeleteVideoTrack(session_id, video_track_1);
            assert(ret == NO_ERROR);

            ret = recorder_.DeleteSession(session_id);
            assert(ret == NO_ERROR);

            ClearSessions();
            dump_bitstream_.CloseAll();


        }
        else
        {
            SetUp();
            ret = Init();
            assert(ret == NO_ERROR);

            stream           = stream_info_map_[video_track_1];
            width            = stream.width;
            height           = stream.height;
            format           = stream.format;
            linkStatus       = 0;
            fps              = stream.fps;
            TEST_INFO("%s:Init Camera recorder  Camera id%d", __func__, camera_id_);
            ret = recorder_.StartCamera(camera_id_, camera_fps_, camera_extra_param);
            assert(ret == NO_ERROR);
            ret =  recorder_.GetCameraParam(camera_id_, static_info_);
            assert(ret == NO_ERROR);
            linkStatus = GetLinkStatus();
            TEST_INFO("%s:Init Camera recorder  linkStatus 0x%x", __func__, linkStatus);
            if(linkStatus == 0xF){
                height = height * 4;
                Camera_Number = 4;
            }else if (linkStatus == 0xE || linkStatus == 0xD ||
                      linkStatus == 0xB || linkStatus == 0x7){
                height = height * 3;
                Camera_Number = 3;
            }else if(linkStatus == 0xC || linkStatus == 0xA ||
                     linkStatus == 0x9 || linkStatus == 0x6 ||
                     linkStatus == 0x5 || linkStatus == 0x3){
                height = height * 2;
                Camera_Number = 2;
            }else if(link_status == 0x1 || link_status == 0x2 ||
                     link_status == 0x4 || link_status == 0x8){
                Camera_Number = 1;
            }
            TEST_INFO("GetCameraNum:%d:this:%p", Camera_Number,this);
            stream.width = height;
            snap_height_ = height;
            stream_info_map_.emplace(kFirstStreamID, stream);
        }

        pthread_mutex_lock(&m_processMutex);
        pthread_cond_wait(&m_processSignal, &m_processMutex);
        pthread_mutex_unlock(&m_processMutex);
    }

    if(m_processStatus == processDestroy)
    {
        TEST_INFO("%s:Destory Camera recorder Enter", __func__);
        ret = recorder_.StopCamera(camera_id_);
        assert(ret == NO_ERROR);
        ret = DeInit();
        assert(ret == NO_ERROR);
        TEST_INFO("%s:Destory Camera recorder", __func__);
    }
    TEST_INFO("%s: Exit....", __func__);
    return;
}


void CameraRecorder::StartCamera(BufferCallBack p_func, void* buffer)
{
    TEST_INFO("%s: Enter....", __func__);
    m_bufferCallBack = p_func;
    input_Buffer     = buffer;
    pthread_mutex_lock(&m_processMutex);
    m_processStatus = processStart;
    in_Buffer = (streamInfo_t*)buffer;
    pthread_cond_signal(&m_processSignal);
    pthread_mutex_unlock(&m_processMutex);
    TEST_INFO("%s: Exit....", __func__);
}

void CameraRecorder::StopCamera()
{
    TEST_INFO("%s: Enter....", __func__);
    pthread_mutex_lock(&m_processMutex);
    m_processStatus = processStop;
    pthread_cond_signal(&m_processSignal);
    pthread_mutex_unlock(&m_processMutex);

    pthread_mutex_lock(&m_bufferMutex);
    pthread_cond_signal(&m_bufferSignal);
    pthread_mutex_unlock(&m_bufferMutex);
    TEST_INFO("%s: Exit....", __func__);

}

