/*
* Copyright (c) 2018, The Linux Foundation. All rights reserved.
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

#include <cstdint>

/** heap_tracker_get_total_allocations
*
* Returns number of total allocations
*
* return: number of total allocations
**/
extern "C" uint32_t heap_tracker_get_total_allocations();

/** heap_tracker_init
*
* Initialize heap tracker. Do NOT invoke it directly. Let init hook to invoke it
*
* return: void
**/
extern "C" void heap_tracker_init();

/** heap_tracker_deinit
*
* Deinitialize heap tracker.  Do NOT invoke it directly. Let deinit hook to
*   invoke it
*
* return: void
**/
extern "C" void heap_tracker_deinit();

/** init
*
* Deinitialize heap tracker
*
* return: void
**/
static __attribute__((constructor)) void init(void) { heap_tracker_init(); }

/** deinit
*
* Deinitialize heap tracker
*
* return: void
**/
static __attribute__((destructor)) void deinit(void) { heap_tracker_deinit(); }