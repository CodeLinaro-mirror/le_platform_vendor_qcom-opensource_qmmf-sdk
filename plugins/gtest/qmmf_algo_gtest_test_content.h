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
#include <map>
#include <memory>
#include <string>

#include "qmmf_algo_gtest_configuration.h"

namespace qmmf {
namespace qmmf_alg_plugin {

/** QmmfAlgoGtestTestContent:
 *    @configurations_: map with configurations for all test cases
 *    @kCommonConfigurationTag: JSON configuration tag representing common
 *       configuration for all test cases
 *
 *  This class handles algo configurations for all test cases for given library
 **/
class QmmfAlgoGtestTestContent {
 private:
  QmmfAlgoGtestTestContent(Json::Value &v);

  /** FromJson
    *    @v: parsed json value
    *
    * parses json file
    *
    * return: void
    **/
  void FromJson(Json::Value &v);

 public:
  /** New
    *    @v: parsed json value
    *
    * returns new instance of QmmfAlgoGtestTestContent
    *
    * return: new instance of QmmfAlgoGtestTestContent
    **/
  static std::shared_ptr<QmmfAlgoGtestTestContent> New(Json::Value &v);

  /** GetConfiguration
  *    @test_case: test case name
  *
  * Returns test case configuration
  *
  * return: test case configuration
  **/
  std::shared_ptr<QmmfAlgoGtestConfiguration> GetConfiguration(
      const std::string &test_case);

 private:
  std::map<const std::string, std::shared_ptr<QmmfAlgoGtestConfiguration>>
      configurations_;
  static const std::string kCommonConfigurationTag;
};

};  // namespace qmmf_alg_plugin
};  // namespace qmmf
