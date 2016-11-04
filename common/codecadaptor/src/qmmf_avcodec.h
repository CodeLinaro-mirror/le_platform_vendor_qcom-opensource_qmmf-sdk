/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *     Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *     Neither the name of The Linux Foundation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.

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

#include <fcntl.h>

#include <iomanip>
#include <string>
#include <sstream>
#include <mutex>
#include <chrono>
#include <condition_variable>

#include <utils/String8.h>
#include <OMX_QCOMExtns.h>
#include <utils/RefBase.h>
#include <linux/msm_ion.h>
#include <OMX_VideoExt.h>
#include <OMX_IndexExt.h>
#include <libstagefrighthw/QComOMXMetadata.h>
#include <media/hardware/HardwareAPI.h>

#include "qmmf_omx_client.h"
#include "qmmf_avcodec_common.h"


namespace qmmf {

using namespace android;

#define INPUT_MAX_COUNT   11
#define OUTPUT_MAX_COUNT   6
#define CMD_BUF_MAX_COUNT 10

typedef struct CodecBuffer {
  void*     pointer;
  int32_t   fd;
  size_t    frame_length;
  size_t    filled_length;
  uint64_t  ts;
  int32_t   flag;
  struct    ion_handle_data handle_data;
  uint32_t  offset_to_frame;

  ::std::string ToString() const {
    ::std::stringstream stream;
    stream << "pointer[" << pointer << "] ";
    stream << "fd[" << fd << "] ";
    stream << "frame_length[" << frame_length << "] ";
    stream << "filled_length[" << filled_length << "] ";
    stream << "ts[" << ts << "] ";
    stream << "flag[" << ::std::setbase(16) << flag << ::std::setbase(10)
           << "] ";
    return stream.str();
  }
}CodecBuffer;

typedef struct CodecCmdType {
  OMX_EVENTTYPE   event_type;
  OMX_COMMANDTYPE event_cmd;
  OMX_U32         event_data;
  OMX_ERRORTYPE   event_result;
  OMX_U32         event_flags;
}CodecCmdType;

#define OMX_SPEC_VERSION 0x00000101

template<class T>
static void InitOMXParams(T *params) {
  memset(params, 0x0, sizeof(T));
  params->nSize = sizeof(T);
  params->nVersion.nVersion = OMX_SPEC_VERSION;
}

class IInputCodecSource : public RefBase {
 public:
  virtual ~IInputCodecSource() {}

  // this method provides an input buffer to the AVCodec
  virtual status_t Read(StreamBuffer& stream_buffer) = 0;

  // this method is used by AVCodec to return buffer after encoding
  virtual status_t SignalBufferReturned(StreamBuffer& stream_buffer) = 0;

  // this method is used by AVCodec to notify stop
  virtual status_t NotifyStatus(CodecInputPortStatus status) = 0;
};

class IOutputCodecSource : public RefBase {
 public:
  virtual ~IOutputCodecSource() {};

  // this method provides free output port buffer to AVCodec
  virtual status_t GetBuffer(CodecBuffer& codec_buffer) = 0;

  // this method provides filled output buffer to track
  virtual status_t ReturnBuffer(CodecBuffer& codec_buffer) = 0;
};

#define Log2(number, power)                   \
  { OMX_U32 temp = number; power = 0;         \
  while( (0 == (temp & 0x1)) &&  power < 16)  \
  { temp >>=0x1; power++; } }

#define FractionToQ16(q,num,den)     \
  { OMX_U32 power; Log2(den,power);  \
  q = num << (16 - power); }

#define OMX_PORT_NAME(port) (port == kPortIndexInput ? "IN_PORT" : "OUT_PORT")

#define OMX_STATE_NAME(state)                            \
  (state == OMX_StateInvalid ? "OMX_StateInvalid" :      \
  (state == OMX_StateLoaded ? "OMX_StateLoaded" :        \
  (state == OMX_StateIdle ? "OMX_StateIdle" :            \
  (state == OMX_StateExecuting ? "OMX_StateExecuting" :  \
  (state == OMX_StatePause ? "OMX_StatePause" :          \
  "Unknown")))))

class OmxClient;
class AVCodec : public RefBase {

public:
  AVCodec();

  ~AVCodec();

  status_t GetComponentRole(char *role, uint32_t *num_comps, OMX_U8 **comp_names);

  status_t ConfigureCodec(CodecType codec_type, CodecCreateParam& codec_param);

  status_t SetParameters(CodecParamType param_type, void *codec_param,
                         size_t param_size);

  status_t GetBufferRequirements(OMX_U32 port_index, uint32_t *buf_count,
                                 uint32_t *buf_size);

  status_t UseBuffer(OMX_U32 port, void *imp);

  status_t ReleaseBuffer();

  status_t StartCodec();

  status_t StopCodec();

  status_t PauseCodec();

  status_t ResumeCodec();

  status_t Flush(OMX_U32 nPortIndex);

private:
  // Create OMX Handle
  status_t CreateHandle(char *component_Name);

  status_t DeleteHandle();

  status_t ConfigureVideoEncoder(CodecCreateParam& codec_param);

  status_t ConfigureAudioEncoder(CodecCreateParam& codec_param);

  status_t ConfigureAudioDecoder(CodecCreateParam& codec_param);

  status_t ConfigureAudioCodec(uint32_t sample_rate, uint32_t channels,
                               uint32_t bit_depth, AudioFormat format_type,
                               AudioCodecParams codec_param);

  status_t SetupAVCEncoderParameters(CodecCreateParam& codec_param);

  status_t SetupHEVCEncoderParameters(CodecCreateParam& codec_param);

  status_t ConfigureBitrate(CodecCreateParam& codec_param);

  status_t SetPortParams(OMX_U32 ePortIndex,OMX_U32 nWidth, OMX_U32 nHeight,
                         OMX_U32 nFrameRate);

  status_t GetVideoProfile(CodecCreateParam& codec_param);

  status_t GetVideoLevel(CodecCreateParam& codec_param);

  status_t SetState(OMX_STATETYPE eState, OMX_BOOL bSynchronous);

  status_t WaitState(OMX_STATETYPE state);

  status_t PushEventCommand(OMX_EVENTTYPE event, OMX_COMMANDTYPE command,
                            OMX_U32, OMX_U32 flag);

  status_t EmptyThisBuffer(OMX_BUFFERHEADERTYPE *buffer);

  status_t FillThisBuffer(OMX_BUFFERHEADERTYPE *buffer);

  OMX_BUFFERHEADERTYPE* GetBufferHdr(StreamBuffer &stream_buffer);

  OMX_BUFFERHEADERTYPE* GetBufferHdr(CodecBuffer &codec_buffer);

  bool IsInputPortStop();

  bool IsOutputPortStop();

  void StopOutput();

  status_t FreeBufferPool();

  IInputCodecSource* getInputBufferSource() {return input_source_;}

  IOutputCodecSource* getOutputBufferSource() {return output_source_;}

  //DeliverInput thread will pull data to be encoded.
  static void* DeliverInput(void *ptr);

  //DeliverOutput thread will pull bitstream encoded data from Encoder.
  static void* DeliverOutput(void *ptr);

  //DeliverEvent will notify event from OMX component.
  void DeliverEvent(OMX_EVENTTYPE event, OMX_U32 data1, OMX_U32 data2);

  static OMX_ERRORTYPE OnEvent(OMX_IN OMX_HANDLETYPE component,
                               OMX_IN OMX_PTR app_data,
                               OMX_IN OMX_EVENTTYPE event,
                               OMX_IN OMX_U32 data1,
                               OMX_IN OMX_U32 data2,
                               OMX_IN OMX_PTR event_data);

  static OMX_ERRORTYPE OnEmptyBufferDone(OMX_IN OMX_HANDLETYPE component,
                                         OMX_IN OMX_PTR app_data,
                                         OMX_IN OMX_BUFFERHEADERTYPE *buffer);

  static OMX_ERRORTYPE OnFillBufferDone(OMX_IN OMX_HANDLETYPE component,
                                        OMX_IN OMX_PTR app_data,
                                        OMX_IN OMX_BUFFERHEADERTYPE *puffer);

  void UpdateBufferHeaderList(OMX_BUFFERHEADERTYPE* header);

  sp<OmxClient>           omx_client_;
  AVCodecEventCb          event_cb_;
  OMX_STATETYPE           state_;
  OMX_STATETYPE           state_pending_;
  bool                    input_stop_;
  bool                    output_stop_;
  //port_status_ will give whether both port is enable or disable.
  bool                    port_status_;
  Mutex                   input_stop_lock_;
  Mutex                   output_stop_lock_;
  pthread_t               read_thread_;
  IInputCodecSource*      input_source_;
  IOutputCodecSource*     output_source_;
  OMX_BUFFERHEADERTYPE**  in_buff_hdr_;
  OMX_BUFFERHEADERTYPE**  out_buff_hdr_;

  TSQueue<OMX_BUFFERHEADERTYPE*> free_input_buffhdr_list_;
  TSQueue<OMX_BUFFERHEADERTYPE*> used_input_buffhdr_list_;
  std::mutex                     lock_;
  std::condition_variable        wait_for_header_;
  std::mutex                     queue_lock_;

  uint32_t                in_buff_hdr_size_;
  uint32_t                out_buff_hdr_size_;
  CodecCmdType            cmd_buffer_[CMD_BUF_MAX_COUNT];
  uint32_t                cmd_buffer_index_;
  SignalQueue<void *>     signal_queue_;
  static OMX_CALLBACKTYPE callbacks_;
  CodecType               format_type_;
  //to handle the two EOS callbacks from Audio OMX component
  bool                    isEOSonOutput;
}; // class AVCodec
} // namespace qmmf
