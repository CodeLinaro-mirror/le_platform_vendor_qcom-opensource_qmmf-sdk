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

#define LOG_TAG "QmmfAlgoGtestTestSuite"

#include <fstream>

#include <qmmf-alg/qmmf_alg_utils.h>

#include "qmmf_algo_gtest_test_suite.h"
#include "qmmf_json_helper.h"

namespace qmmf {
namespace qmmf_alg_plugin {

const std::string QmmfAlgoGtestTestSuite::kTestContentsTag = "test contents";

const std::string QmmfAlgoGtestTestSuite::kDefaultTestSuite(
    "{ "
    "  \"test contents\": ["
    "   \"qmmf_algo_gtest_yuv_cac.json\","
    "   \"qmmf_algo_gtest_hazebuster.json\","
    "   \"qmmf_algo_gtest_bayer_lcac.json\","
    "   \"qmmf_algo_gtest_sw_tnr.json\","
    "   \"qmmf_algo_gtest_binning_correction.json\","
    "   \"qmmf_algo_gtest_svhdr.json\","
    "   \"qmmf_algo_gtest_edge_smooth.json\","
    "   \"qmmf_algo_gtest_privacy_mask_resizer.json\","
    "   \"qmmf_algo_gtest_bit_blit.json\""
    "  ]"
    "}");

/** QmmfAlgoGtestTestSuite
  *    @test_suite_file: test suite file
  *    @test_content_file: application test content file
  *
  * Constructs gtest test suite
  *
  * return: void
  **/
QmmfAlgoGtestTestSuite::QmmfAlgoGtestTestSuite(
    const std::string &test_suite_file, const std::string &test_content_file) {
  if (test_content_file.length()) {
    Json::Value root = ReadAndParseFile(test_content_file);
    test_contents_.push_back(QmmfAlgoGtestTestContent::New(root));
  } else {
    Json::Value root = ReadAndParseFile(test_suite_file);
    FromJson(root);
  }
}

/** FromJson
  *    @r: parsed json value
  *
  * parses json file
  *
  * return: void
  **/
void QmmfAlgoGtestTestSuite::FromJson(Json::Value &r) {
  QmmfJsonHelper h(r);

  std::list<std::string> test_content_files;
  h.Get(kTestContentsTag, test_content_files);

  for (auto &m : test_content_files) {
    auto v = ReadAndParseFile(m);
    test_contents_.push_back(QmmfAlgoGtestTestContent::New(v));
  }
}

/** New
  *    @test_suite_file: test suite file
  *    @test_content_file: application test content file
  *
  * returns new instance of QmmfAlgoGtestTestSuite
  *
  * return: new instance of QmmfAlgoGtestTestSuite
  **/
std::shared_ptr<QmmfAlgoGtestTestSuite> QmmfAlgoGtestTestSuite::New(
    const std::string &test_suite_file, const std::string &test_content_file) {
  std::shared_ptr<QmmfAlgoGtestTestSuite> new_instance(
      new QmmfAlgoGtestTestSuite(test_suite_file, test_content_file));
  return new_instance;
}

/** GetTestContents
*
* Returns test contents
*
* return: list of test contents
**/
std::list<std::shared_ptr<QmmfAlgoGtestTestContent>>
QmmfAlgoGtestTestSuite::GetTestContents() const {
  return test_contents_;
}

/** ReadAndParseFile
  *    @input_file: input file
  *
  * Reads and parses input file
  *
  * return: new parsed json value
  **/
Json::Value QmmfAlgoGtestTestSuite::ReadAndParseFile(
    const std::string &input_file) {
  Json::Reader r;
  Json::Value v;

  std::string data;

  if (input_file == "") {
    data = kDefaultTestSuite;
  } else {
    std::ifstream cfg_data(Utils::GetDataFolder() + input_file);
    if (!cfg_data.is_open()) {
      std::string err = std::string("gtest test suit file ") +
                        Utils::GetDataFolder() + input_file + " not found ";
      Utils::ThrowException(__func__, err);
    }

    data = std::string((std::istreambuf_iterator<char>(cfg_data)),
                       std::istreambuf_iterator<char>());
  }

  if (!r.parse(data, v)) {
    std::string err = std::string("json parsing failed ") +
                      r.getFormattedErrorMessages().c_str();
    Utils::ThrowException(__func__, err);
  }

  return v;
}

};  // namespace qmmf_alg_plugin
};  // namespace qmmf
