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

#define TAG "DisplayDebugHandlerV"

#include <cutils/properties.h>
#include <sdm/utils/constants.h>

#include "display/src/service/qmmf_display_sdm_debugger.h"

namespace qmmf {

namespace display {

#define MAX_NAME_SIZE 256

#ifndef QMMF_DISPLAY_INTF_v1
DisplayDebugHandlerV1 DisplayDebugHandlerV1::debug_handler_;

std::bitset<32> DisplayDebugHandlerV1::debug_flags_;

int32_t DisplayDebugHandlerV1::verbose_level_ = 0x0;

DisplayDebugHandlerV1::DisplayDebugHandlerV1() {
  char prop_val[PROPERTY_VALUE_MAX];
  property_get(DISPLAY_LOG_LEVEL, prop_val, "0");
  if (atoi(prop_val) == 0) {
    DisplayDebugHandlerV1::debug_flags_ = 0x1; // kTagNone should always be printed.
  } else {
    DisplayDebugHandlerV1::debug_flags_ = 0x7FFFFFFF;
  }
}

void DisplayDebugHandlerV1::DebugAll(bool enable, int verbose_level) {
  if (enable) {
    debug_flags_ = 0x7FFFFFFF;
    verbose_level_ = verbose_level;
  } else {
    debug_flags_ = 0x1;   // kTagNone should always be printed.
    verbose_level_ = 0;
  }
}

void DisplayDebugHandlerV1::DebugResources(bool enable, int verbose_level) {
  if (enable) {
    debug_flags_[kTagResources] = 1;
    verbose_level_ = verbose_level;
  } else {
    debug_flags_[kTagResources] = 0;
    verbose_level_ = 0;
  }
}

void DisplayDebugHandlerV1::DebugStrategy(bool enable, int verbose_level) {
  if (enable) {
    debug_flags_[kTagStrategy] = 1;
    verbose_level_ = verbose_level;
  } else {
    debug_flags_[kTagStrategy] = 0;
    verbose_level_ = 0;
  }
}

void DisplayDebugHandlerV1::DebugCompManager(bool enable, int verbose_level) {
  if (enable) {
    debug_flags_[kTagCompManager] = 1;
    verbose_level_ = verbose_level;
  } else {
    debug_flags_[kTagCompManager] = 0;
    verbose_level_ = 0;
  }
}

void DisplayDebugHandlerV1::DebugDriverConfig(bool enable, int verbose_level) {
  if (enable) {
    debug_flags_[kTagDriverConfig] = 1;
    verbose_level_ = verbose_level;
  } else {
    debug_flags_[kTagDriverConfig] = 0;
    verbose_level_ = 0;
  }
}

void DisplayDebugHandlerV1::DebugRotator(bool enable, int verbose_level) {
  if (enable) {
    debug_flags_[kTagRotator] = 1;
    verbose_level_ = verbose_level;
  } else {
    debug_flags_[kTagRotator] = 0;
    verbose_level_ = 0;
  }
}

void DisplayDebugHandlerV1::DebugQdcm(bool enable, int verbose_level) {
  if (enable) {
    debug_flags_[kTagQDCM] = 1;
    verbose_level_ = verbose_level;
  } else {
    debug_flags_[kTagQDCM] = 0;
    verbose_level_ = 0;
  }
}

int DisplayDebugHandlerV1::GetIdleTimeoutMs() {
  int value = IDLE_TIMEOUT_DEFAULT_MS;
  debug_handler_.GetProperty("sdm.idle_time", &value);

  return value;
}

void DisplayDebugHandlerV1::Error(::DebugTag tag, const char *format, ...) {
  va_list list;
  va_start(list, format);
  __android_log_vprint(ANDROID_LOG_ERROR, LOG_TAG, format, list);
}

void DisplayDebugHandlerV1::Warning(::DebugTag tag, const char *format, ...) {
  va_list list;
  va_start(list, format);
  __android_log_vprint(ANDROID_LOG_WARN, LOG_TAG, format, list);
}

void DisplayDebugHandlerV1::Info(::DebugTag tag, const char *format, ...) {
  if (debug_flags_[tag]) {
    va_list list;
    va_start(list, format);
    __android_log_vprint(ANDROID_LOG_INFO, LOG_TAG, format, list);
  }
}

void DisplayDebugHandlerV1::Debug(::DebugTag tag, const char *format, ...) {
  if (debug_flags_[tag]) {
    va_list list;
    va_start(list, format);
    __android_log_vprint(ANDROID_LOG_DEBUG, LOG_TAG, format, list);
  }
}

void DisplayDebugHandlerV1::Verbose(::DebugTag tag, const char *format, ...) {
  if (debug_flags_[tag] && verbose_level_) {
    va_list list;
    va_start(list, format);
    __android_log_vprint(ANDROID_LOG_VERBOSE, LOG_TAG, format, list);
  }
}

void DisplayDebugHandlerV1::BeginTrace(const char *class_name,
  const char *function_name, const char *custom_string) {
  char name[MAX_NAME_SIZE] = {0};
  snprintf(name, sizeof(name), "%s::%s::%s", class_name, function_name,
      custom_string);
  //TBD
}

void DisplayDebugHandlerV1::EndTrace() {
  //TBD
}

DisplayError DisplayDebugHandlerV1::GetProperty(const char *property_name,
    int *value) {
  char property[PROPERTY_VALUE_MAX];

  if (property_get(property_name, property, NULL) > 0) {
    *value = atoi(property);
    return kErrorNone;
  }

  return kErrorNotSupported;
}

DisplayError DisplayDebugHandlerV1::GetProperty(const char *property_name,
    char *value) {
  if (property_get(property_name, value, NULL) > 0) {
    return kErrorNone;
  }

  return kErrorNotSupported;
}

DisplayError DisplayDebugHandlerV1::SetProperty(const char *property_name,
    const char *value) {
  if (property_set(property_name, value) == 0) {
    return kErrorNone;
  }

  return kErrorNotSupported;
}
#else
DisplayDebugHandlerV2 DisplayDebugHandlerV2::debug_handler_;

std::bitset<32> DisplayDebugHandlerV2::debug_flags_;

int32_t DisplayDebugHandlerV2::verbose_level_ = 0x0;

DisplayDebugHandlerV2::DisplayDebugHandlerV2() {
  char prop_val[PROPERTY_VALUE_MAX];
  property_get(DISPLAY_LOG_LEVEL, prop_val, "0");
  if (atoi(prop_val) == 0) {
    DisplayDebugHandlerV2::debug_flags_ = 0x1; // kTagNone should always be printed.
  } else {
    DisplayDebugHandlerV2::debug_flags_ = 0x7FFFFFFF;
  }

  DebugHandler::Set(DisplayDebugHandlerV2::Get());
}

void DisplayDebugHandlerV2::DebugAll(bool enable, int verbose_level) {
  if (enable) {
    debug_flags_ = 0x7FFFFFFF;
    verbose_level_ = verbose_level;
  } else {
    debug_flags_ = 0x1;   // kTagNone should always be printed.
    verbose_level_ = 0;
  }

  DebugHandler::SetLogMask(debug_handler_.debug_flags_);
}

void DisplayDebugHandlerV2::DebugResources(bool enable, int verbose_level) {
  if (enable) {
    debug_flags_[kTagResources] = 1;
    verbose_level_ = verbose_level;
  } else {
    debug_flags_[kTagResources] = 0;
    verbose_level_ = 0;
  }

  DebugHandler::SetLogMask(debug_handler_.debug_flags_);
}

void DisplayDebugHandlerV2::DebugStrategy(bool enable, int verbose_level) {
  if (enable) {
    debug_flags_[kTagStrategy] = 1;
    verbose_level_ = verbose_level;
  } else {
    debug_flags_[kTagStrategy] = 0;
    verbose_level_ = 0;
  }

  DebugHandler::SetLogMask(debug_handler_.debug_flags_);
}

void DisplayDebugHandlerV2::DebugCompManager(bool enable, int verbose_level) {
  if (enable) {
    debug_flags_[kTagCompManager] = 1;
    verbose_level_ = verbose_level;
  } else {
    debug_flags_[kTagCompManager] = 0;
    verbose_level_ = 0;
  }

  DebugHandler::SetLogMask(debug_handler_.debug_flags_);
}

void DisplayDebugHandlerV2::DebugDriverConfig(bool enable, int verbose_level) {
  if (enable) {
    debug_flags_[kTagDriverConfig] = 1;
    verbose_level_ = verbose_level;
  } else {
    debug_flags_[kTagDriverConfig] = 0;
    verbose_level_ = 0;
  }

  DebugHandler::SetLogMask(debug_handler_.debug_flags_);
}

void DisplayDebugHandlerV2::DebugRotator(bool enable, int verbose_level) {
  if (enable) {
    debug_flags_[kTagRotator] = 1;
    verbose_level_ = verbose_level;
  } else {
    debug_flags_[kTagRotator] = 0;
    verbose_level_ = 0;
  }

  DebugHandler::SetLogMask(debug_handler_.debug_flags_);
}

void DisplayDebugHandlerV2::DebugQdcm(bool enable, int verbose_level) {
  if (enable) {
    debug_flags_[kTagQDCM] = 1;
    verbose_level_ = verbose_level;
  } else {
    debug_flags_[kTagQDCM] = 0;
    verbose_level_ = 0;
  }

  DebugHandler::SetLogMask(debug_handler_.debug_flags_);
}

void DisplayDebugHandlerV2::DebugScalar(bool enable, int verbose_level) {
  if (enable) {
    debug_handler_.debug_flags_[kTagScalar] = 1;
    debug_handler_.verbose_level_ = verbose_level;
  } else {
    debug_handler_.debug_flags_[kTagScalar] = 0;
    debug_handler_.verbose_level_ = 0;
  }

  DebugHandler::SetLogMask(debug_handler_.debug_flags_);
}

void DisplayDebugHandlerV2::DebugClient(bool enable, int verbose_level) {
  if (enable) {
    debug_handler_.debug_flags_[kTagClient] = 1;
    debug_handler_.verbose_level_ = verbose_level;
  } else {
    debug_handler_.debug_flags_[kTagClient] = 0;
    debug_handler_.verbose_level_ = 0;
  }

  DebugHandler::SetLogMask(debug_handler_.debug_flags_);
}

void DisplayDebugHandlerV2::DebugDisplay(bool enable, int verbose_level) {
  if (enable) {
    debug_handler_.debug_flags_[kTagDisplay] = 1;
    debug_handler_.verbose_level_ = verbose_level;
  } else {
    debug_handler_.debug_flags_[kTagDisplay] = 0;
    debug_handler_.verbose_level_ = 0;
  }

  DebugHandler::SetLogMask(debug_handler_.debug_flags_);
}

int DisplayDebugHandlerV2::GetIdleTimeoutMs() {
  int value = IDLE_TIMEOUT_DEFAULT_MS;
  debug_handler_.GetProperty("sdm.idle_time", &value);

  return value;
}

void DisplayDebugHandlerV2::Error(const char *format, ...) {
  va_list list;
  va_start(list, format);
  __android_log_vprint(ANDROID_LOG_ERROR, LOG_TAG, format, list);
}

void DisplayDebugHandlerV2::Warning(const char *format, ...) {
  va_list list;
  va_start(list, format);
  __android_log_vprint(ANDROID_LOG_WARN, LOG_TAG, format, list);
}

void DisplayDebugHandlerV2::Info(const char *format, ...) {
  va_list list;
  va_start(list, format);
  __android_log_vprint(ANDROID_LOG_INFO, LOG_TAG, format, list);
}

void DisplayDebugHandlerV2::Debug(const char *format, ...) {
  va_list list;
  va_start(list, format);
  __android_log_vprint(ANDROID_LOG_DEBUG, LOG_TAG, format, list);
}

void DisplayDebugHandlerV2::Verbose(const char *format, ...) {
  if (debug_handler_.verbose_level_) {
    va_list list;
    va_start(list, format);
    __android_log_vprint(ANDROID_LOG_VERBOSE, LOG_TAG, format, list);
  }
}

void DisplayDebugHandlerV2::BeginTrace(const char *class_name,
  const char *function_name, const char *custom_string) {
  char name[MAX_NAME_SIZE] = {0};
  snprintf(name, sizeof(name), "%s::%s::%s", class_name, function_name,
      custom_string);
  //TBD
}

void DisplayDebugHandlerV2::EndTrace() {
  //TBD
}

int DisplayDebugHandlerV2::GetProperty(const char *property_name,
    int *value) {
  char property[PROPERTY_VALUE_MAX];

  if (property_get(property_name, property, NULL) > 0) {
    *value = atoi(property);
    return kErrorNone;
  }

  return kErrorNotSupported;
}

int DisplayDebugHandlerV2::GetProperty(const char *property_name,
    char *value) {
  if (property_get(property_name, value, NULL) > 0) {
    return kErrorNone;
  }

  return kErrorNotSupported;
}
#endif

}; // namespace display

}; //namespace qmmf

