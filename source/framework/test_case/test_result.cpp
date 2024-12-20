/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test_result.h"

#include "framework/utility/error.h"

// clang-format off
const static std::unordered_map<TestResult, TestResultHelper::TestResultInfo> testResultInfoMapping = {
    //                                    stringMessage            printSingle   printAll   skipped  printAllResults
    {TestResult::Error,                   { "ERROR",               true ,        true ,     false,   true } },
    {TestResult::DriverFunctionNotFound,  { "NO_SUPPORT",          true ,        true,      true,    true } },
    {TestResult::DeviceNotCapable,        { "NO_SUPPORT",          true ,        false,     true,    true } },
    {TestResult::ApiNotCapable,           { "NO_SUPPORT (API)",    true ,        false,     true,    true } },
    {TestResult::KernelNotFound,          { "MISSING_KERNEL",      true ,        true ,     true,    true } },
    {TestResult::SkippedApi,              { "SKIPPED",             false,        false,     true,    true } },
    {TestResult::UnsupportedApi,          { "SKIPPED",             false,        false,     true,    true } },
    {TestResult::NoImplementation,        { "NO_IMPLEMENT",        true ,        false,     true,    true } },
    {TestResult::IntelExtensionsRequired, { "NO_SUPPORT",          true ,        false,     true,    true } },
    {TestResult::InvalidArgs,             { "INVALID_ARGS",        true ,        true ,     true,    true } },
    {TestResult::Nooped,                  { "NOOP",                true ,        true ,     true,    true } },
    {TestResult::FilteredOut,             { "FILTERED_OUT",        true ,        false,     true,    false} },
    {TestResult::VerificationFail,        { "VERIF_FAIL",          true ,        true ,     false,   true } },
    {TestResult::KernelBuildError,        { "KERNEL_BUILD_ERROR",  true ,        true ,     false,   true } },
};
// clang-format on

const TestResultHelper::TestResultInfo &
TestResultHelper::getTestResultInfo(TestResult testResult) {
  DEVELOPER_WARNING_IF(testResult == TestResult::Success,
                       "Tried to get metadata for TestResult::Success. This is "
                       "a benchmark framework issue.");

  const auto testResultInfo = testResultInfoMapping.find(testResult);
  FATAL_ERROR_IF(
      testResultInfo == testResultInfoMapping.end(),
      "No metadata for TestResult found. This is a benchmark framework issue.");
  return testResultInfo->second;
}
