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

#include "qmmf_transcode_track.h"

namespace qmmf {
namespace transcode {

TranscoderTrack::TranscoderTrack(char*file) : isFirstFrame_(true),
    isLastFrame_(false), input_stop_(false) {
  TEST_INFO("%s:%s:%s Enter", TAG, __func__, "TRACK");
  memset(&params_, 0x0, sizeof(params_));
  if (file) {
    strncpy(params_.track_file_,file,MAX_FILE_NAME);
  } else {
    memset(params_.track_file_, 0x0, sizeof(params_.track_file_));
  }
  TEST_INFO("%s:%s:%s Exit", TAG, __func__, "TRACK");
}

TranscoderTrack::~TranscoderTrack() {
  TEST_INFO("%s:%s:%s Enter", TAG, __func__, "TRACK");
  TEST_INFO("%s:%s:%s Exit", TAG, __func__, "TRACK");
}

status_t TranscoderTrack::PreparePipeline() {
  TEST_INFO("%s:%s:%s Enter", TAG, __func__, "TRACK");
  status_t ret = 0;

  ret = ParseFile(params_.track_file_, (void*)this);
  if (ret != 0) {
    TEST_ERROR("%s:%s:%s Failed to Parse file", TAG, __func__, "TRACK");
    return -1;
  }

  transcoder_pipe_ = make_shared<TransCoderPipe>(params_.track_type_);

  transcoder_core_ = make_shared<TransCoderCore>(params_.core_codec_type,
      params_.core_params_, transcoder_pipe_);

  transcoder_sink_ = make_shared<TransCoderSink>(params_.sink_codec_type,
      params_.sink_params_, transcoder_pipe_);

  ret = transcoder_core_->PreparePipeline();
  if (ret != 0) {
    TEST_ERROR("%s:%s:%s Failed to Prepare pipeline on core side",
        TAG, __func__, "TRACK");
    return ret;
  }

  ret = transcoder_sink_->PreparePipeline();
  if (ret != 0) {
    TEST_ERROR("%s:%s:%s Failed to Prepare pipeline on sink side",
        TAG, __func__, "TRACK");
    return ret;
  }

  ret = transcoder_pipe_->PreparePipeline();
  if (ret != 0) {
    TEST_ERROR("%s:%s:%s Failed to Prepare pipeline on pipe side",
        TAG, __func__, "TRACK");
    return ret;
  }

  TEST_INFO("%s:%s:%s Exit", TAG, __func__, "TRACK");
  return ret;
}

status_t TranscoderTrack::FillParams() {
  TEST_INFO("%s:%s:%s: Enter", TAG, __func__, "TRACK");

  CreateDataSource();

  ::qmmf::player::AudioTrackCreateParam audio_track_param_;
  ::qmmf::player::VideoTrackCreateParam video_track_param_;


  if (track_type_ == TrackTypes::kAudioVideo ||
      track_type_ == TrackTypes::kAudioOnly) {

    audio_track_param_.sample_rate = m_sTrackInfo_.sAudio.ulSampleRate;
    audio_track_param_.channels    = m_sTrackInfo_.sAudio.ulChCount;
    audio_track_param_.bit_depth   = 16; //TODO m_sTrackInfo_.sAudio.ulBitDepth;

    if (m_sTrackInfo_.sAudio.ulCodecType == 3) {
      audio_track_param_.codec       = ::qmmf::player::AudioCodecType::kAAC;
      audio_track_param_.codec_params.aac.bit_rate =
          m_sTrackInfo_.sAudio.ulBitRate;
      audio_track_param_.codec_params.aac.format   = AACFormat::kRaw;
      audio_track_param_.codec_params.aac.mode     = AACMode::kAALC;
    } else if (m_sTrackInfo_.sAudio.ulCodecType == 55) {  //need verification
      audio_track_param_.codec      = ::qmmf::player::AudioCodecType::kAMR;
      audio_track_param_.codec_params.amr.isWAMR   = 0;
    } else if (m_sTrackInfo_.sAudio.ulCodecType == 45) {  //need verification
      audio_track_param_.codec      = ::qmmf::player::AudioCodecType::kAMR;
      audio_track_param_.codec_params.amr.isWAMR   = 1;
    }
    audio_track_param_.out_device                = AudioOutSubtype::kBuiltIn;

    TEST_INFO("%s:%s:%s sample rate : %d channel %d bitdepth %d, bitrate %d ",
        TAG, __func__, "TRACK", audio_track_param_.sample_rate,
        audio_track_param_.channels, audio_track_param_.bit_depth,
        m_sTrackInfo_.sAudio.ulBitRate);
  }

  if (track_type_ == TrackTypes::kAudioVideo ||
      track_type_ == TrackTypes::kVideoOnly) {

    if (m_sTrackInfo_.sVideo.ulCodecType == 11) {
      video_track_param_.codec       = ::qmmf::player::VideoCodecType::kAVC;
    } else if (m_sTrackInfo_.sVideo.ulCodecType == 12) {
      video_track_param_.codec       = ::qmmf::player::VideoCodecType::kHEVC;
    }

    video_track_param_.frame_rate  = m_sTrackInfo_.sVideo.fFrameRate;
    video_track_param_.height      = m_sTrackInfo_.sVideo.ulHeight;
    video_track_param_.width       = m_sTrackInfo_.sVideo.ulWidth;
    video_track_param_.bitrate     = m_sTrackInfo_.sVideo.ulBitRate;
    video_track_param_.num_buffers = 1;
    video_track_param_.out_device  = VideoOutSubtype::kHDMI;

    TEST_INFO("%s:%s:%s height : %d width %d frame_rate %d, bitrate %d ",
        TAG, __func__, "TRACK", video_track_param_.height,
        video_track_param_.width, video_track_param_.frame_rate,
        video_track_param_.bitrate);
  }

  if (params_.track_type_ == TransCodeType::kVideoDecodeVideoEncode)
    params_.core_params_.video_dec_param = video_track_param_;
  else
    assert(0);

  TEST_INFO("%s:%s:%s: Exit", TAG, __func__, "TRACK");

  return 0;
}

status_t TranscoderTrack::CreateDataSource() {

  TEST_INFO("%s:%s:%s Enter", TAG, __func__, "TRACK");
  int32_t eErr = MM_STATUS_ErrorNone;

  m_pDemux_ = CMM_MediaDemuxInt::New(*m_pIStreamPort_, FILE_SOURCE_MPEG4);

  if (!m_pDemux_) {
    TEST_ERROR("%s %s DataSource CreationFAILURE!!", TAG, __func__, "TRACK");
    BAIL_ON_ERROR(MM_STATUS_ErrorDefault);
  }

  TEST_INFO("%s:%s:%s: DataSource Creation SUCCESS!!", TAG, __func__, "TRACK");

  //Read file meta-data
  eErr = ReadMediaInfo();
  BAIL_ON_ERROR(eErr);
  TEST_INFO("%s:%s:%s Exit", TAG, __func__, "TRACK");

ERROR_BAIL:
  return eErr;
}

status_t TranscoderTrack::ReadMediaInfo() {

  TEST_INFO("%s:%s:%s: Enter", TAG, __func__, "TRACK");

  status_t eErr = 0;
  FileSourceTrackIdInfoType aTrackList[MM_SOURCE_MAX_TRACKS];
  FileSourceMjMediaType eMjType = FILE_SOURCE_MJ_TYPE_UNKNOWN;
  FileSourceMnMediaType eMnType = FILE_SOURCE_MN_TYPE_UNKNOWN;
  FileSourceStatus eFS_Status = FILE_SOURCE_FAIL;
  track_type_ = TrackTypes::kAudioVideo;

  // Get total number of tracks available.
  m_sTrackInfo_.ulNumTracks = m_pDemux_->GetWholeTracksIDList(aTrackList);
  TEST_INFO("%s:%s:%s: NumTracks = %u",
      TAG, __func__, "TRACK", m_sTrackInfo_.ulNumTracks);

  for (uint32 ulIdx = 0; ulIdx < m_sTrackInfo_.ulNumTracks; ulIdx++) {
    FileSourceTrackIdInfoType sTrackInfo = aTrackList[ulIdx];

    // Get MimeType
    eFS_Status = m_pDemux_->GetMimeType(sTrackInfo.id, eMjType, eMnType);
    if(FILE_SOURCE_SUCCESS != eFS_Status) {
      TEST_INFO("%s:%s:%s: Unable to get MIME_TYPE = %u",
          TAG, __func__, "TRACK", eFS_Status);
      continue;
    }

    if (FILE_SOURCE_SUCCESS == eFS_Status ) {
      if ( FILE_SOURCE_MJ_TYPE_AUDIO == eMjType ) {
        TEST_INFO("%s:%s:%s: TRACK_AUDIO @MIME_TYPE = %u",
            TAG, __func__, "TRACK", eMnType);
        m_sTrackInfo_.sAudio.bTrackSelected = sTrackInfo.selected;

       TEST_INFO("%s : %s id:%d ", __func__, "TRACK", TAG, sTrackInfo.id);

        eErr = ReadAudioTrackMediaInfo(sTrackInfo.id, eMnType);
        if (m_sTrackInfo_.ulNumTracks  == 1) {
          track_type_ = TrackTypes::kAudioOnly;
        }

      } else if (FILE_SOURCE_MJ_TYPE_VIDEO == eMjType) {
        TEST_INFO("%s:%s:%s: TRACK_VIDEO @MIME_TYPE = %u",
            TAG, __func__, "TRACK", eMnType);

        m_sTrackInfo_.sVideo.bTrackSelected = sTrackInfo.selected;

        TEST_INFO("%s : %s id:%d ", __func__, "TRACK", TAG, sTrackInfo.id);

        eErr = ReadVideoTrackMediaInfo(sTrackInfo.id, eMnType);
        if (m_sTrackInfo_.ulNumTracks  == 1) {
          track_type_ = TrackTypes::kVideoOnly;
        }
      }
    } else {
      eErr = MM_STATUS_ErrorStreamCorrupt;
      TEST_ERROR("%s %s Failed to identify Tracks Error= %u",
          TAG, __func__, "TRACK", eFS_Status);
      BAIL_ON_ERROR(eErr);
    }
  }

  TEST_INFO("%s:%s:%s: Exit", TAG, __func__, "TRACK");

ERROR_BAIL:
  return eErr;
}

status_t TranscoderTrack::ReadAudioTrackMediaInfo(
    uint32 ulTkId,
    FileSourceMnMediaType eTkMnType) {

  TEST_INFO("%s:%s:%s: Enter", TAG, __func__, "TRACK");

  MM_STATUS_TYPE eErr = MM_STATUS_ErrorNone;
  FileSourceStatus eFS_Status = FILE_SOURCE_FAIL;
  MediaTrackInfo sMediaInfo;
  memset(&sMediaInfo, 0, sizeof(MediaTrackInfo));

  // Get max buffer size
  m_sTrackInfo_.sAudio.sSampleBuf.ulMaxLen = \
    m_pDemux_->GetTrackMaxFrameBufferSize(ulTkId);

  // Get track media information
  eFS_Status = m_pDemux_->GetMediaTrackInfo(ulTkId, &sMediaInfo);

  // Update track media information
  if (FILE_SOURCE_SUCCESS == eFS_Status) {
    m_sTrackInfo_.sAudio.ulTkId = ulTkId;
    m_sTrackInfo_.sAudio.ulCodecType = eTkMnType;
    m_sTrackInfo_.sAudio.ulChCount = \
                                sMediaInfo.audioTrackInfo.numChannels;
    m_sTrackInfo_.sAudio.ulBitRate = \
                                sMediaInfo.audioTrackInfo.bitRate;
    m_sTrackInfo_.sAudio.ulSampleRate = \
                                sMediaInfo.audioTrackInfo.samplingRate;
    m_sTrackInfo_.sAudio.ulBitDepth = \
                                sMediaInfo.audioTrackInfo.nBitsPerSample;
    m_sTrackInfo_.sAudio.ullDuration = \
                                sMediaInfo.audioTrackInfo.duration;
    m_sTrackInfo_.sAudio.ulTimeScale = \
                                sMediaInfo.audioTrackInfo.timeScale;

    TEST_INFO("%s:%s:%s:Audio CodecType is = %u ", TAG, __func__, "TRACK",
        m_sTrackInfo_.sAudio.ulCodecType);

    TEST_INFO("%s:%s:%s: TkId = %u CH= %u  SR= %u BD=%u",
        TAG, __func__, "TRACK", ulTkId, m_sTrackInfo_.sAudio.ulChCount,
        m_sTrackInfo_.sAudio.ulSampleRate, m_sTrackInfo_.sAudio.ulBitDepth);

    // Get track CSD data len
    eFS_Status = m_pDemux_->GetFormatBlock(ulTkId,
                                          nullptr,
                                          &m_sTrackInfo_.sAudio.sCSD.ulLen,
                                          FALSE);
    BAIL_ON_ERROR(eFS_Status);

    // Get track CSD data if CSD len is valid
    if (0 != m_sTrackInfo_.sAudio.sCSD.ulLen) {
      TEST_INFO("%s:%s:%s: CSD Len = %u", TAG, __func__, "TRACK",
          m_sTrackInfo_.sAudio.sCSD.ulLen);

      m_sTrackInfo_.sAudio.sCSD.pucData = \
        (uint8*)MM_Malloc(sizeof(uint8)* m_sTrackInfo_.sAudio.sCSD.ulLen);
      if (!m_sTrackInfo_.sAudio.sCSD.pucData) {
        eErr = MM_STATUS_ErrorMemAllocFail;
        BAIL_ON_ERROR(eErr);
      }
      eFS_Status = m_pDemux_->GetFormatBlock(ulTkId,
                                            m_sTrackInfo_.sAudio.sCSD.pucData,
                                            &m_sTrackInfo_.sAudio.sCSD.ulLen,
                                            FALSE);
      BAIL_ON_ERROR(eFS_Status);
    }
  }

  TEST_INFO("%s:%s:%s: Exit", TAG, __func__, "TRACK");

ERROR_BAIL:
  if (FILE_SOURCE_SUCCESS != eFS_Status) {
    eErr = MM_STATUS_ErrorDefault;
  }
  TEST_ERROR("%s:%s:%s: Return Status %u", TAG, __func__, "TRACK", eErr);
  return eErr;
}

status_t TranscoderTrack::ReadVideoTrackMediaInfo(
    uint32 ulTkId,
    FileSourceMnMediaType eTkMnType) {

  TEST_INFO("%s:%s:%s: Enter", TAG, __func__, "TRACK");

  MM_STATUS_TYPE eErr = MM_STATUS_ErrorNone;
  FileSourceStatus eFS_Status = FILE_SOURCE_FAIL;
  MediaTrackInfo sMediaInfo;
  memset(&sMediaInfo, 0, sizeof(MediaTrackInfo));

  m_sTrackInfo_.sVideo.sSampleBuf.ulMaxLen = \
    m_pDemux_->GetTrackMaxFrameBufferSize(ulTkId);
  eFS_Status = m_pDemux_->GetMediaTrackInfo(ulTkId, &sMediaInfo);
  if (FILE_SOURCE_SUCCESS == eFS_Status) {
    m_sTrackInfo_.sVideo.ulTkId = ulTkId;
    m_sTrackInfo_.sVideo.ulCodecType = sMediaInfo.videoTrackInfo.videoCodec;
    m_sTrackInfo_.sVideo.ulWidth = sMediaInfo.videoTrackInfo.frameWidth;
    m_sTrackInfo_.sVideo.ulHeight = sMediaInfo.videoTrackInfo.frameHeight;
    m_sTrackInfo_.sVideo.fFrameRate = sMediaInfo.videoTrackInfo.frameRate;
    m_sTrackInfo_.sVideo.ulBitRate = sMediaInfo.videoTrackInfo.bitRate;
    m_sTrackInfo_.sVideo.ullDuration = sMediaInfo.videoTrackInfo.duration;
    m_sTrackInfo_.sVideo.ulTimeScale = sMediaInfo.videoTrackInfo.timeScale;

    TEST_INFO("%s:%s:%s:Video CodecType is = %u ", TAG, __func__, "TRACK",
        m_sTrackInfo_.sVideo.ulCodecType);

    TEST_INFO("%s:%s:%s: TkId = %u Width= %u  Height= %u FR=%f bitrate = %u"
        "duration  = %llu", TAG, __func__, "TRACK", ulTkId,
        m_sTrackInfo_.sVideo.ulWidth, m_sTrackInfo_.sVideo.ulHeight,
        m_sTrackInfo_.sVideo.fFrameRate, m_sTrackInfo_.sVideo.ulBitRate,
        m_sTrackInfo_.sVideo.ullDuration);

    // Get CSD data len
    eFS_Status = m_pDemux_->GetFormatBlock(ulTkId,
                                          nullptr,
                                          &m_sTrackInfo_.sVideo.sCSD.ulLen,
                                          FALSE);
    BAIL_ON_ERROR(eFS_Status);
    if (0 != m_sTrackInfo_.sVideo.sCSD.ulLen) {
      TEST_INFO("%s:%s:%s: CSD Len = %u", TAG, __func__, "TRACK",
        m_sTrackInfo_.sVideo.sCSD.ulLen);

      m_sTrackInfo_.sVideo.sCSD.pucData = \
              (uint8*)MM_Malloc(sizeof(uint8)* m_sTrackInfo_.sVideo.sCSD.ulLen);
      if (!m_sTrackInfo_.sVideo.sCSD.pucData) {
        eErr = MM_STATUS_ErrorMemAllocFail;
        TEST_ERROR("%s %s CSD Alloc failure", TAG, __func__, "TRACK");
        BAIL_ON_ERROR(eErr);
      }
      eFS_Status = m_pDemux_->GetFormatBlock(ulTkId,
                                            m_sTrackInfo_.sVideo.sCSD.pucData,
                                            &m_sTrackInfo_.sVideo.sCSD.ulLen,
                                            FALSE);
      BAIL_ON_ERROR(eFS_Status);
    }
  }

  TEST_INFO("%s:%s:%s: Exit", TAG, __func__, "TRACK");

ERROR_BAIL:
  if(FILE_SOURCE_SUCCESS != eFS_Status){
    eErr = MM_STATUS_ErrorDefault;
  }

  TEST_ERROR("%s:%s:%s: Return Status %u", TAG, __func__, "TRACK", eErr);
  return eErr;
}

status_t TranscoderTrack::Start() {
  status_t ret = 0;
  TEST_INFO("%s:%s:%s Enter", TAG, __func__, "TRACK");

  ret = transcoder_core_->StartCodec();
  if (ret != 0) {
    TEST_ERROR("%s:%s:%s Failed to Start on core side", TAG, __func__, "TRACK");
    return ret;
  }

  ret = transcoder_sink_->StartCodec();
  if (ret != 0) {
    TEST_ERROR("%s:%s:%s Failed to Start on pipe side", TAG, __func__, "TRACK");
    return ret;
  }

  {
    Mutex::Autolock l(input_stop_lock_);
    input_stop_ = false;
  }

  pthread_create(&deliver_thread_, nullptr, DeliverInput, (void*)this);
  pthread_create(&receiver_thread_, nullptr, ReceiveOutput, (void*)this);

  TEST_INFO("%s:%s:%s Exit", TAG, __func__, "TRACK");

  return ret;
}

void* TranscoderTrack::DeliverInput(void* arg) {
  TEST_INFO("%s:%s:%s Enter", TAG, __func__, "TRACK");
  TranscoderTrack* track = static_cast<TranscoderTrack*>(arg);
  status_t ret = 0;
  while(1) {
    TransCodeBuffer buffer;
    ret = track->getInputBufferSource()->DequeInputBuffer(buffer);
    uint32_t csd_data_size = 0;
    FileSourceSampleInfo sSampleInfo;
    FileSourceMediaStatus eMediaStatus = FILE_SOURCE_DATA_ERROR;
    memset(&sSampleInfo, 0, sizeof(FileSourceSampleInfo));
    (track->m_sTrackInfo_).sVideo.sSampleBuf.ulLen =
      (track->m_sTrackInfo_).sVideo.sSampleBuf.ulMaxLen;
    if (track->isFirstFrame_) {
      uint32_t status = track->m_pDemux_->m_pFileSource->GetFormatBlock(
          (track->m_sTrackInfo_).sVideo.ulTkId, nullptr, &csd_data_size);

      TEST_INFO("%s:%s:%s Video CSD data Size = %u", TAG, __func__, "TRACK",
          csd_data_size);
      assert(FILE_SOURCE_SUCCESS == status);

      status = track->m_pDemux_->m_pFileSource->GetFormatBlock(
          (track->m_sTrackInfo_).sVideo.ulTkId,
          static_cast<uint8_t*>(buffer.data()), &csd_data_size);
      assert(FILE_SOURCE_SUCCESS == status);
      track->isFirstFrame_ = false;
    }

    eMediaStatus = track->m_pDemux_->GetNextMediaSample(
        (track->m_sTrackInfo_).sVideo.ulTkId,
        static_cast<uint8_t*>(buffer.data()) + csd_data_size,
        &((track->m_sTrackInfo_).sVideo.sSampleBuf.ulLen), sSampleInfo);
    buffer.FilledSize() = (track->m_sTrackInfo_).sVideo.sSampleBuf.ulLen +
        csd_data_size;
    // Multiplying the timestamp to convert it into nano seconds
    buffer.Ts() = 1000*(sSampleInfo.startTime);
    buffer.Flag() = 0x0;
    buffer.Offset() = 0x0;

    if (FILE_SOURCE_DATA_END == eMediaStatus || track->IsInputPortStop()) {
      TEST_INFO("%s:%s:%s: File read completed", TAG, __func__, "TRACK");
      buffer.FilledSize() = 0;
      buffer.Flag() = EOS_FLAG;
      buffer.Offset() = 0x0;

      track->isLastFrame_ = true;

      TEST_INFO("%s:%s:%s Video Ts[%llu] filled_size[%u] flags[0x%x]  fd[%d]",
          TAG, __func__, "TRACK", buffer.Ts(), buffer.FilledSize(),
          buffer.Flag(), buffer.Fd());
      ret = track->getInputBufferSource()->QueueInputBuffer(buffer);
      assert(ret == 0);

      track->Stop();
      break;
    }

    TEST_INFO("%s:%s:%s Video Ts[%llu] filled_size[%u] flags[0x%x]  fd[%d]",
          TAG, __func__, "TRACK", buffer.Ts(), buffer.FilledSize(),
          buffer.Flag(), buffer.Fd());
    ret = track->getInputBufferSource()->QueueInputBuffer(buffer);
    assert(ret == 0);

  }
  TEST_INFO("%s:%s:%s Exit", TAG, __func__, "TRACK");
  return nullptr;
}

void* TranscoderTrack::ReceiveOutput(void* arg) {
  TEST_INFO("%s:%s:%s Enter", TAG, __func__, "TRACK");
  status_t ret = 0;
  TranscoderTrack* track = static_cast<TranscoderTrack*>(arg);
  FILE* file = fopen(track->params_.output_file_, "w");
  while(1) {
    TransCodeBuffer buffer;
    ret = track->getOutputBufferSource()->DequeInputBuffer(buffer);
    assert(ret == 0);
    TEST_INFO("%s:%s:%s Video Ts[%llu] filled_size[%u] flags[0x%x]  fd[%d]",
        TAG, __func__, "TRACK", buffer.Ts(), buffer.FilledSize(),
        buffer.Flag(), buffer.Fd());
    fwrite(static_cast<void*>(static_cast<uint32_t*>(buffer.data())
        + buffer.Offset()), 1, buffer.FilledSize(), file);
    ret = track->getOutputBufferSource()->QueueInputBuffer(buffer);
    assert(ret == 0);
    if((buffer.Flag() & EOS_FLAG) || track->IsInputPortStop()) {
      fclose(file);
      break;
    }
  }
  TEST_INFO("%s:%s:%s Exit", TAG, __func__, "TRACK");
  return nullptr;
}

status_t TranscoderTrack::Stop() {
  status_t ret = 0;
  TEST_INFO("%s:%s:%s Enter", TAG, __func__, "TRACK");

  {
    Mutex::Autolock l(input_stop_lock_);
    input_stop_ = true;
  }

  pthread_join(deliver_thread_, NULL);
  pthread_join(receiver_thread_, NULL);

  ret = transcoder_core_->StopCodec();
  if (ret != 0) {
    TEST_ERROR("%s:%s:%s Failed to Stop on core side", TAG, __func__, "TRACK");
    return ret;
  }

  ret = transcoder_sink_->StopCodec();
  if (ret != 0) {
    TEST_ERROR("%s:%s:%s Failed to Stop on pipe side", TAG, __func__, "TRACK");
    return ret;
  }

  TEST_INFO("%s:%s:%s Exit", TAG, __func__, "TRACK");
  return ret;
}

inline bool TranscoderTrack::IsInputPortStop() {
  Mutex::Autolock l(input_stop_lock_);
  return input_stop_;
}

status_t TranscoderTrack::Delete() {
  status_t ret = 0;
  TEST_INFO("%s:%s:%s Enter", TAG, __func__, "TRACK");

  ret = transcoder_core_->DeleteCodec();
  if (ret != 0) {
    TEST_ERROR("%s:%s:%s Failed to Delete on core side",
        TAG, __func__, "TRACK");
    return ret;
  }
  ret = transcoder_sink_->DeleteCodec();
  if (ret != 0) {
    TEST_ERROR("%s:%s:%s Failed to Delete on pipe side",
        TAG, __func__, "TRACK");
    return ret;
  }
  ret = transcoder_pipe_->RemovePipe();
  if (ret != 0) {
    TEST_ERROR("%s:%s:%s Failed to Remove Pipe", TAG, __func__, "TRACK");
    return ret;
  }
  // Need to be discussed
  // MM_FREE(m_sTrackInfo_.sAudio.sCSD.pucData);
  // MM_FREE(m_sTrackInfo_.sVideo.sCSD.pucData);

  TEST_INFO("%s:%s:%s Exit", TAG, __func__, "TRACK");
  return ret;
}

/* This function will fill the following parameters
1. Sink Codec Params structure or Core Codec Params structure
2. track_file_name, input_file_name and output_file_name
3. Transcode Type, Core Codec Type, Sink Codec Type
4. What ever has been chossen for filling in 1, the opposite one
will be filled by demuxer
*/
status_t TranscoderTrack::ParseFile(char*filename, void*arg) {
  status_t ret = 0;
  TEST_INFO("%s:%s:%s Enter", TAG, __func__, "TRACK");
  TranscoderTrack* track = static_cast<TranscoderTrack*>(arg);
  if (!track) {
    TEST_ERROR("%s:%s:%s TransCodeParams not provided", TAG, __func__, "TRACK");
    return -1;
  }

  strncpy(track->params_.input_file_,"/data/4K_input.mp4", MAX_FILE_NAME);
  strncpy(track->params_.output_file_,"/data/1080p_output.h264", MAX_FILE_NAME);
  track->params_.track_type_ = TransCodeType::kVideoDecodeVideoEncode;
  track->params_.core_codec_type = CodecType::kVideoDecoder;
  track->params_.sink_codec_type = CodecType::kVideoEncoder;

  // Filling Sink Params
  track->params_.sink_params_.video_enc_param.camera_id = NOT_REQUIRED;
  track->params_.sink_params_.video_enc_param.width = 1920;
  track->params_.sink_params_.video_enc_param.height = 1080;
  track->params_.sink_params_.video_enc_param.frame_rate = 15;
  track->params_.sink_params_.video_enc_param.format_type = VideoFormat::kAVC;
  track->params_.sink_params_.video_enc_param.codec_param.avc.idr_interval = 1;
  track->params_.sink_params_.video_enc_param.codec_param.avc.bitrate = 10000000;
  track->params_.sink_params_.video_enc_param.codec_param.avc.profile = AVCProfileType::kBaseline;
  track->params_.sink_params_.video_enc_param.codec_param.avc.level = AVCLevelType::kLevel3;
  track->params_.sink_params_.video_enc_param.codec_param.avc.ratecontrol_type = VideoRateControlType::kMaxBitrate;
  track->params_.sink_params_.video_enc_param.codec_param.avc.qp_params.enable_init_qp = true;
  track->params_.sink_params_.video_enc_param.codec_param.avc.qp_params.init_qp.init_IQP = 26;
  track->params_.sink_params_.video_enc_param.codec_param.avc.qp_params.init_qp.init_PQP = 26;
  track->params_.sink_params_.video_enc_param.codec_param.avc.qp_params.init_qp.init_BQP = 26;
  track->params_.sink_params_.video_enc_param.codec_param.avc.qp_params.init_qp.init_QP_mode = 0x7;
  track->params_.sink_params_.video_enc_param.codec_param.avc.qp_params.enable_qp_range = true;
  track->params_.sink_params_.video_enc_param.codec_param.avc.qp_params.qp_range.min_QP = 1;
  track->params_.sink_params_.video_enc_param.codec_param.avc.qp_params.qp_range.max_QP = 51;
  track->params_.sink_params_.video_enc_param.codec_param.avc.qp_params.enable_qp_IBP_range = true;
  track->params_.sink_params_.video_enc_param.codec_param.avc.qp_params.qp_IBP_range.min_IQP = 1;
  track->params_.sink_params_.video_enc_param.codec_param.avc.qp_params.qp_IBP_range.max_IQP = 51;
  track->params_.sink_params_.video_enc_param.codec_param.avc.qp_params.qp_IBP_range.min_PQP = 1;
  track->params_.sink_params_.video_enc_param.codec_param.avc.qp_params.qp_IBP_range.max_PQP = 51;
  track->params_.sink_params_.video_enc_param.codec_param.avc.qp_params.qp_IBP_range.min_BQP = 1;
  track->params_.sink_params_.video_enc_param.codec_param.avc.qp_params.qp_IBP_range.max_BQP = 51;
  track->params_.sink_params_.video_enc_param.codec_param.avc.ltr_count = 4;
  track->params_.sink_params_.video_enc_param.codec_param.avc.hier_layer = 0;
  track->params_.sink_params_.video_enc_param.codec_param.avc.prepend_sps_pps_to_idr = true;
  track->params_.sink_params_.video_enc_param.codec_param.avc.insert_aud_delimiter = true;
  track->params_.sink_params_.video_enc_param.codec_param.avc.sar_enabled = false;
  track->params_.sink_params_.video_enc_param.codec_param.avc.sar_width = 0;
  track->params_.sink_params_.video_enc_param.codec_param.avc.sar_height = 0;
  track->params_.sink_params_.video_enc_param.out_device = NOT_REQUIRED;
  track->params_.sink_params_.video_enc_param.low_power_mode = NOT_REQUIRED;

  //Filling Core Params
  if (track->params_.track_type_ == TransCodeType::kVideoDecodeVideoEncode) {
    if((track->params_).input_file_ != nullptr) {
      track->m_pIStreamPort_ = new CMM_MediaSourcePort((track->params_).input_file_);
      ret = track->FillParams();
      assert(ret == 0);
    } else {
      TEST_ERROR("%s:%s:%s Input File Name not found", TAG, __func__, "TRACK");
      return -1;
    }
  } else{
    TEST_ERROR("%s:%s:%s TranscodType Not supported", TAG, __func__, "TRACK");
    return -1;
  }

  track->params_.core_params_.video_dec_param.enable_downscalar = true;
  track->params_.core_params_.video_dec_param.output_height = 1080;
  track->params_.core_params_.video_dec_param.output_width = 1920;
  track->params_.core_params_.video_dec_param.frame_rate  = 15;

  TEST_INFO("%s:%s:%s Exit", TAG, __func__, "TRACK");
  return ret;
}

};  //namespace transcode
};  //namespace qmmf