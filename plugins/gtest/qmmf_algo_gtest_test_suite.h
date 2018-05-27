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

#include <list>
#include <memory>
#include <string>

#include <json/json.h>

#include "qmmf_algo_gtest_test_content.h"

namespace qmmf {
namespace qmmf_alg_plugin {

/** QmmfAlgoGtestTestSuite:
 *    @test_contents_: list of all test contents
 *    @kTestContentsTag: JSON tag representing list of test contents
 *    @kDefaultTestSuite: default test suite
 *
 *  This class handles test suite for all test contents
 **/
class QmmfAlgoGtestTestSuite {
 private:
  QmmfAlgoGtestTestSuite(const std::string &test_suite_file,
                         const std::string &test_content_file);

  /** FromJson
    *    @r: parsed json value
    *
    * parses json file
    *
    * return: void
    **/
  void FromJson(Json::Value &r);

 public:
  /** New
    *    @test_suite_file: test suite file
    *    @test_content_file: application test content file
    *
    * returns new instance of QmmfAlgoGtestTestSuite
    *
    * return: new instance of QmmfAlgoGtestTestSuite
    **/
  static std::shared_ptr<QmmfAlgoGtestTestSuite> New(
      const std::string &test_suite_file, const std::string &test_content_file);

  /** GetTestContents
  *
  * Returns test suite
  *
  * return: list of test contents
  **/
  std::list<std::shared_ptr<QmmfAlgoGtestTestContent>> GetTestContents() const;

  /** ReadAndParseFile
    *    @input_file: input file
    *
    * Reads and parses input file
    *
    * return: new parsed json value
    **/
  Json::Value ReadAndParseFile(const std::string &input_file);

 private:
  std::list<std::shared_ptr<QmmfAlgoGtestTestContent>> test_contents_;
  static const std::string kTestContentsTag;
  static const std::string kDefaultTestSuite;
};

};  // namespace qmmf_alg_plugin
};  // namespace qmmf
