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

#include <sstream>
#include <string>

#include <utils/Log.h>
#include <cutils/properties.h>

#include <qmmf-alg/qmmf_alg_plugin.h>

namespace qmmf {

/** Property:
 *
 *  This class defines property operations
 **/
class Property {
 public:
  /** Get
   *    @property: property
   *    @default_value: default value
   *
   * Gets requested property value
   *
   * return: property value
   **/
  template <typename TProperty>
    static TProperty Get(std::string property, TProperty default_value) {
      TProperty value = default_value;
      char prop_val[PROPERTY_VALUE_MAX];
      std::stringstream s;
      s << default_value;
      property_get(property.c_str(), prop_val, s.str().c_str());

      std::stringstream output(prop_val);
      output >> value;
      return value;
    }

    /** Set
     *    @property: property
     *    @value: value
     *
     * Sets requested property value
     *
     * return: nothing
     **/
    template <typename TProperty>
    static void Set(std::string property, TProperty value) {
      std::stringstream s;
      s << value;
      std::string value_string = s.str();
      value_string.resize(PROPERTY_VALUE_MAX);
      property_set(property.c_str(), value_string.c_str());
    }
};

/** QmmfAlgoTools
 *
 * Qmmf Alog tools implementation
 *
 **/
class QmmfAlgoTools : public qmmf_alg_plugin::ITools {
public:
  ~QmmfAlgoTools() {};

  void SetProperty(std::string property,
                   std::string value) {
    Property::Set(property, value);
  }

  void SetProperty(std::string property,
                   int32_t value) {
    Property::Set(property, value);
  }

  const std::string GetProperty(std::string property,
                                std::string default_value) {
    return Property::Get(property, default_value);
  }

  uint32_t GetProperty(std::string property,
                       int32_t default_value) {
    return Property::Get(property, default_value);
  }

  void LogError(const std::string &s) {
    ALOGE("%s", s.c_str());
  }

  void LogWarning(const std::string &s) {
    ALOGW("%s", s.c_str());
  }

  void LogInfo(const std::string &s) {
    ALOGI("%s", s.c_str());
  }

  void LogDebug(const std::string &s) {
    ALOGD("%s", s.c_str());
  }

  void LogVerbose(const std::string &s) {
    ALOGV("%s", s.c_str());
  }
};

} // qmmf namespace
