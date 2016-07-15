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

#define LOG_TAG "AudioEndPointClient"

#include <binder/Parcel.h>
#include <binder/ProcessState.h>
#include <binder/IPCThreadState.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include "qmmf_audio_endpoint_client.h"
#include "qmmf_audio_common.h"

namespace qmmf {
namespace audio {

/**
This file has implementation of following classes:

- AudioEndPointClient    : Delegation to binder proxy <IAudioService>
                           and implementation of binder CB.
- BpAudioService         : Binder proxy implementation.
- BpAudioServiceCallback : Binder CB proxy implementation.
- BnAudioServiceCallback : Binder CB stub implementation.
*/

using namespace android;

AudioEndPointClient::AudioEndPointClient()
                : audio_service_(nullptr)
                , death_notifier_(nullptr)
{
    QMMF_INFO("%s: Enter", __func__);
    QMMF_INFO("%s: Exit", __func__);
}

AudioEndPointClient::~AudioEndPointClient()
{
    QMMF_INFO("%s: Enter", __func__);
    audio_service_.clear();
    death_notifier_.clear();
    client_handle_.clear();
    QMMF_INFO("%s: Exit", __func__);
}

status_t AudioEndPointClient::Connect(AudioEventCallback &cb)
{
    QMMF_INFO("%s: Enter", __func__);
    Mutex::Autolock lock(lock_);

    if (audio_service_.get() != nullptr) {
        QMMF_WARN("%s: already connected to service", __func__);
        return NO_ERROR;
    }

    sp<ProcessState> proc(ProcessState::self());
    proc->startThreadPool();
    audio_cb_ = cb;

    death_notifier_ = new DeathNotifier(this);
    if (death_notifier_.get() == nullptr) {
        QMMF_ERROR("%s: unable to allocate death notifier", __func__);
        return NO_MEMORY;
    }

    sp<IBinder> service_handle;
    sp<IServiceManager> service_manager = defaultServiceManager();

    service_handle = service_manager->getService(String16(QMMF_AUDIO_SERVICE_NAME));
    if (service_handle.get() == nullptr) {
        QMMF_ERROR("%s: can't get (%s) service", __func__,
                QMMF_AUDIO_SERVICE_NAME);
        return NO_INIT;
    }

    audio_service_ = interface_cast<IAudioService>(service_handle);
    IInterface::asBinder(audio_service_)->linkToDeath(death_notifier_);

    auto ret = audio_service_->Connect(this, client_handle_);
    if (ret != NO_ERROR) {
        QMMF_ERROR("%s: can't connect to (%s) service", __func__,
                             QMMF_AUDIO_SERVICE_NAME);
    }

    QMMF_INFO("%s: Exit", __func__);
    return ret;
}

status_t AudioEndPointClient::Disconnect()
{
    QMMF_INFO("%s: Enter", __func__);
    Mutex::Autolock lock(lock_);

    if (audio_service_.get() == NULL) {
        QMMF_WARN("%s: not connected to audio service", __func__);
        return NO_ERROR;
    }

    auto ret = audio_service_->Disconnect(client_handle_);
    if(ret != NO_ERROR) {
        QMMF_ERROR("%s: service->Disconnect failed: %d", __func__, ret);
    }

    audio_service_->asBinder(audio_service_)->unlinkToDeath(death_notifier_);
    audio_service_.clear();
    audio_service_ = NULL;

    death_notifier_.clear();
    death_notifier_ = NULL;

    client_handle_.clear();

    QMMF_INFO("%s: Exit", __func__);
    return ret;
}

status_t AudioEndPointClient::Configure(const AudioType type,
                                        const std::vector<DeviceID> &devices,
                                        const AudioParams &params)
{
    QMMF_INFO("%s: Enter", __func__);
    Mutex::Autolock lock(lock_);

    if (audio_service_.get() == NULL) {
        QMMF_WARN("%s: not connected to audio service", __func__);
        return NO_ERROR;
    }

    auto ret = audio_service_->Create(client_handle_, type, devices, params);
    if(ret != NO_ERROR) {
        QMMF_ERROR("%s: service->Create failed: %d", __func__, ret);
    }

    QMMF_INFO("%s: Exit", __func__);
    return ret;
}

status_t AudioEndPointClient::Start()
{
    QMMF_INFO("%s: Enter", __func__);
    Mutex::Autolock lock(lock_);

    if (audio_service_.get() == NULL) {
        QMMF_WARN("%s: not connected to audio service", __func__);
        return NO_ERROR;
    }

    auto ret = audio_service_->Start(client_handle_);
    if(ret != NO_ERROR) {
        QMMF_ERROR("%s: service->Start failed: %d", __func__, ret);
    }
    QMMF_INFO("%s: Exit", __func__);
    return ret;
}

status_t AudioEndPointClient::Stop(const bool do_flush)
{
    QMMF_INFO("%s: Enter", __func__);
    Mutex::Autolock lock(lock_);

    if (audio_service_.get() == NULL) {
        QMMF_WARN("%s: not connected to audio service", __func__);
        return NO_ERROR;
    }

    auto ret = audio_service_->Stop(client_handle_, do_flush);
    if(ret != NO_ERROR) {
        QMMF_ERROR("%s: service->Stop failed: %d", __func__, ret);
    }
    QMMF_INFO("%s: Exit", __func__);
    return ret;
}

status_t AudioEndPointClient::Pause()
{
    QMMF_INFO("%s: Enter", __func__);
    Mutex::Autolock lock(lock_);

    if (audio_service_.get() == NULL) {
        QMMF_WARN("%s: not connected to audio service", __func__);
        return NO_ERROR;
    }

    auto ret = audio_service_->Pause(client_handle_);
    if(ret != NO_ERROR) {
        QMMF_ERROR("%s: service->Pause failed: %d", __func__, ret);
    }
    QMMF_INFO("%s: Exit", __func__);
    return ret;
}

status_t AudioEndPointClient::Resume()
{
    QMMF_INFO("%s: Enter", __func__);
    Mutex::Autolock lock(lock_);

    if (audio_service_.get() == NULL) {
        QMMF_WARN("%s: not connected to audio service", __func__);
        return NO_ERROR;
    }

    auto ret = audio_service_->Resume(client_handle_);
    if(ret != NO_ERROR) {
        QMMF_ERROR("%s: service->Resume failed: %d", __func__, ret);
    }
    QMMF_INFO("%s: Exit", __func__);
    return ret;
}

status_t AudioEndPointClient::Read(const std::vector<AudioTrackBuffer> &buffers)
{
    QMMF_INFO("%s: Enter", __func__);
    Mutex::Autolock lock(lock_);

    if (audio_service_.get() == NULL) {
        QMMF_WARN("%s: not connected to audio service", __func__);
        return NO_ERROR;
    }

    auto ret = audio_service_->Read(client_handle_, buffers);
    if(ret != NO_ERROR) {
        QMMF_ERROR("%s: service->Read failed: %d", __func__, ret);
    }
    QMMF_INFO("%s: Exit", __func__);
    return ret;
}

status_t AudioEndPointClient::Write(const std::vector<AudioTrackBuffer> &buffers)
{
    QMMF_INFO("%s: Enter", __func__);
    Mutex::Autolock lock(lock_);

    if (audio_service_.get() == NULL) {
        QMMF_WARN("%s: not connected to audio service", __func__);
        return NO_ERROR;
    }

    auto ret = audio_service_->Write(client_handle_, buffers);
    if(ret != NO_ERROR) {
        QMMF_ERROR("%s: service->Write failed: %d", __func__, ret);
    }
    QMMF_INFO("%s: Exit", __func__);
    return ret;
}

status_t AudioEndPointClient::GetLatency(uint32_t &latency)
{
    QMMF_INFO("%s: Enter", __func__);
    Mutex::Autolock lock(lock_);

    if (audio_service_.get() == NULL) {
        QMMF_WARN("%s: not connected to audio service", __func__);
        return NO_ERROR;
    }

    auto ret = audio_service_->GetLatency(client_handle_, latency);
    if(ret != NO_ERROR) {
        QMMF_ERROR("%s: service->GetLatency failed: %d", __func__, ret);
    }
    QMMF_INFO("%s: Exit", __func__);
    return ret;
}

status_t AudioEndPointClient::GetBufferSize(uint32_t &buffer_size)
{
    QMMF_INFO("%s: Enter", __func__);
    Mutex::Autolock lock(lock_);

    if (audio_service_.get() == NULL) {
        QMMF_WARN("%s: not connected to audio service", __func__);
        return NO_ERROR;
    }

    auto ret = audio_service_->GetBufferSize(client_handle_, buffer_size);
    if(ret != NO_ERROR) {
        QMMF_ERROR("%s: service->GetBufferSize failed: %d", __func__, ret);
    }
    QMMF_INFO("%s: Exit", __func__);
    return ret;
}

status_t AudioEndPointClient::SetParam(const AudioParamType type,
                                       const AudioParamData &data)
{
    QMMF_INFO("%s: Enter", __func__);
    Mutex::Autolock lock(lock_);

    if (audio_service_.get() == NULL) {
        QMMF_WARN("%s: not connected to audio service", __func__);
        return NO_ERROR;
    }

    auto ret = audio_service_->SetParam(client_handle_, type, data);
    if(ret != NO_ERROR) {
        QMMF_ERROR("%s: service->SetParam failed: %d", __func__, ret);
    }

    QMMF_INFO("%s: Exit", __func__);
    return ret;
}

void AudioEndPointClient::notifyErrorEvent(int32_t error)
{
    QMMF_INFO("%s: Enter", __func__);
    EventData data;

    data.error = error;
    audio_cb_(kError, data);

    QMMF_INFO("%s: Exit", __func__);
}

void AudioEndPointClient::notifyStateChangedEvent(AudioState state)
{
    QMMF_INFO("%s: Enter", __func__);
    EventData data;

    data.state = state;
    audio_cb_(kStateChanged, data);

    QMMF_INFO("%s: Exit", __func__);
}

void AudioEndPointClient::notifyReadCompleteEvent(
                                        std::vector<AudioTrackBuffer> &buffers)
{
    QMMF_INFO("%s: Enter", __func__);
    EventData data;

    QMMF_INFO("%s: buf.size()=%u", __func__, buffers.size());
    for (uint32_t i = 0; i < buffers.size(); i++) {
        QMMF_INFO("%s: buf[%u].data=%p", __func__, i, buffers[i].data);
        QMMF_INFO("%s: buf[%u].capacity=%zu", __func__, i, buffers[i].capacity);
        QMMF_INFO("%s: buf[%u].size=%zu", __func__, i, buffers[i].size);
        QMMF_INFO("%s: buf[%u].timestamp=%lld", __func__, i, buffers[i].timestamp);
        QMMF_INFO("%s: buf[%u].flags=0x%08X", __func__, i, buffers[i].flags);
    }

    data.buffers = buffers;
    audio_cb_(kReadComplete, data);

    QMMF_INFO("%s: Exit", __func__);
}

void AudioEndPointClient::notifyWriteCompleteEvent(
                                        std::vector<AudioTrackBuffer> &buffers)
{
    QMMF_INFO("%s: Enter", __func__);
    EventData data;

    QMMF_INFO("%s: buf.size()=%u", __func__, buffers.size());
    for (uint32_t i = 0; i < buffers.size(); i++) {
        QMMF_INFO("%s: buf[%u].data=%p", __func__, i, buffers[i].data);
        QMMF_INFO("%s: buf[%u].capacity=%zu", __func__, i, buffers[i].capacity);
        QMMF_INFO("%s: buf[%u].size=%zu", __func__, i, buffers[i].size);
        QMMF_INFO("%s: buf[%u].timestamp=%lld", __func__, i, buffers[i].timestamp);
        QMMF_INFO("%s: buf[%u].flags=0x%08X", __func__, i, buffers[i].flags);
    }

    data.buffers = buffers;
    audio_cb_(kWriteComplete, data);

    QMMF_INFO("%s: Exit", __func__);
}

// Binder Proxy implementation of IAudioService
class BpAudioService: public BpInterface<IAudioService>
{
public:
    BpAudioService(const sp<IBinder>& impl)
        : BpInterface<IAudioService>(impl)
    {
    }

    status_t Connect(const sp<IAudioServiceCallback> &service_cb,
                     std::string &handle)
    {
        Parcel data, reply;

        // Register service callback to get callbacks from audio service
        data.writeInterfaceToken(IAudioService::getInterfaceDescriptor());
        data.writeStrongBinder(IInterface::asBinder(service_cb));

        remote()->transact(uint32_t(QMMF_AUDIO_SERVICE_CMDS::AUDIO_CONNECT),
                data, &reply);

        handle.assign(reply.readCString());
        return reply.readInt32();
    }

    status_t Disconnect(const std::string &handle)
    {
        Parcel data, reply;

        data.writeInterfaceToken(IAudioService::getInterfaceDescriptor());
        data.writeCString(handle.c_str());

        remote()->transact(uint32_t(QMMF_AUDIO_SERVICE_CMDS::AUDIO_DISCONNECT),
                data, &reply);

        return reply.readInt32();
    }

    status_t Configure(const std::string &handle,
                       const AudioType type,
                       const std::vector<DeviceID> &devices,
                       const AudioParams &params)
    {
        Parcel data, reply;
        Parcel::WritableBlob blob;
        uint32_t param_size = sizeof params;

        data.writeInterfaceToken(IAudioService::getInterfaceDescriptor());
        data.writeCString(handle.c_str());

        data.writeUint32(uint32_t(type));

        data.writeUint32(devices.size());
        for (uint32_t i = 0; i < devices.size(); ++i)
            data.writeCString(devices[i].c_str());

        data.writeUint32(param_size);
        data.writeBlob(param_size, false, &blob);
        memset(blob.data(), 0x0, param_size);
        memcpy(blob.data(), reinterpret_cast<void*>(&params), param_size);

        remote()->transact(uint32_t(QMMF_AUDIO_SERVICE_CMDS::AUDIO_CONFIGURE),
                data, &reply);
        blob.release();

        return reply.readInt32();
    }

    status_t Start(const std::string &handle)
    {
        Parcel data, reply;

        data.writeInterfaceToken(IAudioService::getInterfaceDescriptor());
        data.writeCString(handle.c_str());

        remote()->transact(uint32_t(QMMF_AUDIO_SERVICE_CMDS::AUDIO_START),
                data, &reply);

        return reply.readInt32();
    }

    status_t Stop(const std::string &handle, const bool do_flush)
    {
        Parcel data, reply;

        data.writeInterfaceToken(IAudioService::getInterfaceDescriptor());
        data.writeCString(handle.c_str());
        data.writeBool(do_flush);

        remote()->transact(uint32_t(QMMF_AUDIO_SERVICE_CMDS::AUDIO_STOP),
                data, &reply);

        return reply.readInt32();
    }

    status_t Pause(const std::string &handle)
    {
        Parcel data, reply;

        data.writeInterfaceToken(IAudioService::getInterfaceDescriptor());
        data.writeCString(handle.c_str());

        remote()->transact(uint32_t(QMMF_AUDIO_SERVICE_CMDS::AUDIO_PAUSE),
                data, &reply);

        return reply.readInt32();
    }

    status_t Resume(const std::string &handle)
    {
        Parcel data, reply;

        data.writeInterfaceToken(IAudioService::getInterfaceDescriptor());
        data.writeCString(handle.c_str());

        remote()->transact(uint32_t(QMMF_AUDIO_SERVICE_CMDS::AUDIO_RESUME),
                data, &reply);

        return reply.readInt32();
    }

    status_t Read(const std::string &handle,
                  const std::vector<AudioTrackBuffer> &buffers)
    {
        Parcel data, reply;
        Parcel::WritableBlob blob;
        uint32_t blob_size = sizeof AudioTrackBuffer * buffers.size();

        data.writeInterfaceToken(IRecorderService::getInterfaceDescriptor());
        data.writeCString(handle.c_str());

        data.writeUint32(buffers.size());
        data.writeBlob(blob_size, false, &blob);
        memset(blob.data(), 0x0, blob_size);
        memcpy(blob.data(), reinterpret_cast<void*>(buffers.data()), blob_size);

        remote()->transact(uint32_t(QMMF_AUDIO_SERVICE_CMDS::AUDIO_READ),
                data, &reply);
        blob.release();

        return reply.readInt32();
    }

    status_t Write(const std::string &handle,
                   const std::vector<AudioTrackBuffer> &buffers)
    {
        Parcel data, reply;
        Parcel::WritableBlob blob;
        uint32_t blob_size = sizeof AudioTrackBuffer * buffers.size();

        data.writeInterfaceToken(IRecorderService::getInterfaceDescriptor());
        data.writeCString(handle.c_str());

        data.writeUint32(buffers.size());
        data.writeBlob(blob_size, false, &blob);
        memset(blob.data(), 0x0, blob_size);
        memcpy(blob.data(), reinterpret_cast<void*>(buffers.data()), blob_size);

        remote()->transact(uint32_t(QMMF_AUDIO_SERVICE_CMDS::AUDIO_WRITE),
                data, &reply);
        blob.release();

        return reply.readInt32();
    }

    status_t GetLatency(const std::string &handle, uint32_t &latency)
    {
        Parcel data, reply;

        data.writeInterfaceToken(IAudioService::getInterfaceDescriptor());
        data.writeCString(handle.c_str());

        remote()->transact(uint32_t(QMMF_AUDIO_SERVICE_CMDS::AUDIO_GET_LATENCY),
                data, &reply);

        latency = reply.readUint32();
        return reply.readInt32();
    }

    status_t GetBufferSize(const std::string &handle, uint32_t &buffer_size)
    {
        Parcel data, reply;

        data.writeInterfaceToken(IAudioService::getInterfaceDescriptor());
        data.writeCString(handle.c_str());

        remote()->transact(uint32_t(QMMF_AUDIO_SERVICE_CMDS::AUDIO_GET_BUFFER_SIZE),
                data, &reply);

        buffer_size = reply.readUint32();
        return reply.readInt32();
    }

    status_t SetParam(const std::string &handle, const AudioParamType type,
                      const AudioParamData &audio_data)
    {
        Parcel data, reply;
        Parcel::WritableBlob blob;
        uint32_t blob_size = sizeof AudioParamData;

        data.writeInterfaceToken(IAudioService::getInterfaceDescriptor());
        data.writeCString(handle.c_str());

        data.writeUint32(uint32_t(type));
        data.writeUint32(blob_size);
        data.writeBlob(blob_size, false, &blob);
        memset(blob.data(), 0x0, blob_size);
        memcpy(blob.data(), reinterpret_cast<void*>(audio_data), blob_size);

        remote()->transact(uint32_t(QMMF_AUDIO_SERVICE_CMDS::AUDIO_SET_PARAM),
                data, &reply);
        blob.release();

        return reply.readInt32();
    }
};

IMPLEMENT_META_INTERFACE(AudioService, QMMF_AUDIO_SERVICE_NAME);

class BpAudioServiceCallback: public BpInterface<IAudioServiceCallback>
{
public:
    BpAudioServiceCallback(const sp<IBinder>& impl)
        : BpInterface<IAudioServiceCallback>(impl)
    {
    }

    void notifyErrorEvent(int32_t error)
    {
        QMMF_INFO("%s: Enter", __func__);
        Parcel data, reply;

        data.writeInterfaceToken(IAudioServiceCallback::getInterfaceDescriptor());
        data.writeInt32(error);

        remote()->transact(uint32_t(AUDIO_SERVICE_CB_CMDS::AUDIO_NOTIFY_ERROR),
                data, &reply);

        QMMF_INFO("%s: Exit - sent message one way", __func__);
    }

    void notifyStateChangedEvent(AudioState state)
    {
        QMMF_INFO("%s: Enter", __func__);
        Parcel data, reply;

        data.writeInterfaceToken(IAudioServiceCallback::getInterfaceDescriptor());
        data.writeUint32(uint32_t(state));

        remote()->transact(uint32_t(AUDIO_SERVICE_CB_CMDS::AUDIO_NOTIFY_STATE_CHANGED),
                data, &reply);

        QMMF_INFO("%s: Exit - sent message one way", __func__);
    }

    void notifyReadCompleteEvent(std::vector<AudioTrackBuffer> &buffers)
    {
        QMMF_INFO("%s: Enter", __func__);
        Parcel data, reply;
        Parcel::WritableBlob blob;
        uint32_t blob_size = sizeof AudioTrackBuffer * buffers.size();

        data.writeInterfaceToken(IRecorderService::getInterfaceDescriptor());
        data.writeUint32(buffers.size());
        data.writeBlob(blob_size, false, &blob);
        memset(blob.data(), 0x0, blob_size);
        memcpy(blob.data(), reinterpret_cast<void*>(buffers.data()), blob_size);

        remote()->transact(uint32_t(AUDIO_SERVICE_CB_CMDS::AUDIO_NOTIFY_READ_COMPLETE),
                data, &reply);
        blob.release();

        QMMF_INFO("%s: Exit - sent message one way", __func__);
    }

    void notifyWriteCompleteEvent(std::vector<AudioTrackBuffer> &buffers)
    {
        QMMF_INFO("%s: Enter", __func__);
        Parcel data, reply;
        Parcel::WritableBlob blob;
        uint32_t blob_size = sizeof AudioTrackBuffer * buffers.size();

        data.writeInterfaceToken(IRecorderService::getInterfaceDescriptor());
        data.writeUint32(buffers.size());
        data.writeBlob(blob_size, false, &blob);
        memset(blob.data(), 0x0, blob_size);
        memcpy(blob.data(), reinterpret_cast<void*>(buffers.data()), blob_size);

        remote()->transact(uint32_t(AUDIO_SERVICE_CB_CMDS::AUDIO_NOTIFY_WRITE_COMPLETE),
                data, &reply);
        blob.release();

        QMMF_INFO("%s: Exit - sent message one way", __func__);
    }
};

IMPLEMENT_META_INTERFACE(AudioServiceCallback,
                            "audio.service.IAudioServiceCallback");

status_t BnAudioServiceCallback::onTransact(uint32_t code, const Parcel& data,
                                            Parcel* reply, uint32_t flags)
{
    QMMF_INFO("%s: Enter", __func__);

    CHECK_INTERFACE(IAudioServiceCallback, data, reply);

    switch(code) {
        case AUDIO_SERVICE_CB_CMDS::AUDIO_NOTIFY_ERROR:
            {
                QMMF_INFO("%s: AUDIO_NOTIFY_ERROR", __func__);
                int32_t error;

                error = data.readInt32();
                notifyErrorEvent(error);

                return NO_ERROR;
            }
            break;

        case AUDIO_SERVICE_CB_CMDS::AUDIO_NOTIFY_STATE_CHANGED:
            {
                QMMF_INFO("%s: AUDIO_NOTIFY_STATE_CHANGED", __func__);
                AudioState state;

                state = data.readUint32();
                notifyStateChangedEvent(state);

                return NO_ERROR;
            }
            break;

        case AUDIO_SERVICE_CB_CMDS::AUDIO_NOTIFY_READ_COMPLETE:
            {
                QMMF_INFO("%s: AUDIO_NOTIFY_READ_COMPLETE", __func__);

                uint32_t vector_size = data.readUint32();
                uint32_t blob_size = vector_size * sizeof AudioTrackBuffer;
                AudioTrackBuffer buffer_array[vector_size];
                memset(&buffer_array, 0x0, blob_size);
                Parcel::ReadableBlob blob;
                readBlob(blob_size, &blob);
                memcpy(&buffer_array, blob.data(), blob_size);
                blob.release();

                std::vector<AudioTrackBuffer> buffers;
                for (uint32_t i = 0; i < vector_size; ++i) {
                    buffers.push_back(buffer_array[i]);
                }
                notifyReadCompleteEvent(buffers);

                return NO_ERROR;
            }
            break;

        case AUDIO_SERVICE_CB_CMDS::AUDIO_NOTIFY_WRITE_COMPLETE:
            {
                QMMF_INFO("%s: AUDIO_NOTIFY_WRITE_COMPLETE", __func__);

                uint32_t vector_size = data.readUint32();
                uint32_t blob_size = vector_size * sizeof AudioTrackBuffer;
                AudioTrackBuffer buffer_array[vector_size];
                memset(&buffer_array, 0x0, blob_size);
                Parcel::ReadableBlob blob;
                readBlob(blob_size, &blob);
                memcpy(&buffer_array, blob.data(), blob_size);
                blob.release();

                std::vector<AudioTrackBuffer> buffers;
                for (uint32_t i = 0; i < vector_size; ++i) {
                    buffers.push_back(buffer_array[i]);
                }
                notifyWriteCompleteEvent(buffers);

                return NO_ERROR;
            }
            break;

        default:
            QMMF_ERROR("%s: code %u not supported ", __func__, code);
            break;
    }
    return NO_ERROR;
}

}; // namespace audio
}; // namespace qmmf
