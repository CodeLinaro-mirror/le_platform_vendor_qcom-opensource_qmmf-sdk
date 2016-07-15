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

#pragma once

#include "qmmf_display_params.h"

namespace qmmf {
namespace display {

class DisplayClient;
class Display
{
public:
    Display(DisplayType type, DisplayEventHandler &event_handler);

    Display() = delete;

    ~Display();

    // This API internally calls validate() which checks surface properties and check whether
    // one of the available pipe's can be assigned to this surface. If surface properties meet
    // the requirements of available pipe capabilities, one of the pipe available pipe is assigned
    // to this layer
    // Surface represents producer side of the buffer queue,
    status_t CreateSurface(SurfaceConfig &surface_config, std:string &surface_uuid);

    status_t DestroySurface(const std:string &surface_uuid);

    status_t DequeueSurfaceBuffer(std:string &surface_uuid, SurfaceBuffer &surface_buffer);

    status_t QueueSurfaceBuffer(const std:string &surface_uuid, SurfaceBuffer &surface_buffer, SurfaceParam &surface_param);

    status_t GetDisplayParam(void *param,
                            size_t param_size);

    // Sets Dynamic display params
    status_t SetDisplayParam(const void *param,
                            size_t param_size);

private:
    sp<DisplayClient> mDisplayClient;
};

}
} // namespace qmmf::display

