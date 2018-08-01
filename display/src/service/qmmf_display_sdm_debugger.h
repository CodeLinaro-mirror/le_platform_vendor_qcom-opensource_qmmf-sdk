/*
* Copyright (c) 2016, 2018, The Linux Foundation. All rights reserved.
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

#include <bitset>

#include <cutils/log.h>
#include <utils/Trace.h>
#include <sdm/core/sdm_types.h>
#ifdef QMMF_DISPLAY_INTF_v1
#include <debug_handler.h>
#else
#include <sdm/core/debug_interface.h>
#endif

#include "qmmf-sdk/qmmf_display_params.h"

using namespace sdm;
#ifdef QMMF_DISPLAY_INTF_v1
using namespace display;
#endif
//Prop to enable Display logging
#define DISPLAY_LOG_LEVEL  "persist.qmmf.display.log"

namespace qmmf {

namespace display {
#ifndef QMMF_DISPLAY_INTF_v1
class DisplayDebugHandlerV1 : public DebugHandler {
 public:
  DisplayDebugHandlerV1();
  static inline DebugHandler* Get() { return &debug_handler_; }

  static void DebugAll(bool enable, int verbose_level);
  static void DebugResources(bool enable, int verbose_level);
  static void DebugStrategy(bool enable, int verbose_level);
  static void DebugCompManager(bool enable, int verbose_level);
  static void DebugDriverConfig(bool enable, int verbose_level);
  static void DebugRotator(bool enable, int verbose_level);
  static void DebugQdcm(bool enable, int verbose_level);
  static int  GetIdleTimeoutMs();

  virtual void Error(::DebugTag tag, const char *format, ...) override;
  virtual void Warning(::DebugTag tag, const char *format, ...) override;
  virtual void Info(::DebugTag tag, const char *format, ...) override;
  virtual void Debug(::DebugTag tag, const char *format, ...) override;
  virtual void Verbose(::DebugTag tag, const char *format, ...) override ;
  virtual void BeginTrace(const char *class_name, const char *function_name,
                          const char *custom_string) override;
  void EndTrace() override;
  DisplayError GetProperty(const char *property_name, int *value) override;
  DisplayError GetProperty(const char *property_name, char *value) override;
  DisplayError SetProperty(const char *property_name,
      const char *value) override;

 private:
  static DisplayDebugHandlerV1 debug_handler_;
  static std::bitset<32> debug_flags_;
  static int32_t verbose_level_;
};
#else
class DisplayDebugHandlerV2 : public DebugHandler {
 public:
  DisplayDebugHandlerV2();
  static inline DebugHandler* Get() { return &debug_handler_; }

  static void DebugAll(bool enable, int verbose_level);
  static void DebugResources(bool enable, int verbose_level);
  static void DebugStrategy(bool enable, int verbose_level);
  static void DebugCompManager(bool enable, int verbose_level);
  static void DebugDriverConfig(bool enable, int verbose_level);
  static void DebugRotator(bool enable, int verbose_level);
  static void DebugQdcm(bool enable, int verbose_level);
  static void DebugScalar(bool enable, int verbose_level);
  static void DebugClient(bool enable, int verbose_level);
  static void DebugDisplay(bool enable, int verbose_level);
  static int  GetIdleTimeoutMs();

  void Error(const char *format, ...) override;
  void Warning(const char *format, ...) override;
  void Info(const char *format, ...) override;
  void Debug(const char *format, ...) override;
  void Verbose(const char *format, ...) override;
  void BeginTrace(const char *class_name, const char *function_name,
                          const char *custom_string) override;
  void EndTrace() override;
  int GetProperty(const char *property_name, int *value) override;
  int GetProperty(const char *property_name, char *value) override;

 private:
  static DisplayDebugHandlerV2 debug_handler_;
  static std::bitset<32> debug_flags_;
  static int32_t verbose_level_;
};
#endif
}; // namespace display

}; //namespace qmmf
