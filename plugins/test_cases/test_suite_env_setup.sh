#!/bin/bash
#
# Copyright (c) 2018, The Linux Foundation. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
#       copyright notice, this list of conditions and the following
#       disclaimer in the documentation and/or other materials provided
#       with the distribution.
#     * Neither the name of The Linux Foundation nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
# WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
# ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
# BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
# OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
# IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

function print_green () {
  tput setaf 2; echo "$1"; tput sgr0;
}

function print_yellow () {
  tput setaf 3; echo "$1"; tput sgr0;
}

function print_red () {
  tput setaf 1; echo "$1"; tput sgr0;
}

function print_white () {
  tput setaf 7; echo "$1"; tput sgr0;
}

function detect_le
{
  if [ ${#BUILDDIR} -eq 0 ]; then
    return 1
  fi
  return 0
}

function detect_la
{
  if [ ${#ANDROID_PRODUCT_OUT} -eq 0 ]; then
    return 1
  fi
  return 0
}

function detect_pc
{
  ls -la ../../build &>/dev/null
  local rc=$?
  if [ $rc -eq 0 ]; then
    return 0
  fi
  ls -la ../../../build &>/dev/null
  rc=$?
  if [ $rc -eq 0 ]; then
    return 0
  fi
  return 1
}

function run_test_case () {
  local PLATFORM_TEST_CASE="$1"

  pushd $TEST_FOLDER > /dev/null

  if detect_le; then
    adb shell "qmmf_algo_interface_gtest --gtest_color=yes --custom_test_suite $PLATFORM_TEST_CASE"
  else
    if detect_la; then
      adb shell "data/nativetest/qmmf_algo_interface_gtest/qmmf_algo_interface_gtest --gtest_color=yes --custom_test_suite $PLATFORM_TEST_CASE"
    else
      if detect_pc; then
        ../../build/usr/bin/qmmf_algo_interface_gtest --gtest_color=yes --custom_test_suite "$PLATFORM_TEST_CASE"
      fi
    fi
  fi

  popd > /dev/null
}

function push_file_to_target () {
  local FILE_TO_BE_PUSHED="$1"
  local OVERWRITE="$2"
  local TARGET_NAME=`echo $FILE_TO_BE_PUSHED | rev | cut -d '/' -f 1 | rev`

  if detect_le; then
    local ERROR_LOG="$(adb shell ls -al /data/misc/qmmf/$TARGET_NAME 2>&1)"
    if [[ $ERROR_LOG == *"No such file or directory"* || "$OVERWRITE" == "r" ]]; then
      adb push "$FILE_TO_BE_PUSHED" "/data/misc/qmmf/$TARGET_NAME"
    fi
  else
    if detect_la; then
      local ERROR_LOG="$(adb shell ls -al /data/misc/qmmf/$TARGET_NAME 2>&1)"
      if [[ $ERROR_LOG == *"No such file or directory"* || "$OVERWRITE" == "r" ]]; then
        adb push "$FILE_TO_BE_PUSHED" "/data/misc/qmmf/$TARGET_NAME"
      fi
    else
      if detect_pc; then
        cp "-fv$OVERWRITE" "$FILE_TO_BE_PUSHED" "../../build/usr/data/$TARGET_NAME"
      else
        print_red "Error: Environment NOT detected !!!"
      fi
    fi
  fi
}

function run_test_suite ()
{
  pushd $TEST_FOLDER > /dev/null

  # push test input images to target
  for TEST_IMAGE in test_images/*; do
    push_file_to_target "$TEST_IMAGE"
  done

  # push test cases to target
  for TEST_CASE in *.json; do
    push_file_to_target "$TEST_CASE" r
  done

  # push calibration files for the tested algorithms to target
  for TEST_CALIBRATION in calibration/*.json; do
    push_file_to_target "$TEST_CALIBRATION" r
  done

  # run test suite
  run_test_case test_suite.json

  popd > /dev/null
}

function run_single_test_case ()
{
  local TEST_CASE="$1"

  pushd $TEST_FOLDER > /dev/null

  # push test input images to target
  for TEST_IMAGE in test_images/*; do
    push_file_to_target "$TEST_IMAGE"
  done

  # push test case to target
  push_file_to_target "$TEST_CASE" r

  # create temp single test suite
  local TEST_SUIT="{\"test contents\": [ $TEST_CASE ]}"
  echo "{" > single_test_suite.json
  echo "  \"test contents\": [" >> single_test_suite.json
  echo "    \"$TEST_CASE\"" >> single_test_suite.json
  echo "  ]" >> single_test_suite.json
  echo "}" >> single_test_suite.json
  push_file_to_target single_test_suite.json r

  # run temp single test suite
  run_test_case single_test_suite.json

  # remove temp single test suite
  rm -f single_test_suite.json
  # remove_file_from_target single_test_suite.json

  popd > /dev/null
}

function remove_file_from_target () {
  local FILE_TO_BE_REMOVED="$1"

  if detect_le; then
    adb shell rm /data/misc/qmmf/$FILE_TO_BE_REMOVED &>/dev/null
  else
    if detect_la; then
      adb shell rm /data/misc/qmmf/$FILE_TO_BE_REMOVED &>/dev/null
    else
      if detect_pc; then
        rm -rfv "../../build/usr/data/$FILE_TO_BE_REMOVED" &>/dev/null
      else
        print_red "Error: Environment NOT detected !!!"
      fi
    fi
  fi
}

function clean_target ()
{
  pushd $TEST_FOLDER > /dev/null

  # remove test input images from target
  pushd test_images > /dev/null
  for TEST_IMAGE in *; do
    remove_file_from_target "$TEST_IMAGE"
  done
  popd > /dev/null

  # remove test cases from target
  for TEST_CASE in *.json; do
    remove_file_from_target "$TEST_CASE"
  done

  print_green "Target cleaned !!!"

  popd > /dev/null
}

TEST_FOLDER="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

print_yellow "===== Exported functions ====="
print_green  "run_test_suite"
print_white  "   runs all test cases described in $TEST_FOLDER/test_suite.json on the target"
print_yellow "run_single_test_case <test case>"
print_white  "   runs single test case on the target"
print_red    "clean_target"
print_white  "   removes all test cases and test images from target"