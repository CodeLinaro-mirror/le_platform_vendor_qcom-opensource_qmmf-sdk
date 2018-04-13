/*
* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
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

#include <json/json.h>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <string>

#include "qmmf-plugin/qmmf_alg_utils.h"

namespace qmmf {
namespace qmmf_alg_plugin {

class QmmfJsonHelper {
 public:
  QmmfJsonHelper(Json::Value &v) : v_(v){};

  template <typename TItem>
  void Get(const std::string &field_name, TItem &item, bool validate = true) {
    if (validate) {
      Validate(field_name, 1);
    }
    if (Exists(field_name)) {
      Fill(field_name, v_[field_name], item);
    }
  }

  void Get(const std::string &field_name, std::string &item,
           bool validate = true) {
    if (validate) {
      Validate(field_name, 1);
    }
    if (Exists(field_name)) {
      Fill(field_name, v_[field_name], item);
    }
  }

  template <typename TItem, typename TMap>
  void Get(const std::string &field_name, TItem &item, TMap map,
           bool validate = true) {
    if (validate) {
      Validate(field_name, 1);
    }
    if (Exists(field_name)) {
      if (map.find(v_[field_name].asString()) == map.end()) {
        std::stringstream s;
        s << " is enum but value " << v_[field_name].asString()
          << " is not supported in the enum from string method";
        Utils::ThrowException(field_name, s.str());
      }
      item = map[v_[field_name].asString()];
    }
  }

  template <typename TItem>
  void Get(const std::string &field_name, std::list<TItem> &list,
           bool validate = true) {
    if (validate) {
      Validate(field_name, -1);
    }
    if (Exists(field_name)) {
      for (uint32_t i = 0; i < v_[field_name].size(); i++) {
        TItem item;
        Fill(field_name, v_[field_name][i], item);
        list.push_back(item);
      }
    }
  }

  template <typename TItem>
  void Get(const std::string &field_name,
           std::list<std::shared_ptr<TItem>> &list, bool validate = true) {
    if (validate) {
      Validate(field_name, -1);
    }
    if (Exists(field_name)) {
      for (uint32_t i = 0; i < v_[field_name].size(); i++) {
        list.push_back(TItem::New(v_[field_name][i]));
      }
    }
  }

  template <typename TItem>
  void Get(const std::string &field_name,
           std::map<const std::string, std::shared_ptr<TItem>> &map,
           bool validate = true) {
    if (validate) {
      Validate(field_name, -1);
    }
    if (Exists(field_name)) {
      auto new_item = TItem::New(v_[field_name]);
      map.insert(std::pair<const std::string, std::shared_ptr<TItem>>(
          field_name, new_item));
    }
  }

 private:
  template <typename TItem>
  void Fill(const std::string &field_name, Json::Value &v, TItem &item) {
    switch (v.type()) {
      case Json::ValueType::intValue:
        item = v.asInt();
        break;
      case Json::ValueType::uintValue:
        item = v.asUInt();
        break;
      case Json::ValueType::realValue:
        item = v.asDouble();
        break;
      case Json::ValueType::booleanValue:
        item = v.asBool();
        break;
      default:
        std::stringstream s;
        s << "value type " << v.type()
          << " is not supported by current get method";
        Utils::ThrowException(field_name, s.str());
        break;
    }
  }

  void Fill(const std::string &field_name, Json::Value &v, std::string &item) {
    switch (v.type()) {
      case Json::ValueType::stringValue:
        item = v.asString();
        break;
      default:
        std::stringstream s;
        s << "value type " << v.type()
          << " is not supported by current get method";
        Utils::ThrowException(field_name, s.str());
        break;
    }
  }

  bool Exists(const std::string &field_name) {
    return (!v_[field_name].empty());
  }

  void Validate(const std::string &field_name, uint32_t max_size) {
    if (v_[field_name].empty()) {
      Utils::ThrowException(field_name, "is missing from JSON configuration");
    }
    if (max_size < v_[field_name].size()) {
      std::stringstream s;
      s << "value has size " << v_[field_name].size() << ", but max size is "
        << max_size;
      Utils::ThrowException(field_name, s.str());
    }
  }

 private:
  Json::Value v_;
};

};  // namespace qmmf_alg_plugin
};  // namespace qmmf
