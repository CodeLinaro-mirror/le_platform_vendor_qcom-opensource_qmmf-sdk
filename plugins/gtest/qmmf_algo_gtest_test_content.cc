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

#define LOG_TAG "QmmfAlgoGtestTestContent"

#include <fstream>

#include <qmmf-alg/qmmf_alg_utils.h>

#include "qmmf_algo_gtest_test_content.h"
#include "qmmf_json_helper.h"

namespace qmmf {
namespace qmmf_alg_plugin {

const std::string QmmfAlgoGtestTestContent::kCommonConfigurationTag = "Common";

/** QmmfAlgoGtestTestContent
  *    @v: parsed json value
  *
  * Constructs test content
  *
  * return: void
  **/
QmmfAlgoGtestTestContent::QmmfAlgoGtestTestContent(Json::Value &v) {
  FromJson(v);
}

/** FromJson
  *    @v: parsed json value
  *
  * parses json file
  *
  * return: void
  **/
void QmmfAlgoGtestTestContent::FromJson(Json::Value &v) {
  if (v[kCommonConfigurationTag].empty()) {
    Utils::ThrowException(__func__,
                          std::string("Missing mandatory field \"") +
                              kCommonConfigurationTag +
                              "\" in the application configuration file");
  }

  QmmfJsonHelper h(v);
  for (auto &m : v.getMemberNames()) {
    h.Get(m, configurations_);
  }
}

/** New
  *    @v: parsed json value
  *
  * returns new instance of QmmfAlgoGtestTestContent
  *
  * return: new instance of QmmfAlgoGtestTestContent
  **/
std::shared_ptr<QmmfAlgoGtestTestContent> QmmfAlgoGtestTestContent::New(
    Json::Value &v) {
  std::shared_ptr<QmmfAlgoGtestTestContent> new_instance(
      new QmmfAlgoGtestTestContent(v));
  return new_instance;
}

/** GetConfiguration
*    @test_case: test case name
*
* Returns test case configuration
*
* return: test case configuration
**/
std::shared_ptr<QmmfAlgoGtestConfiguration>
QmmfAlgoGtestTestContent::GetConfiguration(const std::string &test_case) {
  std::shared_ptr<QmmfAlgoGtestConfiguration> rc = configurations_[test_case];
  if (nullptr == rc) {
    rc = configurations_[kCommonConfigurationTag];
  }

  return rc;
}

};  // namespace qmmf_alg_plugin
};  // namespace qmmf
