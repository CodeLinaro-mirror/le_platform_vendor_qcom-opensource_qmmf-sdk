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

#include "qmmf-sdk/qmmf_recorder_interface.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <gbm_priv.h>
#include <sys/mman.h>
#include <cutils/native_handle.h>
#include <sys/stat.h>
#include <time.h>



#define DRM_DEVICE_NAME "/dev/dri/card0"
struct gbm_bo* pGbmBuffObject   = NULL;
int32_t format_yuv422 = GBM_FORMAT_UYVY;
int fd;
int bufferNum = 4;
int dump = 0;
int runing_time = 100;

typedef struct cameraBuffer{
    char*      camBuffer[4];             //at most 4 GMSL camera <--> buffer, if less than 4 camera, based on Camera_ID to alloc buffer.
    uint32_t   frameNumber;              //every frame have a frame number for count.
    uint32_t   reference;                //Read in is 2, Read out is 0;  buffer refersh is 1;
}cameraBuffer_t;

typedef struct inputInfo{
    cameraBuffer_t   recorderBuffers[8];    //Multiple cameraBuffer_t are used for refersh buffer.
    cameraBuffer_t   reserved1[8];
    cameraBuffer_t   reserved2[8];
    uint32_t         bufMemNum;            //The actual number of cameraBuffer_t in every stream, need <= 8
    uint32_t         Camera_ID = 0x0;      //Camera ID need based on camera link number, if number==3, at most Camera_ID=0x7;
                                           //But if you only want using 2 camera, you can set Camera_ID = 0x3/0x6/0x5
}inputInfo_t;


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// GBMAllocs
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int GBMAlloc(
    int32_t gbm_format, uint32_t w, uint32_t h)
{
    struct gbm_device * pGbmDevice;                ///< GBM device object
    native_handle_t* p_handle = NULL;
    uint64_t bo_handle          = 0;
    int  deviceFD;
    deviceFD = open(DRM_DEVICE_NAME, O_RDWR);
    if (0 > deviceFD)
    {
        printf("unsupported device!!");
    }
    else
    {
        pGbmDevice = gbm_create_device(deviceFD);
        if (pGbmDevice != NULL &&
            gbm_device_get_fd(pGbmDevice) != deviceFD)
        {
            printf("unable to create GBM device!! \n");
            gbm_device_destroy(pGbmDevice);
            pGbmDevice = NULL;
        }
    }
    if (-1 == gbm_format)
        {
            printf("Invalid Argument");
            return NULL;
        }

    pGbmBuffObject = gbm_bo_create(pGbmDevice, w, h, gbm_format, 0);

    if(pGbmBuffObject != NULL)
    {
        bo_handle = reinterpret_cast<uint64_t>(pGbmBuffObject);
        p_handle = native_handle_create(1, 2);
        p_handle->data[0] = gbm_bo_get_fd(pGbmBuffObject);
        p_handle->data[1] = (int)((bo_handle >> 32) & 0xFFFFFFFF);
        p_handle->data[2] = (int)(bo_handle & 0xFFFFFFFF);
        if(NULL == p_handle)
        {
            printf("Invalid native handle");
        }
    }

    return p_handle->data[0];
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// FreeGbmBufferObjs
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void FreeGbmBufferObj()
{
    if(pGbmBuffObject != NULL)
    {
        gbm_bo_destroy(pGbmBuffObject);
    }
}

void buffer_CallBack(void* input_buffer, int memCount){
    uint32_t frameNum;
    uint32_t buffer_size = 1920*1020*2;
    unsigned long long int time_temp;
    struct timeval time_t;
    inputInfo_t* input_buffers = (inputInfo_t*)input_buffer;


    frameNum = input_buffers->recorderBuffers[memCount].frameNumber;
    input_buffers->recorderBuffers[memCount].reference = 2;
    gettimeofday(&time_t, NULL);
    time_temp = 1000000 * time_t.tv_sec + time_t.tv_usec;
    printf("Camera_ID:0x%x,frame NUmber:%d, time:%lld\n",input_buffers->Camera_ID, frameNum, time_temp);
////
//To-do


////
    if(dump){
        if(frameNum % 100 == 0 && input_buffers->Camera_ID & 0x1){
            std::string file_path("/data/misc/qmmf/qmmf_test_0_");
            file_path += std::to_string(frameNum);
            file_path += ".yuv";
            FILE *file = fopen(file_path.c_str(), "w+");
            fwrite(input_buffers->recorderBuffers[memCount].camBuffer[0] , sizeof(uint8_t),
                       buffer_size, file);
            fclose(file);
        }
        if(frameNum % 101 == 0 && input_buffers->Camera_ID & 0x2){
            std::string file_path("/data/misc/qmmf/qmmf_test_1_");
            file_path += std::to_string(frameNum);
            file_path += ".yuv";
            FILE *file = fopen(file_path.c_str(), "w+");
            fwrite(input_buffers->recorderBuffers[memCount].camBuffer[1] , sizeof(uint8_t),
                       buffer_size, file);
            fclose(file);
        }
        if(frameNum % 102 == 0 && input_buffers->Camera_ID & 0x4){
            std::string file_path("/data/misc/qmmf/qmmf_test_2_");
            file_path += std::to_string(frameNum);
            file_path += ".yuv";
            FILE *file = fopen(file_path.c_str(), "w+");
            fwrite(input_buffers->recorderBuffers[memCount].camBuffer[2] , sizeof(uint8_t),
                       buffer_size, file);
            fclose(file);
        }
        if(frameNum % 103 == 0 && input_buffers->Camera_ID & 0x8){
            std::string file_path("/data/misc/qmmf/qmmf_test_3_");
            file_path += std::to_string(frameNum);
            file_path += ".yuv";
            FILE *file = fopen(file_path.c_str(), "w+");
            fwrite(input_buffers->recorderBuffers[memCount].camBuffer[3] , sizeof(uint8_t),
                       buffer_size, file);
            fclose(file);
        }
    }
    input_buffers->recorderBuffers[memCount].reference = 0;



}



int main(){

inputInfo_t* input_buffers = (inputInfo_t*)malloc(sizeof(input_buffers));

CameraRecorder* CamObj = new CameraRecorder;

uint32_t buffer_size = 1920*1020*2;


input_buffers->bufMemNum = bufferNum;
input_buffers->Camera_ID = 0xD;  //if link 4 GMSL camera, Camera_ID max is 0xF
for(int i = 0; i < bufferNum; i++ ){
    if(input_buffers->Camera_ID & 0x1){
    fd = GBMAlloc(format_yuv422, 1920, 1020);
    input_buffers->recorderBuffers[i].camBuffer[0] = (char*)mmap(NULL, buffer_size, (PROT_READ | PROT_WRITE), MAP_SHARED, fd, 0);
    }
    if(input_buffers->Camera_ID & 0x2){
    fd = GBMAlloc(format_yuv422, 1920, 1020);
    input_buffers->recorderBuffers[i].camBuffer[1] = (char*)mmap(NULL, buffer_size, (PROT_READ | PROT_WRITE), MAP_SHARED, fd, 0);
    }
    if(input_buffers->Camera_ID & 0x4){
    fd = GBMAlloc(format_yuv422, 1920, 1020);
    input_buffers->recorderBuffers[i].camBuffer[2] = (char*)mmap(NULL, buffer_size, (PROT_READ | PROT_WRITE), MAP_SHARED, fd, 0);
    }
    if(input_buffers->Camera_ID & 0x8){
    fd = GBMAlloc(format_yuv422, 1920, 1020);
    input_buffers->recorderBuffers[i].camBuffer[3] = (char*)mmap(NULL, buffer_size, (PROT_READ | PROT_WRITE), MAP_SHARED, fd, 0);
    }
    input_buffers->recorderBuffers[i].reference = 0;
    input_buffers->recorderBuffers[i].frameNumber = 0;
}

    sleep(5);     //need some delay after create obj
    int camera_number =  CamObj->GetCameraNum();
    printf("camera NUmber:%d, %p\n",camera_number,CamObj);
    void* inbuf = (void*)input_buffers;
    CamObj->StartCamera(buffer_CallBack, inbuf);

    sleep(runing_time);     //camera runing time

    CamObj->StopCamera();
    for(int i = 0; i < bufferNum; i++ ){
        if(input_buffers->Camera_ID & 0x1)
            munmap(input_buffers->recorderBuffers[i].camBuffer[0], buffer_size);
        if(input_buffers->Camera_ID & 0x2)
            munmap(input_buffers->recorderBuffers[i].camBuffer[1], buffer_size);
        if(input_buffers->Camera_ID & 0x4)
            munmap(input_buffers->recorderBuffers[i].camBuffer[2], buffer_size);
        if(input_buffers->Camera_ID & 0x8)
            munmap(input_buffers->recorderBuffers[i].camBuffer[3], buffer_size);
        input_buffers->Camera_ID = 0;
        input_buffers->recorderBuffers[i].reference = 0;
        input_buffers->recorderBuffers[i].frameNumber = 0;
    }

    FreeGbmBufferObj();
    sleep(5);     //need some delay before delete obj
    delete CamObj;
    return 0;
}



