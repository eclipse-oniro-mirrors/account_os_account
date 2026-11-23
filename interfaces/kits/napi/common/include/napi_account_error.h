/*
 * Copyright (c) 2022-2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef OS_ACCOUNT_INTERFACES_KITS_NAPI_COMMON_INCLUDE_NAPI_ACCOUNT_ERROR_H
#define OS_ACCOUNT_INTERFACES_KITS_NAPI_COMMON_INCLUDE_NAPI_ACCOUNT_ERROR_H

#include <cstdint>
#include <string>
#include "napi/native_api.h"
#include "napi/native_common.h"

namespace OHOS {
namespace AccountJsKit {
napi_value GenerateBusinessSuccess(napi_env env, bool throwErr);
napi_value GenerateBusinessError(napi_env env, int32_t jsErrCode, const std::string &jsErrMsg);
napi_value GenerateBusinessError(napi_env env, int32_t nativeErrCode);
napi_value GenerateAuthorizationBusinessError(napi_env env, int32_t nativeErrCode);
napi_value GenerateAuthorizationPublicBusinessError(napi_env env, int32_t nativeErrCode);
napi_value GenerateBusinessError(napi_env env, int32_t nativeErrCode, bool throwErr);
void GenerateBusinessError(const int32_t nativeErrCode, int32_t &jsErrCode, std::string &errMsg);
void AccountNapiThrow(napi_env env, int32_t jsErrCode, const std::string &jsErrMsg, bool throwErr = true);
void AccountNapiThrow(napi_env env, int32_t nativeErrCode, bool throwErr = true);
void AccountIAMNapiThrow(napi_env env, int32_t jsErrCode, bool throwErr);

std::string GetNapiValueTypeName(napi_env env, napi_value value);
napi_value GetPropertyValue(napi_env env, napi_value container, const std::string &propName);
std::string BuildParamCountError(size_t expectCount, size_t actualCount);
std::string BuildParamAtLeastCountError(size_t expectCount, size_t actualCount);
std::string BuildParamTypeError(int index, const std::string &expected, const std::string &actual);
std::string BuildParamTypeError(napi_env env, int index, napi_value value, const std::string &expected);
std::string BuildParamRequiredError(int index);
std::string BuildPropertyTypeError(int index, const std::string &propName, const std::string &expected,
    const std::string &actual);
std::string BuildPropertyTypeError(napi_env env, int index, const std::string &propName, napi_value value,
    const std::string &expected);
std::string BuildPropertyRequiredError(int index, const std::string &propName);

void ThrowParamTypeError(napi_env env, int index, napi_value value, const std::string &expected,
    bool throwErr = true);
void ThrowParamCountError(napi_env env, size_t expectCount, size_t actualCount, bool throwErr = true);
void ThrowParamAtLeastCountError(napi_env env, size_t expectCount, size_t actualCount, bool throwErr = true);
void ThrowParamRequiredError(napi_env env, int index, bool throwErr = true);
void ThrowPropertyTypeError(napi_env env, int index, const std::string &propName, napi_value value,
    const std::string &expected);
void ThrowPropertyRequiredError(napi_env env, int index, const std::string &propName, bool throwErr = true);
} // namespace AccountJsKit
} // namespace OHOS

#endif // OS_ACCOUNT_INTERFACES_KITS_NAPI_COMMON_INCLUDE_NAPI_ACCOUNT_ERROR_H
