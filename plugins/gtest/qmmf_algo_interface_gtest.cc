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

#define LOG_TAG "QmmfAlgoInterfaceGtest"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <list>
#include <mutex>
#include <sstream>
#include <thread>

#include <gtest/gtest.h>

#include "qmmf-plugin/qmmf_alg_plugin.h"
#include "qmmf-plugin/qmmf_alg_utils.h"

#include "buffer_handler.h"
#include "heap_tracker.h"
#include "qmmf_algo_gtest_test_suite.h"

namespace qmmf {
namespace qmmf_alg_plugin {

static const auto kOneFrameProcessTimeout = std::chrono::seconds(10);

/** QmmfAlgoEventListener
 *    @lock_: mutex protection for thread safe
 *    @signal_: notification for end of processing
 *    @inplace_processing_: inplace processing
 *    @pending_input_buffers_: list of pending input buffers
 *    @pending_output_buffers_: list of pending input buffers
 *    @idle_input_buffers_: list of idle input buffers
 *    @idle_output_buffers_: list of idle input buffers
 *    @algo_: algorithm instance
 *    @time_stamps_: vector of frame ready time stamps
 *
 * Qmmf algorithm event listener
 *
 **/
class QmmfAlgoEventListener : public IEventListener {
 public:
  QmmfAlgoEventListener(
      bool inplace_processing,
      std::list<std::shared_ptr<BufferHandler>> idle_input_buffers,
      std::list<std::shared_ptr<BufferHandler>> idle_output_buffers,
      IAlgPlugin *algo)
      : inplace_processing_(inplace_processing),
        idle_input_buffers_(idle_input_buffers),
        idle_output_buffers_(idle_output_buffers),
        algo_(algo) {}
  ~QmmfAlgoEventListener() {}

  /** RemoveBuffers
   *    @input_buffers: input buffers to be removed
   *    @output_buffers: output buffers to be removed
   *
   * Removes input and output buffers from circulation
   *
   * return: void
   **/
  void RemoveBuffers(const std::vector<AlgBuffer> &input_buffers,
                     const std::vector<AlgBuffer> &output_buffers) {
    std::unique_lock<std::mutex> l(lock_);

    for (auto &input_buffer : input_buffers) {
      std::shared_ptr<BufferHandler> rb = nullptr;
      idle_input_buffers_.remove_if(
          [&input_buffer, &rb](std::shared_ptr<BufferHandler> b) {
            if (input_buffer.fd_ == b->fd_) {
              rb = b;
              return true;
            }
            return false;
          });

      if (nullptr == rb) {
        Utils::ThrowException(__func__, "Unexpected idle input buffer");
      }
    }

    for (auto &output_buffer : output_buffers) {
      std::shared_ptr<BufferHandler> rb = nullptr;
      idle_output_buffers_.remove_if(
          [&output_buffer, &rb](std::shared_ptr<BufferHandler> b) {
            if (output_buffer.fd_ == b->fd_) {
              rb = b;
              return true;
            }
            return false;
          });

      if (nullptr == rb) {
        Utils::ThrowException(__func__, "Unexpected idle output buffer");
      }
    }
  }

  /** GetIdleBuffers
   *    @num_input: number of input buffers to be returned
   *    @input_buffers: list of input buffers to be returned
   *    @num_output: number of output buffers to be returned
   *    @output_buffers: list of output buffers to be returned
   *    @configuration: requested configuration
   *    @duration: duration timeout
   *    @input_buffer_handlers: input buffer handlers
   *    @output_buffer_handlers: output buffer handlers
   *
   * Gets idle buffers
   *
   * return: void
   **/
  template <class Rep, class Period = std::ratio<1>>
  void GetIdleBuffers(
      const uint32_t num_input, std::vector<AlgBuffer> &input_buffers,
      const uint32_t num_output, std::vector<AlgBuffer> &output_buffers,
      const std::shared_ptr<QmmfAlgoGtestConfiguration> &configuration,
      const std::chrono::duration<Rep, Period> &duration,
      std::vector<std::shared_ptr<BufferHandler>> *input_buffer_handlers =
          nullptr,
      std::vector<std::shared_ptr<BufferHandler>> *output_buffer_handlers =
          nullptr) {
    std::unique_lock<std::mutex> l(lock_);

    auto rc = signal_.wait_for(l, duration, [&] {
      return ((num_input <= idle_input_buffers_.size()) &&
              (num_output <= idle_output_buffers_.size()));
    });
    if (!rc) {
      Utils::ThrowException(__func__, "algorithm process timed out !!!");
    }

    if (nullptr != input_buffer_handlers) {
      input_buffer_handlers->clear();
    }
    input_buffers.clear();

    for (auto &b : configuration->input_buffers_) {
      for (auto &idle_buf : idle_input_buffers_) {
        if (idle_buf->Compare(b)) {
          pending_input_buffers_.push_back(idle_buf);
          if (nullptr != input_buffer_handlers) {
            input_buffer_handlers->push_back(idle_buf);
          }
          input_buffers.push_back(*idle_buf);
          break;
        }
      }
    }

    for (auto &b : input_buffers) {
      idle_input_buffers_.remove_if(
          [&b](auto &idle_buf) { return idle_buf->Compare(b.fd_); });
    }

    if (nullptr != output_buffer_handlers) {
      output_buffer_handlers->clear();
    }
    output_buffers.clear();

    for (auto &b : configuration->output_buffers_) {
      for (auto &idle_buf : idle_output_buffers_) {
        if (idle_buf->Compare(b)) {
          pending_output_buffers_.push_back(idle_buf);
          if (nullptr != output_buffer_handlers) {
            output_buffer_handlers->push_back(idle_buf);
          }
          output_buffers.push_back(*idle_buf);
          break;
        }
      }
    }

    for (auto &b : output_buffers) {
      idle_output_buffers_.remove_if(
          [&b](auto &idle_buf) { return idle_buf->Compare(b.fd_); });
    }
  }

  /** Wait
   *    @duration: wait duration
   *
   * Waits event to be received
   *
   * return: void
   **/
  template <class Rep, class Period = std::ratio<1>>
  void Wait(const std::chrono::duration<Rep, Period> &duration) {
    std::unique_lock<std::mutex> l(lock_);
    auto rc = signal_.wait_for(l, duration, [&] {
      return (0 ==
              pending_input_buffers_.size() + pending_output_buffers_.size());
    });
    if (!rc) {
      Utils::ThrowException(__func__, "algorithm process timed out !!!");
    }
  }

  /** GetTimingPerformance
   *
   * Returns timing performance
   *
   * return: String containing timing performance
   **/
  std::string GetTimingPerformance() {
    std::unique_lock<std::mutex> l(lock_);
    std::stringstream s;

    if (1 < time_stamps_.size()) {
      std::chrono::duration<float> d =
          time_stamps_.back() - time_stamps_.front();
      s << "Average timing performance "
        << 1000 * d.count() / (time_stamps_.size() - 1) << " ms\n";

      size_t max_frame_dumps = 10;
      auto count = std::max(time_stamps_.size() - 1, max_frame_dumps);
      for (uint32_t i = 0; i < count; i++) {
        std::chrono::duration<float> d = time_stamps_[i + 1] - time_stamps_[i];
        s << "\tDiff between frame id " << i << " and " << i + 1 << " is "
          << 1000 * d.count() << " ms\n";
      }
    }

    return s.str();
  }

  /** OnFrameProcessed
   *    @input_buffer: input buffer
   *
   * Indicates that input buffer is processed
   *
   * return: void
   **/
  void OnFrameProcessed(const AlgBuffer &input_buffer) override {
    std::unique_lock<std::mutex> l(lock_);

    std::shared_ptr<BufferHandler> rb = nullptr;

    pending_input_buffers_.remove_if(
        [&input_buffer, &rb](std::shared_ptr<BufferHandler> b) {
          if (input_buffer.fd_ == b->fd_) {
            rb = b;
            return true;
          }
          return false;
        });

    if (nullptr == rb) {
      Utils::ThrowException(__func__, "Unexpected returned output buffer");
    }

    idle_input_buffers_.push_back(rb);

    std::vector<AlgBuffer> bufs;
    bufs.push_back(*rb);
    algo_->UnregisterInputBuffers(bufs);

    signal_.notify_one();
  }

  /** OnFrameReady
   *    @output_buffer: output buffer
   *
   * Indicates that output buffer is processed
   *
   * return: void
   **/
  void OnFrameReady(const AlgBuffer &output_buffer) override {
    std::unique_lock<std::mutex> l(lock_);
    time_stamps_.push_back(std::chrono::system_clock::now());

    std::list<std::shared_ptr<BufferHandler>> *pending_buffers_;
    if (inplace_processing_) {
      pending_buffers_ = &pending_input_buffers_;
    } else {
      pending_buffers_ = &pending_output_buffers_;
    }

    std::shared_ptr<BufferHandler> rb = nullptr;
    pending_buffers_->remove_if(
        [&output_buffer, &rb](std::shared_ptr<BufferHandler> b) {
          if (output_buffer.fd_ == b->fd_) {
            rb = b;
            return true;
          }
          return false;
        });

    if (nullptr == rb) {
      Utils::ThrowException(__func__, "Unexpected returned output buffer");
    }

    std::list<std::shared_ptr<BufferHandler>> *idle_buffers_;
    std::vector<AlgBuffer> bufs;
    bufs.push_back(*rb);
    if (inplace_processing_) {
      idle_buffers_ = &idle_input_buffers_;
      algo_->UnregisterInputBuffers(bufs);
    } else {
      idle_buffers_ = &idle_output_buffers_;
      algo_->UnregisterOutputBuffers(bufs);
    }

    idle_buffers_->push_back(rb);

    rb->WriteOutputFile();

    signal_.notify_one();
  }

  /** OnError
   *    @err: error id
   *
   * Indicates runtime error
   *
   * return: void
   **/
  void OnError(RuntimeError err) override {
    ALOGE("Runtime error : %s", std::to_string(err).c_str());
    ADD_FAILURE();
  }

 private:
  std::condition_variable signal_;
  std::mutex lock_;
  bool inplace_processing_;
  std::list<std::shared_ptr<BufferHandler>> pending_input_buffers_;
  std::list<std::shared_ptr<BufferHandler>> pending_output_buffers_;
  std::list<std::shared_ptr<BufferHandler>> idle_input_buffers_;
  std::list<std::shared_ptr<BufferHandler>> idle_output_buffers_;
  IAlgPlugin *algo_;
  std::vector<std::chrono::system_clock::time_point> time_stamps_;
};

/** QmmfAlgoInterfaceGtest
 *    @test_info_: gtest info
 *    @app_test_suite_file_: application test suite file
 *    @app_test_content_file_: application test content file
 *    @lib_handle_: lib handle
 *    @algo_: algorithm instance
 *    @test_suite_: test suite
 *    @configuration_: configuration for current test case
 *
 * Qmmf algorithm test
 *
 **/
class QmmfAlgoInterfaceGtest : public ::testing::Test {
 public:
  QmmfAlgoInterfaceGtest()
      : test_info_(nullptr),
        lib_handle_(nullptr),
        algo_(nullptr),
        test_suite_(QmmfAlgoGtestTestSuite::New(app_test_suite_file_,
                                                app_test_content_file_)),
        configuration_(nullptr) {}

  ~QmmfAlgoInterfaceGtest() { Deinit(); }

  /** UpdateTestSuiteFile
  *    @test_suite_file: application test suite file
  *
  * Updates test suite file
  *
  * return: void
  **/
  static void UpdateTestSuiteFile(std::string &test_suite_file) {
    app_test_suite_file_ = test_suite_file;
  };

  /** UpdateTestContentFile
  *    @test_content_file: application test content file
  *
  * Updates test content file
  *
  * return: void
  **/
  static void UpdateTestContentFile(std::string &test_content_file) {
    app_test_content_file_ = test_content_file;
  };

 protected:
  const ::testing::TestInfo *test_info_;

  static std::string app_test_suite_file_;
  static std::string app_test_content_file_;

  void *lib_handle_;
  IAlgPlugin *algo_;

  std::shared_ptr<QmmfAlgoGtestTestSuite> test_suite_;
  std::shared_ptr<QmmfAlgoGtestConfiguration> configuration_;

  /** SetUp
  *
  * Gtest setup
  *
  * return: void
  **/
  void SetUp() override {
    test_info_ = ::testing::UnitTest::GetInstance()->current_test_info();
  };

  /** SetUp
  *
  * Gtest tear down
  *
  * return: void
  **/
  void TearDown() override{};

  /** Init
  *
  * Initialize current test
  *
  * return: void
  **/
  void Init() {
    try {
      auto alg_lib_folder = Utils::GetLibFolder();
      Utils::LoadLib(alg_lib_folder + configuration_->tested_library_,
                     lib_handle_);

      QmmfAlgLoadPlugin LoadPluginFunc;
      Utils::LoadLibHandler(lib_handle_, "QmmfAlgoNew", LoadPluginFunc);

      algo_ = LoadPluginFunc(configuration_->calibration_data_);
    } catch (const std::exception &e) {
      Deinit();
      Utils::ThrowException(__func__, e.what());
    }
  }

  /** Deinit
  *
  * Deinitialize current test
  *
  * return: void
  **/
  void Deinit() {
    if (nullptr != algo_) {
      delete algo_;
      algo_ = nullptr;
    }

    Utils::UnloadLib(lib_handle_);
  }

  /** GetNumThreads
  *
  * Show current number of enable threads
  *
  * return: number of threads
  **/
  uint32_t GetNumThreads() {
    std::ifstream f("/proc/self/status");
    std::string str((std::istreambuf_iterator<char>(f)),
                    std::istreambuf_iterator<char>());
    std::string target("Threads:");
    auto pos = str.find(target);
    auto end_pos = str.find("SigQ:");
    auto tmp =
        str.substr(pos + target.length(), end_pos - pos - target.length());
    return std::stoi(tmp);
  }

  /** GetIonBuffers
  *
  * Show current number of ION buffers
  *
  * return: number of ION buffers
  **/
  uint32_t GetIonBuffers() {
    std::ifstream f("/sys/kernel/debug/ion/heaps/system");
    std::string str = std::string((std::istreambuf_iterator<char>(f)),
                                  std::istreambuf_iterator<char>());

    uint32_t count = 0;
    std::string target("qmmf_algo_inter");
    std::string::size_type pos = 0;
    while ((pos = str.find(target, pos)) != std::string::npos) {
      ++count;
      pos += target.length();
    }

    return count;
  }

  /** GetMemoryUsage
  *
  * Show current memory usage
  *
  * return: memory usage
  **/
  uint32_t GetMemoryUsage() { return heap_tracker_get_total_allocations(); }

  /** InvokeAlgo
  *    @tested_library: tested library
  *    @check_for_leaks: check for leaks
  *
  * Invokes algo
  *
  * return: void
  **/
  void InvokeAlgo(std::string tested_library, bool check_for_leaks) {
    auto InitThreadStart = GetNumThreads();
    auto InitIONStart = GetIonBuffers();
    auto InitMemStart = GetMemoryUsage();

    {
      Init();

      const auto caps = algo_->GetCaps();

      algo_->Configure(configuration_->configuration_data_);

      std::vector<AlgBuffer> input_buffers;
      std::vector<AlgBuffer> output_buffers;

      auto input_buffer_handlers = BufferHandler::New(
          caps.in_buffer_requirements_, configuration_->input_buffers_);
      auto output_buffer_handlers = BufferHandler::New(
          caps.out_buffer_requirements_, configuration_->output_buffers_);

      for (auto &b : input_buffer_handlers) {
        b->ReadInputFile();
      }

      QmmfAlgoEventListener l(caps.inplace_processing_, input_buffer_handlers,
                              output_buffer_handlers, algo_);
      algo_->SetCallbacks(&l);

      // Open cl driver creates callback on first invocation. So wait one
      //    process iteration to avoid false positive test failures
      if (check_for_leaks) {
        l.GetIdleBuffers(caps.in_buffer_requirements_.count_, input_buffers,
                         caps.out_buffer_requirements_.count_, output_buffers,
                         configuration_, kOneFrameProcessTimeout);

        algo_->RegisterInputBuffers(input_buffers);
        algo_->RegisterOutputBuffers(output_buffers);

        algo_->Process(input_buffers, output_buffers);

        l.Wait(kOneFrameProcessTimeout);
      }

      l.GetIdleBuffers(caps.in_buffer_requirements_.count_, input_buffers,
                       caps.out_buffer_requirements_.count_, output_buffers,
                       configuration_, kOneFrameProcessTimeout);

      algo_->RegisterInputBuffers(input_buffers);
      algo_->RegisterOutputBuffers(output_buffers);

      auto ProcessThreadStart = GetNumThreads();
      auto ProcessIONStart = GetIonBuffers();
      auto ProcessMemStart = GetMemoryUsage();

      algo_->Process(input_buffers, output_buffers);

      l.Wait(kOneFrameProcessTimeout);

      auto ProcessThreadEnd = GetNumThreads();
      auto ProcessIONEnd = GetIonBuffers();
      auto ProcessMemEnd = GetMemoryUsage();
      if (check_for_leaks) {
        EXPECT_GE(ProcessThreadStart, ProcessThreadEnd) << "Failed lib: "
                                                        << tested_library;
        EXPECT_EQ(ProcessIONStart, ProcessIONEnd) << "Failed lib: "
                                                  << tested_library;
        EXPECT_GE(ProcessMemStart, ProcessMemEnd) << "Failed lib: "
                                                  << tested_library;
      }

      Deinit();
    }

    auto InitMemEnd = GetMemoryUsage();
    auto InitThreadEnd = GetNumThreads();
    auto InitIONEnd = GetIonBuffers();

    if (check_for_leaks) {
      EXPECT_GE(InitThreadStart, InitThreadEnd) << "Failed lib: "
                                                << tested_library;
      EXPECT_EQ(InitIONStart, InitIONEnd) << "Failed lib: " << tested_library;
      EXPECT_GE(InitMemStart, InitMemEnd) << "Failed lib: " << tested_library;
    }
  }
};

std::string QmmfAlgoInterfaceGtest::app_test_suite_file_ = "";
std::string QmmfAlgoInterfaceGtest::app_test_content_file_ = "";

/*
    * NewInstance: This test case will test new instance API.
    * Api test sequence:
    *  - new instance
    *  - destroy instance
    */
TEST_F(QmmfAlgoInterfaceGtest, NewInstance) {
  std::stringstream err;
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());

  auto test_contents = test_suite_->GetTestContents();
  for (auto &test : test_contents) {
    configuration_ = test->GetConfiguration(test_info_->name());
    std::string tested_library = configuration_->tested_library_;
    fprintf(stderr, "\n++++++++++++ Tested Library %s ++++++++++++\n",
            tested_library.c_str());

    try {
      for (uint32_t i = 1; i <= configuration_->iteration_count_; i++) {
        fprintf(stderr, "test iteration = %d/%d\n", i,
                configuration_->iteration_count_);
        ALOGD("%s: Running Test(%s) iteration = %d\n", __func__,
              test_info_->name(), i);

        Init();
        Deinit();
      }
    } catch (const std::exception &e) {
      err << "\t" << tested_library << " : \n";
      err << "\t\t" << e.what() << "\n";
    }
  }

  ASSERT_EQ(err.str().length(), 0u) << "List of failed libraries:\n"
                                    << err.str();

  fprintf(stderr, "---------- Test Completed %s.%s ----------\n",
          test_info_->test_case_name(), test_info_->name());
}

/*
* GetCaps: This test case will test GetCaps API.
* Api test sequence:
*  - new instance
*  - GetCaps
*  - destroy instance
*/
TEST_F(QmmfAlgoInterfaceGtest, GetCaps) {
  std::stringstream err;
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());

  auto test_contents = test_suite_->GetTestContents();
  for (auto &test : test_contents) {
    configuration_ = test->GetConfiguration(test_info_->name());
    std::string tested_library = configuration_->tested_library_;
    fprintf(stderr, "\n++++++++++++ Tested Library %s ++++++++++++\n",
            tested_library.c_str());

    try {
      for (uint32_t i = 1; i <= configuration_->iteration_count_; i++) {
        fprintf(stderr, "test iteration = %d/%d\n", i,
                configuration_->iteration_count_);
        ALOGD("%s: Running Test(%s) iteration = %d\n", __func__,
              test_info_->name(), i);

        Init();

        const auto caps = algo_->GetCaps();
        ALOGD("%s: Algorithm Capabilities =\n%s\n", __func__,
              caps.ToString().c_str());

        Deinit();
      }
    } catch (const std::exception &e) {
      err << "\t" << tested_library << " : \n";
      err << "\t\t" << e.what() << "\n";
    }
  }

  ASSERT_EQ(err.str().length(), 0u) << "List of failed libraries:\n"
                                    << err.str();

  fprintf(stderr, "---------- Test Completed %s.%s ----------\n",
          test_info_->test_case_name(), test_info_->name());
}
/*
* GetInputRequirements: This test case will test algorithm input requirements.
* Api test sequence:
*  - new instance
*  - GetCaps
*  - RegisterInputBuffers
*  - UnregisterInputBuffers
*  - destroy instance
*/
TEST_F(QmmfAlgoInterfaceGtest, GetInputRequirements) {
  std::stringstream err;
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());

  auto test_contents = test_suite_->GetTestContents();
  for (auto &test : test_contents) {
    configuration_ = test->GetConfiguration(test_info_->name());
    std::string tested_library = configuration_->tested_library_;
    fprintf(stderr, "\n++++++++++++ Tested Library %s ++++++++++++\n",
            tested_library.c_str());

    try {
      for (uint32_t i = 1; i <= configuration_->iteration_count_; i++) {
        fprintf(stderr, "test iteration = %d/%d\n", i,
                configuration_->iteration_count_);
        ALOGD("%s: Running Test(%s) iteration = %d\n", __func__,
              test_info_->name(), i);

        Init();

        const auto caps = algo_->GetCaps();
        Requirements try_out_requirements;
        BufferRequirements buffer_requirements;
        if (caps.inplace_processing_) {
          buffer_requirements = caps.in_buffer_requirements_;
        } else {
          buffer_requirements = caps.out_buffer_requirements_;
        }

        // Min requirements
        try_out_requirements.width_ = buffer_requirements.min_width_;
        try_out_requirements.height_ = buffer_requirements.min_height_;
        try_out_requirements.stride_ =
            Utils::LCM(buffer_requirements.min_width_,
                       buffer_requirements.stride_alignment_);

        try_out_requirements.scanline_ = buffer_requirements.min_height_;
        try_out_requirements.formats_.insert(
            try_out_requirements.formats_.end(),
            buffer_requirements.pixel_formats_.begin(),
            buffer_requirements.pixel_formats_.end());

        auto result_input_requirements =
            algo_->GetInputRequirements(try_out_requirements);

        EXPECT_LE(result_input_requirements.width_,
                  buffer_requirements.max_width_)
            << "Failed lib: " << tested_library;
        EXPECT_GE(result_input_requirements.width_,
                  buffer_requirements.min_width_)
            << "Failed lib: " << tested_library;
        EXPECT_LE(result_input_requirements.height_,
                  buffer_requirements.max_height_)
            << "Failed lib: " << tested_library;
        EXPECT_GE(result_input_requirements.height_,
                  buffer_requirements.min_height_)
            << "Failed lib: " << tested_library;
        EXPECT_GE(result_input_requirements.stride_,
                  try_out_requirements.stride_)
            << "Failed lib: " << tested_library;
        EXPECT_GE(result_input_requirements.scanline_,
                  try_out_requirements.scanline_)
            << "Failed lib: " << tested_library;

        for (const auto &s : result_input_requirements.formats_) {
          const bool is_in = buffer_requirements.pixel_formats_.find(s) !=
                             buffer_requirements.pixel_formats_.end();
          EXPECT_TRUE(is_in) << "Failed lib: " << tested_library;
        }

        // Max requirements
        try_out_requirements.width_ = buffer_requirements.max_width_;
        try_out_requirements.height_ = buffer_requirements.max_height_;
        try_out_requirements.stride_ =
            Utils::LCM(buffer_requirements.max_width_,
                       buffer_requirements.stride_alignment_);
        try_out_requirements.scanline_ = buffer_requirements.max_height_;

        result_input_requirements =
            algo_->GetInputRequirements(try_out_requirements);

        EXPECT_LE(result_input_requirements.width_,
                  buffer_requirements.max_width_)
            << "Failed lib: " << tested_library;
        EXPECT_GE(result_input_requirements.width_,
                  buffer_requirements.min_width_)
            << "Failed lib: " << tested_library;
        EXPECT_LE(result_input_requirements.height_,
                  buffer_requirements.max_height_)
            << "Failed lib: " << tested_library;
        EXPECT_GE(result_input_requirements.height_,
                  buffer_requirements.min_height_)
            << "Failed lib: " << tested_library;
        EXPECT_GE(result_input_requirements.stride_,
                  try_out_requirements.stride_)
            << "Failed lib: " << tested_library;
        EXPECT_GE(result_input_requirements.scanline_,
                  try_out_requirements.scanline_)
            << "Failed lib: " << tested_library;

        for (const auto &s : result_input_requirements.formats_) {
          const bool is_in = buffer_requirements.pixel_formats_.find(s) !=
                             buffer_requirements.pixel_formats_.end();
          EXPECT_TRUE(is_in) << "Failed lib: " << tested_library;
        }

        // Average requirements
        try_out_requirements.width_ =
            (buffer_requirements.min_width_ + buffer_requirements.max_width_) >>
            1;
        try_out_requirements.height_ = (buffer_requirements.min_height_ +
                                        buffer_requirements.max_height_) >>
                                       1;
        try_out_requirements.stride_ = Utils::LCM(
            try_out_requirements.width_, buffer_requirements.stride_alignment_);
        try_out_requirements.scanline_ = try_out_requirements.height_;

        result_input_requirements =
            algo_->GetInputRequirements(try_out_requirements);

        EXPECT_LE(result_input_requirements.width_,
                  buffer_requirements.max_width_)
            << "Failed lib: " << tested_library;
        EXPECT_GE(result_input_requirements.width_,
                  buffer_requirements.min_width_)
            << "Failed lib: " << tested_library;
        EXPECT_LE(result_input_requirements.height_,
                  buffer_requirements.max_height_)
            << "Failed lib: " << tested_library;
        EXPECT_GE(result_input_requirements.height_,
                  buffer_requirements.min_height_)
            << "Failed lib: " << tested_library;
        EXPECT_GE(result_input_requirements.stride_,
                  try_out_requirements.stride_)
            << "Failed lib: " << tested_library;
        EXPECT_GE(result_input_requirements.scanline_,
                  try_out_requirements.scanline_)
            << "Failed lib: " << tested_library;

        for (const auto &s : result_input_requirements.formats_) {
          const bool is_in = buffer_requirements.pixel_formats_.find(s) !=
                             buffer_requirements.pixel_formats_.end();
          EXPECT_TRUE(is_in) << "Failed lib: " << tested_library;
        }

        Deinit();
      }
    } catch (const std::exception &e) {
      err << "\t" << tested_library << " : \n";
      err << "\t\t" << e.what() << "\n";
    }
  }

  ASSERT_EQ(err.str().length(), 0u) << "List of failed libraries:\n"
                                    << err.str();

  fprintf(stderr, "---------- Test Completed %s.%s ----------\n",
          test_info_->test_case_name(), test_info_->name());
}

/*
* Configure: This test case will test Configure API
* Api test sequence:
*  - new instance
*  - Configure
*  - destroy instance
*/
TEST_F(QmmfAlgoInterfaceGtest, Configure) {
  std::stringstream err;
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());

  auto test_contents = test_suite_->GetTestContents();
  for (auto &test : test_contents) {
    configuration_ = test->GetConfiguration(test_info_->name());
    std::string tested_library = configuration_->tested_library_;
    fprintf(stderr, "\n++++++++++++ Tested Library %s ++++++++++++\n",
            tested_library.c_str());

    try {
      for (uint32_t i = 1; i <= configuration_->iteration_count_; i++) {
        fprintf(stderr, "test iteration = %d/%d\n", i,
                configuration_->iteration_count_);
        ALOGD("%s: Running Test(%s) iteration = %d\n", __func__,
              test_info_->name(), i);

        Init();

        algo_->Configure(configuration_->configuration_data_);

        Deinit();
      }
    } catch (const std::exception &e) {
      err << "\t" << tested_library << " : \n";
      err << "\t\t" << e.what() << "\n";
    }
  }

  ASSERT_EQ(err.str().length(), 0u) << "List of failed libraries:\n"
                                    << err.str();

  fprintf(stderr, "---------- Test Completed %s.%s ----------\n",
          test_info_->test_case_name(), test_info_->name());
}

/*
* RegisterUnregisterInputBuffers: This test case will test
*   RegisterUnregisterInputBuffers API.
* Api test sequence:
*  - new instance
*  - GetCaps
*  - RegisterInputBuffers
*  - UnregisterInputBuffers
*  - destroy instance
*/
TEST_F(QmmfAlgoInterfaceGtest, RegisterUnregisterInputBuffers) {
  std::stringstream err;
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());

  auto test_contents = test_suite_->GetTestContents();
  for (auto &test : test_contents) {
    configuration_ = test->GetConfiguration(test_info_->name());
    std::string tested_library = configuration_->tested_library_;
    fprintf(stderr, "\n++++++++++++ Tested Library %s ++++++++++++\n",
            tested_library.c_str());

    try {
      for (uint32_t i = 1; i <= configuration_->iteration_count_; i++) {
        fprintf(stderr, "test iteration = %d/%d\n", i,
                configuration_->iteration_count_);
        ALOGD("%s: Running Test(%s) iteration = %d\n", __func__,
              test_info_->name(), i);

        Init();

        const auto caps = algo_->GetCaps();
        const auto b = configuration_->input_buffers_.front();

        for (auto &pix_fmt : caps.in_buffer_requirements_.pixel_formats_) {
          std::vector<AlgBuffer> buffers;

          auto buffer_handler = BufferHandler::New(
              caps.in_buffer_requirements_, pix_fmt, b->width_, b->height_,
              b->stride_, b->scanline_, std::string(""), std::string(""));
          AlgBuffer b = *buffer_handler;
          buffers.push_back(b);

          algo_->RegisterInputBuffers(buffers);
          algo_->UnregisterInputBuffers(buffers);
        }

        Deinit();
      }
    } catch (const std::exception &e) {
      err << "\t" << tested_library << " : \n";
      err << "\t\t" << e.what() << "\n";
    }
  }

  ASSERT_EQ(err.str().length(), 0u) << "List of failed libraries:\n"
                                    << err.str();

  fprintf(stderr, "---------- Test Completed %s.%s ----------\n",
          test_info_->test_case_name(), test_info_->name());
}

/*
* RegisterUnregisterOutputBuffers: This test case will test
*   RegisterUnregisterOutputBuffers API.
* Api test sequence:
*  - new instance
*  - GetCaps
*  - RegisterOutputBuffers
*  - UnregisterOutputBuffers
*  - destroy instance
*/
TEST_F(QmmfAlgoInterfaceGtest, RegisterUnregisterOutputBuffers) {
  std::stringstream err;
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());

  auto test_contents = test_suite_->GetTestContents();
  for (auto &test : test_contents) {
    configuration_ = test->GetConfiguration(test_info_->name());
    std::string tested_library = configuration_->tested_library_;
    fprintf(stderr, "\n++++++++++++ Tested Library %s ++++++++++++\n",
            tested_library.c_str());

    try {
      for (uint32_t i = 1; i <= configuration_->iteration_count_; i++) {
        fprintf(stderr, "test iteration = %d/%d\n", i,
                configuration_->iteration_count_);
        ALOGD("%s: Running Test(%s) iteration = %d\n", __func__,
              test_info_->name(), i);

        Init();

        if (configuration_->output_buffers_.size() > 0) {
          const auto caps = algo_->GetCaps();
          const auto b = configuration_->output_buffers_.front();

          for (auto &pix_fmt : caps.out_buffer_requirements_.pixel_formats_) {
            std::vector<AlgBuffer> buffers;

            auto buffer_handler = BufferHandler::New(
                caps.out_buffer_requirements_, pix_fmt, b->width_, b->height_,
                b->stride_, b->scanline_, std::string(""), std::string(""));
            AlgBuffer b = *buffer_handler;
            buffers.push_back(b);

            algo_->RegisterOutputBuffers(buffers);
            algo_->UnregisterOutputBuffers(buffers);
          }
        }

        Deinit();
      }
    } catch (const std::exception &e) {
      err << "\t" << tested_library << " : \n";
      err << "\t\t" << e.what() << "\n";
    }
  }

  ASSERT_EQ(err.str().length(), 0u) << "List of failed libraries:\n"
                                    << err.str();

  fprintf(stderr, "---------- Test Completed %s.%s ----------\n",
          test_info_->test_case_name(), test_info_->name());
}

/*
* Process: This test case will test Process API.
* Api test sequence:
*  - new instance
*  - GetCaps
*  - Configure
*  - RegisterInputBuffers
*  - RegisterOutputBuffers
*  - Process
*  - UnregisterInputBuffers
*  - UnregisterOutputBuffers
*  - RegisterInputBuffers
*  - RegisterOutputBuffers
*  - Process
*  - UnregisterInputBuffers
*  - UnregisterOutputBuffers
*  - destroy instance
*/
TEST_F(QmmfAlgoInterfaceGtest, Process) {
  std::stringstream err;
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());

  auto test_contents = test_suite_->GetTestContents();
  for (auto &test : test_contents) {
    configuration_ = test->GetConfiguration(test_info_->name());
    std::string tested_library = configuration_->tested_library_;
    fprintf(stderr, "\n++++++++++++ Tested Library %s ++++++++++++\n",
            tested_library.c_str());
    try {
      for (uint32_t i = 1; i <= configuration_->iteration_count_; i++) {
        fprintf(stderr, "test iteration = %d/%d\n", i,
                configuration_->iteration_count_);
        ALOGD("%s: Running Test(%s) iteration = %d\n", __func__,
              test_info_->name(), i);
        InvokeAlgo(tested_library, false);
      }
    } catch (const std::exception &e) {
      err << "\t" << tested_library << " : \n";
      err << "\t\t" << e.what() << "\n";
    }
  }

  ASSERT_EQ(err.str().length(), 0u) << "List of failed libraries:\n"
                                    << err.str();

  fprintf(stderr, "---------- Test Completed %s.%s ----------\n",
          test_info_->test_case_name(), test_info_->name());
}

/*
* Abort: This test case will test Abort API.
* Api test sequence:
*  - new instance
*  - GetCaps
*  - Configure
*  - RegisterInputBuffers
*  - RegisterOutputBuffers
*  - Abort
*  - Process
*  - Process
*  - Process
*  - Abort
*  - UnregisterInputBuffers
*  - UnregisterOutputBuffers
*  - destroy instance
*/
TEST_F(QmmfAlgoInterfaceGtest, Abort) {
  std::stringstream err;
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());

  auto test_contents = test_suite_->GetTestContents();
  for (auto &test : test_contents) {
    configuration_ = test->GetConfiguration(test_info_->name());
    std::string tested_library = configuration_->tested_library_;
    fprintf(stderr, "\n++++++++++++ Tested Library %s ++++++++++++\n",
            tested_library.c_str());

    try {
      Init();

      const auto caps = algo_->GetCaps();

      algo_->Configure(configuration_->configuration_data_);

      std::vector<AlgBuffer> input_buffers;
      std::vector<AlgBuffer> output_buffers;
      std::list<std::shared_ptr<BufferHandler>> input_buffer_handlers;
      std::list<std::shared_ptr<BufferHandler>> output_buffer_handlers;

      for (uint32_t i = 0; i < 3; i++) {
        auto input_handlers = BufferHandler::New(
            caps.in_buffer_requirements_, configuration_->input_buffers_);
        input_buffer_handlers.insert(input_buffer_handlers.end(),
                                     input_handlers.begin(),
                                     input_handlers.end());
        auto output_handlers = BufferHandler::New(
            caps.out_buffer_requirements_, configuration_->output_buffers_);
        output_buffer_handlers.insert(output_buffer_handlers.end(),
                                      output_handlers.begin(),
                                      output_handlers.end());
      }

      for (auto &b : input_buffer_handlers) {
        b->ReadInputFile();
      }

      QmmfAlgoEventListener l(caps.inplace_processing_, input_buffer_handlers,
                              output_buffer_handlers, algo_);

      algo_->SetCallbacks(&l);

      algo_->Abort();

      for (uint32_t i = 1; i <= configuration_->iteration_count_; i++) {
        fprintf(stderr, "test iteration = %d/%d\n", i,
                configuration_->iteration_count_);
        ALOGD("%s: Running Test(%s) iteration = %d\n", __func__,
              test_info_->name(), i);

        l.GetIdleBuffers(caps.in_buffer_requirements_.count_, input_buffers,
                         caps.out_buffer_requirements_.count_, output_buffers,
                         configuration_, kOneFrameProcessTimeout);

        algo_->RegisterInputBuffers(input_buffers);
        algo_->RegisterOutputBuffers(output_buffers);
        algo_->Process(input_buffers, output_buffers);
      }
      algo_->Abort();

      l.Wait(kOneFrameProcessTimeout);

      Deinit();
    } catch (const std::exception &e) {
      err << "\t" << tested_library << " : \n";
      err << "\t\t" << e.what() << "\n";
    }
  }

  ASSERT_EQ(err.str().length(), 0u) << "List of failed libraries:\n"
                                    << err.str();

  fprintf(stderr, "---------- Test Completed %s.%s ----------\n",
          test_info_->test_case_name(), test_info_->name());
}

/*
* Thread: This test case will test thread joining.
* Api test sequence:
*  - new instance
*  - GetCaps
*  - Configure
*  - RegisterInputBuffers
*  - RegisterOutputBuffers
*  - Process
*  - UnregisterInputBuffers
*  - UnregisterOutputBuffers
*  - RegisterInputBuffers
*  - RegisterOutputBuffers
*  - Process
*  - UnregisterInputBuffers
*  - UnregisterOutputBuffers
*  - destroy instance
*/
TEST_F(QmmfAlgoInterfaceGtest, Thread) {
  std::stringstream err;
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());

  auto test_contents = test_suite_->GetTestContents();
  for (auto &test : test_contents) {
    configuration_ = test->GetConfiguration(test_info_->name());
    std::string tested_library = configuration_->tested_library_;
    fprintf(stderr, "\n++++++++++++ Tested Library %s ++++++++++++\n",
            tested_library.c_str());

    try {
      for (uint32_t i = 1; i <= configuration_->iteration_count_; i++) {
        fprintf(stderr, "test iteration = %d/%d\n", i,
                configuration_->iteration_count_);
        ALOGD("%s: Running Test(%s) iteration = %d\n", __func__,
              test_info_->name(), i);

        bool error = false;
        std::thread thread([&]() {
          try {
            InvokeAlgo(tested_library, false);
          } catch (const std::exception &e) {
            error = true;
            err << "\t" << tested_library << " : \n";
            err << "\t\t" << e.what() << "\n";
          }
        });
        thread.join();
        if (true == error) {
          break;
        }
      }
    } catch (const std::exception &e) {
      err << "\t" << tested_library << " : \n";
      err << "\t\t" << e.what() << "\n";
    }
  }

  ASSERT_EQ(err.str().length(), 0u) << "List of failed libraries:\n"
                                    << err.str();

  fprintf(stderr, "---------- Test Completed %s.%s ----------\n",
          test_info_->test_case_name(), test_info_->name());
}

/*
* SimulateCamera: This test case will measure timing performance simulating
* camera input at 30 fps.
* Api test sequence:
*  - new instance
*  - GetCaps
*  - Configure
*  - RegisterInputBuffers
*  - RegisterOutputBuffers
*  - Process
*  - UnregisterInputBuffers
*  - UnregisterOutputBuffers
*  - destroy instance
*/
TEST_F(QmmfAlgoInterfaceGtest, SimulateCamera) {
  std::stringstream err;
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());

  auto test_contents = test_suite_->GetTestContents();
  for (auto &test : test_contents) {
    configuration_ = test->GetConfiguration(test_info_->name());
    std::string tested_library = configuration_->tested_library_;
    fprintf(stderr, "\n++++++++++++ Tested Library %s ++++++++++++\n",
            tested_library.c_str());

    try {
      Init();

      const auto caps = algo_->GetCaps();

      algo_->Configure(configuration_->configuration_data_);

      std::vector<AlgBuffer> input_buffers;
      std::vector<AlgBuffer> output_buffers;
      std::list<std::shared_ptr<BufferHandler>> input_buffer_handlers;
      std::list<std::shared_ptr<BufferHandler>> output_buffer_handlers;

      for (uint32_t i = 0; i < 3; i++) {
        auto input_handlers = BufferHandler::New(
            caps.in_buffer_requirements_, configuration_->input_buffers_);
        input_buffer_handlers.insert(input_buffer_handlers.end(),
                                     input_handlers.begin(),
                                     input_handlers.end());
        auto output_handlers = BufferHandler::New(
            caps.out_buffer_requirements_, configuration_->output_buffers_);
        output_buffer_handlers.insert(output_buffer_handlers.end(),
                                      output_handlers.begin(),
                                      output_handlers.end());
      }

      for (auto &b : input_buffer_handlers) {
        b->ReadInputFile();
      }

      QmmfAlgoEventListener l(caps.inplace_processing_, input_buffer_handlers,
                              output_buffer_handlers, algo_);

      algo_->SetCallbacks(&l);

      std::chrono::system_clock::time_point timePoint;
      for (uint32_t i = 1; i <= configuration_->iteration_count_; i++) {
        fprintf(stderr, "test iteration = %d/%d\n", i,
                configuration_->iteration_count_);
        ALOGD("%s: Running Test(%s) iteration = %d\n", __func__,
              test_info_->name(), i);

        l.GetIdleBuffers(caps.in_buffer_requirements_.count_, input_buffers,
                         caps.out_buffer_requirements_.count_, output_buffers,
                         configuration_, kOneFrameProcessTimeout);
        algo_->RegisterInputBuffers(input_buffers);
        algo_->RegisterOutputBuffers(output_buffers);

        if (i > 1) {
          std::this_thread::sleep_until(timePoint);
        }
        timePoint =
            std::chrono::system_clock::now() + std::chrono::microseconds(33333);
        algo_->Process(input_buffers, output_buffers);
      }

      fprintf(stderr, "%s\n", l.GetTimingPerformance().c_str());

      l.Wait(kOneFrameProcessTimeout);

      Deinit();
    } catch (const std::exception &e) {
      err << "\t" << tested_library << " : \n";
      err << "\t\t" << e.what() << "\n";
    }
  }

  ASSERT_EQ(err.str().length(), 0u) << "List of failed libraries:\n"
                                    << err.str();

  fprintf(stderr, "---------- Test Completed %s.%s ----------\n",
          test_info_->test_case_name(), test_info_->name());
}

/*
* TimingPerformance: This test case will measure timing performance
* Api test sequence:
*  - new instance
*  - GetCaps
*  - Configure
*  - RegisterInputBuffers
*  - RegisterOutputBuffers
*  - Process
*  - UnregisterInputBuffers
*  - UnregisterOutputBuffers
*  - destroy instance
*/
TEST_F(QmmfAlgoInterfaceGtest, TimingPerformance) {
  std::stringstream err;
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());

  auto test_contents = test_suite_->GetTestContents();
  for (auto &test : test_contents) {
    configuration_ = test->GetConfiguration(test_info_->name());
    std::string tested_library = configuration_->tested_library_;
    fprintf(stderr, "\n++++++++++++ Tested Library %s ++++++++++++\n",
            tested_library.c_str());

    try {
      Init();

      const auto caps = algo_->GetCaps();

      algo_->Configure(configuration_->configuration_data_);

      std::vector<AlgBuffer> input_buffers;
      std::vector<AlgBuffer> output_buffers;
      std::list<std::shared_ptr<BufferHandler>> input_buffer_handlers;
      std::list<std::shared_ptr<BufferHandler>> output_buffer_handlers;

      for (uint32_t i = 0; i < 3; i++) {
        auto input_handlers = BufferHandler::New(
            caps.in_buffer_requirements_, configuration_->input_buffers_);
        input_buffer_handlers.insert(input_buffer_handlers.end(),
                                     input_handlers.begin(),
                                     input_handlers.end());
        auto output_handlers = BufferHandler::New(
            caps.out_buffer_requirements_, configuration_->output_buffers_);
        output_buffer_handlers.insert(output_buffer_handlers.end(),
                                      output_handlers.begin(),
                                      output_handlers.end());
      }

      for (auto &b : input_buffer_handlers) {
        b->ReadInputFile();
      }

      QmmfAlgoEventListener l(caps.inplace_processing_, input_buffer_handlers,
                              output_buffer_handlers, algo_);

      algo_->SetCallbacks(&l);

      for (uint32_t i = 1; i <= configuration_->iteration_count_; i++) {
        fprintf(stderr, "test iteration = %d/%d\n", i,
                configuration_->iteration_count_);
        ALOGD("%s: Running Test(%s) iteration = %d\n", __func__,
              test_info_->name(), i);

        l.GetIdleBuffers(caps.in_buffer_requirements_.count_, input_buffers,
                         caps.out_buffer_requirements_.count_, output_buffers,
                         configuration_, kOneFrameProcessTimeout);
        algo_->RegisterInputBuffers(input_buffers);
        algo_->RegisterOutputBuffers(output_buffers);

        algo_->Process(input_buffers, output_buffers);
      }

      fprintf(stderr, "%s\n", l.GetTimingPerformance().c_str());

      l.Wait(kOneFrameProcessTimeout);

      Deinit();
    } catch (const std::exception &e) {
      err << "\t" << tested_library << " : \n";
      err << "\t\t" << e.what() << "\n";
    }
  }

  ASSERT_EQ(err.str().length(), 0u) << "List of failed libraries:\n"
                                    << err.str();

  fprintf(stderr, "---------- Test Completed %s.%s ----------\n",
          test_info_->test_case_name(), test_info_->name());
}

/*
* Consistency: This test case will check algorithm output consistency
* Api test sequence:
*  - new instance
*  - GetCaps
*  - Configure
*  - RegisterInputBuffers
*  - RegisterOutputBuffers
*  - Process
*  - UnregisterInputBuffers
*  - UnregisterOutputBuffers
*  - destroy instance
*/
TEST_F(QmmfAlgoInterfaceGtest, Consistency) {
  std::stringstream err;
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());

  auto test_contents = test_suite_->GetTestContents();
  for (auto &test : test_contents) {
    configuration_ = test->GetConfiguration(test_info_->name());
    std::string tested_library = configuration_->tested_library_;
    fprintf(stderr, "\n++++++++++++ Tested Library %s ++++++++++++\n",
            tested_library.c_str());

    try {
      Init();

      const auto caps = algo_->GetCaps();

      algo_->Configure(configuration_->configuration_data_);

      std::vector<AlgBuffer> input_buffers;
      std::vector<AlgBuffer> output_buffers;
      std::list<std::shared_ptr<BufferHandler>> input_buffer_handlers;
      std::list<std::shared_ptr<BufferHandler>> output_buffer_handlers;

      for (uint32_t i = 0; i < 4; i++) {
        auto input_handlers = BufferHandler::New(
            caps.in_buffer_requirements_, configuration_->input_buffers_);
        input_buffer_handlers.insert(input_buffer_handlers.end(),
                                     input_handlers.begin(),
                                     input_handlers.end());
        auto output_handlers = BufferHandler::New(
            caps.out_buffer_requirements_, configuration_->output_buffers_);
        output_buffer_handlers.insert(output_buffer_handlers.end(),
                                      output_handlers.begin(),
                                      output_handlers.end());
      }

      for (auto &b : input_buffer_handlers) {
        b->ReadInputFile();
      }

      QmmfAlgoEventListener l(caps.inplace_processing_, input_buffer_handlers,
                              output_buffer_handlers, algo_);

      algo_->SetCallbacks(&l);

      std::vector<std::shared_ptr<BufferHandler>> ref_input_buffer_handlers;
      std::vector<std::shared_ptr<BufferHandler>> ref_output_buffer_handlers;
      std::vector<std::shared_ptr<BufferHandler>> tested_input_buffer_handlers;
      std::vector<std::shared_ptr<BufferHandler>> tested_output_buffer_handlers;
      for (uint32_t i = 1; i <= configuration_->iteration_count_; i++) {
        fprintf(stderr, "test iteration = %d/%d\n", i,
                configuration_->iteration_count_);
        ALOGD("%s: Running Test(%s) iteration = %d\n", __func__,
              test_info_->name(), i);
        if (1 == i) {
          l.GetIdleBuffers(caps.in_buffer_requirements_.count_, input_buffers,
                           caps.out_buffer_requirements_.count_, output_buffers,
                           configuration_, kOneFrameProcessTimeout,
                           &ref_input_buffer_handlers,
                           &ref_output_buffer_handlers);
        } else {
          l.GetIdleBuffers(caps.in_buffer_requirements_.count_, input_buffers,
                           caps.out_buffer_requirements_.count_, output_buffers,
                           configuration_, kOneFrameProcessTimeout,
                           &tested_input_buffer_handlers,
                           &tested_output_buffer_handlers);
        }

        algo_->RegisterInputBuffers(input_buffers);
        algo_->RegisterOutputBuffers(output_buffers);

        algo_->Process(input_buffers, output_buffers);
        l.Wait(kOneFrameProcessTimeout);

        if (1 == i) {
          l.RemoveBuffers(input_buffers, output_buffers);
        } else {
          std::vector<std::shared_ptr<BufferHandler>> ref_buffer_handlers;
          std::vector<std::shared_ptr<BufferHandler>> tested_buffer_handlers;

          if (caps.inplace_processing_ == true) {
            ref_buffer_handlers = ref_input_buffer_handlers;
            tested_buffer_handlers = tested_input_buffer_handlers;
          } else {
            ref_buffer_handlers = ref_output_buffer_handlers;
            tested_buffer_handlers = tested_output_buffer_handlers;
          }

          EXPECT_EQ(tested_buffer_handlers.size(), ref_buffer_handlers.size())
              << "Failed lib: " << tested_library;

          for (auto &tb : tested_buffer_handlers) {
            auto it = std::find_if(std::begin(ref_buffer_handlers),
                                   std::end(ref_buffer_handlers),
                                   [&tb](std::shared_ptr<BufferHandler> &rb) {
                                     bool rc = false;
                                     try {
                                       rc = tb->Compare(rb);
                                     } catch (const std::exception &e) {
                                     }
                                     return rc;
                                   });
            if (it == std::end(ref_buffer_handlers)) {
              Utils::ThrowException(__func__,
                                    "Reference is different from algo output");
            }

            if (caps.inplace_processing_ == true) {
              tb->ReadInputFile();
            }
          }
        }
      }

      l.Wait(kOneFrameProcessTimeout);

      Deinit();
    } catch (const std::exception &e) {
      err << "\t" << tested_library << " : \n";
      err << "\t\t" << e.what() << "\n";
    }
  }

  ASSERT_EQ(err.str().length(), 0u) << "List of failed libraries:\n"
                                    << err.str();

  fprintf(stderr, "---------- Test Completed %s.%s ----------\n",
          test_info_->test_case_name(), test_info_->name());
}

/*
* Stride: This test case will check algorithm input output stride support
* Api test sequence:
*  - new instance
*  - GetCaps
*  - Configure
*  - RegisterInputBuffers
*  - RegisterOutputBuffers
*  - Process
*  - UnregisterInputBuffers
*  - UnregisterOutputBuffers
*  - destroy instance
*/
TEST_F(QmmfAlgoInterfaceGtest, Stride) {
  std::stringstream err;
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());

  auto test_contents = test_suite_->GetTestContents();
  for (auto &test : test_contents) {
    configuration_ = test->GetConfiguration(test_info_->name());
    std::string tested_library = configuration_->tested_library_;
    fprintf(stderr, "\n++++++++++++ Tested Library %s ++++++++++++\n",
            tested_library.c_str());

    try {
      Init();

      const auto caps = algo_->GetCaps();

      algo_->Configure(configuration_->configuration_data_);

      std::vector<AlgBuffer> input_buffers;
      std::vector<AlgBuffer> output_buffers;
      std::list<std::shared_ptr<BufferHandler>> input_buffer_handlers;
      std::list<std::shared_ptr<BufferHandler>> output_buffer_handlers;

      for (uint32_t i = 0; i < 4; i++) {
        uint32_t additional_stride =
            std::max(256u, caps.in_buffer_requirements_.stride_alignment_);

        auto input_handlers = BufferHandler::New(caps.in_buffer_requirements_,
                                                 configuration_->input_buffers_,
                                                 0, 0, 0, additional_stride);
        input_buffer_handlers.insert(input_buffer_handlers.end(),
                                     input_handlers.begin(),
                                     input_handlers.end());
        auto output_handlers = BufferHandler::New(
            caps.out_buffer_requirements_, configuration_->output_buffers_, 0,
            0, 0, additional_stride);
        output_buffer_handlers.insert(output_buffer_handlers.end(),
                                      output_handlers.begin(),
                                      output_handlers.end());
      }

      for (auto &b : input_buffer_handlers) {
        b->ReadInputFile();
      }

      QmmfAlgoEventListener l(caps.inplace_processing_, input_buffer_handlers,
                              output_buffer_handlers, algo_);

      algo_->SetCallbacks(&l);

      std::vector<std::shared_ptr<BufferHandler>> ref_input_buffer_handlers;
      std::vector<std::shared_ptr<BufferHandler>> ref_output_buffer_handlers;
      std::vector<std::shared_ptr<BufferHandler>> tested_input_buffer_handlers;
      std::vector<std::shared_ptr<BufferHandler>> tested_output_buffer_handlers;
      for (uint32_t i = 1; i <= configuration_->iteration_count_; i++) {
        fprintf(stderr, "test iteration = %d/%d\n", i,
                configuration_->iteration_count_);
        ALOGD("%s: Running Test(%s) iteration = %d\n", __func__,
              test_info_->name(), i);
        if (1 == i) {
          l.GetIdleBuffers(caps.in_buffer_requirements_.count_, input_buffers,
                           caps.out_buffer_requirements_.count_, output_buffers,
                           configuration_, kOneFrameProcessTimeout,
                           &ref_input_buffer_handlers,
                           &ref_output_buffer_handlers);
        } else {
          l.GetIdleBuffers(caps.in_buffer_requirements_.count_, input_buffers,
                           caps.out_buffer_requirements_.count_, output_buffers,
                           configuration_, kOneFrameProcessTimeout,
                           &tested_input_buffer_handlers,
                           &tested_output_buffer_handlers);
        }

        algo_->RegisterInputBuffers(input_buffers);
        algo_->RegisterOutputBuffers(output_buffers);

        algo_->Process(input_buffers, output_buffers);
        l.Wait(kOneFrameProcessTimeout);

        if (1 == i) {
          l.RemoveBuffers(input_buffers, output_buffers);
        } else {
          std::vector<std::shared_ptr<BufferHandler>> ref_buffer_handlers;
          std::vector<std::shared_ptr<BufferHandler>> tested_buffer_handlers;

          if (caps.inplace_processing_ == true) {
            ref_buffer_handlers = ref_input_buffer_handlers;
            tested_buffer_handlers = tested_input_buffer_handlers;
          } else {
            ref_buffer_handlers = ref_output_buffer_handlers;
            tested_buffer_handlers = tested_output_buffer_handlers;
          }

          EXPECT_EQ(tested_buffer_handlers.size(), ref_buffer_handlers.size())
              << "Failed lib: " << tested_library;

          for (auto &tb : tested_buffer_handlers) {
            auto it = std::find_if(std::begin(ref_buffer_handlers),
                                   std::end(ref_buffer_handlers),
                                   [&tb](std::shared_ptr<BufferHandler> &rb) {
                                     bool rc = false;
                                     try {
                                       rc = tb->Compare(rb);
                                     } catch (const std::exception &e) {
                                     }
                                     return rc;
                                   });
            if (it == std::end(ref_buffer_handlers)) {
              Utils::ThrowException(__func__,
                                    "Reference is different from algo output");
            }

            if (caps.inplace_processing_ == true) {
              tb->ReadInputFile();
            }
          }
        }
      }

      l.Wait(kOneFrameProcessTimeout);

      Deinit();
    } catch (const std::exception &e) {
      err << "\t" << tested_library << " : \n";
      err << "\t\t" << e.what() << "\n";
    }
  }

  ASSERT_EQ(err.str().length(), 0u) << "List of failed libraries:\n"
                                    << err.str();

  fprintf(stderr, "---------- Test Completed %s.%s ----------\n",
          test_info_->test_case_name(), test_info_->name());
}

/*
* HeapIonThreadLeaks: This test case will test API for leaks
* Api test sequence:
*  - mark ion, heap and thread usage for current instance
*  - new instance
*  - GetCaps
*  - Configure
*  - RegisterInputBuffers
*  - RegisterOutputBuffers
*  - Process
*  - UnregisterInputBuffers
*  - UnregisterOutputBuffers
*  - mark ion, heap and thread usage for process
*  - RegisterInputBuffers
*  - RegisterOutputBuffers
*  - Process
*  - UnregisterInputBuffers
*  - UnregisterOutputBuffers
*  - compare ion, heap and thread usage for process
*  - destroy instance
*  - compare ion, heap and thread usage for current instance
*/
TEST_F(QmmfAlgoInterfaceGtest, HeapIonThreadLeaks) {
  std::stringstream err;
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());

  auto test_contents = test_suite_->GetTestContents();
  for (auto &test : test_contents) {
    configuration_ = test->GetConfiguration(test_info_->name());
    std::string tested_library = configuration_->tested_library_;
    fprintf(stderr, "\n++++++++++++ Tested Library %s ++++++++++++\n",
            tested_library.c_str());

    // Invoke algorithm once to avoid false positives. Workaround for open cl
    //   driver allocates some resources for each new process
    InvokeAlgo(tested_library, false);

    try {
      for (uint32_t i = 1; i <= configuration_->iteration_count_; i++) {
        fprintf(stderr, "test iteration = %d/%d\n", i,
                configuration_->iteration_count_);
        ALOGD("%s: Running Test(%s) iteration = %d\n", __func__,
              test_info_->name(), i);
        InvokeAlgo(tested_library, true);
      }
    } catch (const std::exception &e) {
      err << "\t" << tested_library << " : \n";
      err << "\t\t" << e.what() << "\n";
    }
  }

  ASSERT_EQ(err.str().length(), 0u) << "List of failed libraries:\n"
                                    << err.str();

  fprintf(stderr, "---------- Test Completed %s.%s ----------\n",
          test_info_->test_case_name(), test_info_->name());
}

/*
* MemoryCorruption: This test case will check algorithm for memory corruption
* Api test sequence:
*  - new instance
*  - GetCaps
*  - Configure
*  - RegisterInputBuffers
*  - RegisterOutputBuffers
*  - Process
*  - UnregisterInputBuffers
*  - UnregisterOutputBuffers
*  - destroy instance
*/
TEST_F(QmmfAlgoInterfaceGtest, MemoryCorruption) {
  std::stringstream err;
  fprintf(stderr, "\n---------- Run Test %s.%s ------------\n",
          test_info_->test_case_name(), test_info_->name());

  auto test_contents = test_suite_->GetTestContents();
  for (auto &test : test_contents) {
    configuration_ = test->GetConfiguration(test_info_->name());
    std::string tested_library = configuration_->tested_library_;
    fprintf(stderr, "\n++++++++++++ Tested Library %s ++++++++++++\n",
            tested_library.c_str());

    try {
      Init();

      const auto caps = algo_->GetCaps();

      algo_->Configure(configuration_->configuration_data_);

      std::vector<AlgBuffer> input_buffers;
      std::vector<AlgBuffer> output_buffers;
      std::list<std::shared_ptr<BufferHandler>> input_buffer_handlers;
      std::list<std::shared_ptr<BufferHandler>> output_buffer_handlers;

      for (uint32_t i = 0; i < 4; i++) {
        uint32_t border =
            std::max(32u, caps.in_buffer_requirements_.stride_alignment_);

        auto input_handlers = BufferHandler::New(
            caps.in_buffer_requirements_, configuration_->input_buffers_,
            border, border, border, border);
        input_buffer_handlers.insert(input_buffer_handlers.end(),
                                     input_handlers.begin(),
                                     input_handlers.end());
        auto output_handlers = BufferHandler::New(
            caps.out_buffer_requirements_, configuration_->output_buffers_,
            border, border, border, border);
        output_buffer_handlers.insert(output_buffer_handlers.end(),
                                      output_handlers.begin(),
                                      output_handlers.end());
      }

      // This can be an arbitrary value;
      uint8_t padded_value = 0;

      if (caps.inplace_processing_ == true) {
        for (auto &b : input_buffer_handlers) {
          b->FillBufferWith(padded_value);
          padded_value += 127;
        }
      } else {
        for (auto &b : output_buffer_handlers) {
          b->FillBufferWith(padded_value);
          padded_value += 127;
        }
      }

      for (auto &b : input_buffer_handlers) {
        b->ReadInputFile();
      }

      QmmfAlgoEventListener l(caps.inplace_processing_, input_buffer_handlers,
                              output_buffer_handlers, algo_);

      algo_->SetCallbacks(&l);

      std::vector<std::shared_ptr<BufferHandler>> ref_input_buffer_handlers;
      std::vector<std::shared_ptr<BufferHandler>> ref_output_buffer_handlers;
      std::vector<std::shared_ptr<BufferHandler>> tested_input_buffer_handlers;
      std::vector<std::shared_ptr<BufferHandler>> tested_output_buffer_handlers;
      for (uint32_t i = 1; i <= configuration_->iteration_count_; i++) {
        fprintf(stderr, "test iteration = %d/%d\n", i,
                configuration_->iteration_count_);
        ALOGD("%s: Running Test(%s) iteration = %d\n", __func__,
              test_info_->name(), i);
        if (1 == i) {
          l.GetIdleBuffers(caps.in_buffer_requirements_.count_, input_buffers,
                           caps.out_buffer_requirements_.count_, output_buffers,
                           configuration_, kOneFrameProcessTimeout,
                           &ref_input_buffer_handlers,
                           &ref_output_buffer_handlers);
        } else {
          l.GetIdleBuffers(caps.in_buffer_requirements_.count_, input_buffers,
                           caps.out_buffer_requirements_.count_, output_buffers,
                           configuration_, kOneFrameProcessTimeout,
                           &tested_input_buffer_handlers,
                           &tested_output_buffer_handlers);
        }

        algo_->RegisterInputBuffers(input_buffers);
        algo_->RegisterOutputBuffers(output_buffers);

        algo_->Process(input_buffers, output_buffers);
        l.Wait(kOneFrameProcessTimeout);

        if (1 == i) {
          l.RemoveBuffers(input_buffers, output_buffers);
        } else {
          std::vector<std::shared_ptr<BufferHandler>> ref_buffer_handlers;
          std::vector<std::shared_ptr<BufferHandler>> tested_buffer_handlers;

          if (caps.inplace_processing_ == true) {
            ref_buffer_handlers = ref_input_buffer_handlers;
            tested_buffer_handlers = tested_input_buffer_handlers;
          } else {
            ref_buffer_handlers = ref_output_buffer_handlers;
            tested_buffer_handlers = tested_output_buffer_handlers;
          }

          EXPECT_EQ(tested_buffer_handlers.size(), ref_buffer_handlers.size())
              << "Failed lib: " << tested_library;

          for (auto &rb : ref_buffer_handlers) {
            if (rb->MemoryIsCorrupted()) {
              Utils::ThrowException(
                  __func__, "Memory is corrupted in reference buf handler");
            }
          }

          for (auto &tb : tested_buffer_handlers) {
            if (tb->MemoryIsCorrupted()) {
              Utils::ThrowException(
                  __func__, "Memory is corrupted in output buf handler");
            }
          }

          for (auto &tb : tested_buffer_handlers) {
            auto it = std::find_if(std::begin(ref_buffer_handlers),
                                   std::end(ref_buffer_handlers),
                                   [&tb](std::shared_ptr<BufferHandler> &rb) {
                                     bool rc = false;
                                     try {
                                       rc = tb->Compare(rb);
                                     } catch (const std::exception &e) {
                                     }
                                     return rc;
                                   });
            if (it == std::end(ref_buffer_handlers)) {
              Utils::ThrowException(__func__,
                                    "Reference is different from algo output");
            }
          }

          for (auto &tb : tested_buffer_handlers) {
            if (caps.inplace_processing_ == true) {
              tb->ReadInputFile();
            }
          }
        }
      }

      l.Wait(kOneFrameProcessTimeout);

      Deinit();
    } catch (const std::exception &e) {
      err << "\t" << tested_library << " : \n";
      err << "\t\t" << e.what() << "\n";
    }
  }

  ASSERT_EQ(err.str().length(), 0u) << "List of failed libraries:\n"
                                    << err.str();

  fprintf(stderr, "---------- Test Completed %s.%s ----------\n",
          test_info_->test_case_name(), test_info_->name());
}

};  // namespace qmmf_alg_plugin
};  // namespace qmmf

int main(int argc, char **argv) {
  for (int i = 1; i < argc; ++i) {
    std::string s(argv[i]);
    if (s == "--help") {
      printf("Custom test suite:\n");
      printf("  \033[0;32m--custom_test_suite\033[0m\n");
      printf("    Gtest custom test suite json file\n");
      printf("Custom test content:\n");
      printf("  \033[0;32m--custom_test_content\033[0m\n");
      printf("    Gtest custom test content json file\n");
    }
    if ((s == "--custom_test_suite") && (i + 1 < argc)) {
      std::string new_test_suite(argv[i + 1]);
      qmmf::qmmf_alg_plugin::QmmfAlgoInterfaceGtest::UpdateTestSuiteFile(
          new_test_suite);
    }
    if ((s == "--custom_test_content") && (i + 1 < argc)) {
      std::string new_test_content(argv[i + 1]);
      qmmf::qmmf_alg_plugin::QmmfAlgoInterfaceGtest::UpdateTestContentFile(
          new_test_content);
    }
  }

  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
