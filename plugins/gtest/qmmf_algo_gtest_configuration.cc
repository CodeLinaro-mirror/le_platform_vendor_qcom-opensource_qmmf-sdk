/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
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

#define LOG_TAG "QmmfAlgoGtestConfiguration"

#include <qmmf-alg/qmmf_alg_utils.h>

#include "qmmf_algo_gtest_configuration.h"
#include "qmmf_json_helper.h"

namespace qmmf {
namespace qmmf_alg_plugin {

/** QmmfAlgoGtestConfiguration
 *    @v: parsed json value
 *
 * Constructs gtest configuration
 *
 * return: void
 **/
QmmfAlgoGtestConfiguration::QmmfAlgoGtestConfiguration(Json::Value &v) {
  FromJson(v);
}

/** FromJson
 *    @v: parsed json value
 *
 * parses json file
 *
 * return: void
 **/
void QmmfAlgoGtestConfiguration::FromJson(Json::Value &v) {
  QmmfJsonHelper h(v);
  h.Get("iteration count", iteration_count_);
  h.Get("tested library", tested_library_);

  std::string configuration_file;
  h.Get("configuration file", configuration_file);

  if ("" != configuration_file) {
    std::ifstream cfg_data(Utils::GetDataFolder() + configuration_file);
    if (cfg_data.is_open()) {
      configuration_data_ =
          std::string((std::istreambuf_iterator<char>(cfg_data)),
                      std::istreambuf_iterator<char>());
    } else {
      std::string err = std::string("library configuration file ") +
                        Utils::GetDataFolder() + configuration_file +
                        " not found ";
      Utils::ThrowException(__func__, err.c_str());
    }
  }

  std::string calibration_file;
  h.Get("calibration file", calibration_file);

  if ("" != calibration_file) {
    std::ifstream calib_data(Utils::GetAlgTuningFolder() + calibration_file,
                             std::ios::binary);
    //  std::ios::binary | std::ios::ate);
    if (calib_data.is_open()) {
      calib_data.seekg(0, std::ios::end);
      std::streamsize size = calib_data.tellg();
      calib_data.seekg(0, std::ios::beg);
      calibration_data_.resize(size);
      calib_data.read(reinterpret_cast<char *>(calibration_data_.data()), size);
    } else {
      std::string err = std::string("library calibration file ") +
                        Utils::GetAlgTuningFolder() + calibration_file +
                        " not found ";
      Utils::ThrowException(__func__, err.c_str());
    }
  }

  h.Get("input buffers", input_buffers_, false);
  if (input_buffers_.size() < 1) {
    Utils::ThrowException(__func__, "At least one input buffer is needed");
  }

  h.Get("output buffers", output_buffers_, false);
}

/** New
 *    @v: parsed json value
 *
 * returns new instance of QmmfAlgoGtestConfiguration
 *
 * return: new instance of QmmfAlgoGtestConfiguration
 **/
std::shared_ptr<QmmfAlgoGtestConfiguration> QmmfAlgoGtestConfiguration::New(
    Json::Value &v) {
  std::shared_ptr<QmmfAlgoGtestConfiguration> new_instance(
      new QmmfAlgoGtestConfiguration(v));
  return new_instance;
}

};  // namespace qmmf_alg_plugin
};  // namespace qmmf
