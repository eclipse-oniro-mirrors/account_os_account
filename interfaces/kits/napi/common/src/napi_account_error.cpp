/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#include "napi_account_error.h"
#include "account_error_no.h"
#include "account_log_wrapper.h"

using namespace OHOS::AccountSA;

namespace OHOS {
namespace AccountJsKit {
napi_value GenerateBusinessError(napi_env env, int32_t jsErrCode, const std::string &jsErrMsg)
{
    napi_value errCodeJs = nullptr;
    NAPI_CALL(env, napi_create_uint32(env, jsErrCode, &errCodeJs));

    napi_value errMsgJs = nullptr;
    NAPI_CALL(env, napi_create_string_utf8(env, jsErrMsg.c_str(), NAPI_AUTO_LENGTH, &errMsgJs));

    napi_value errJs = nullptr;
    NAPI_CALL(env, napi_create_error(env, nullptr, errMsgJs, &errJs));
    NAPI_CALL(env, napi_set_named_property(env, errJs, "code", errCodeJs));
    NAPI_CALL(env, napi_set_named_property(env, errJs, "message", errMsgJs));
    return errJs;
}

static napi_value GetErrorCodeValue(napi_env env, int errCode)
{
    napi_value jsObject = nullptr;
    napi_value jsValue = nullptr;
    NAPI_CALL(env, napi_create_int32(env, errCode, &jsValue));
    NAPI_CALL(env, napi_create_object(env, &jsObject));
    NAPI_CALL(env, napi_set_named_property(env, jsObject, "code", jsValue));
    return jsObject;
}

napi_value GenerateBusinessSuccess(napi_env env, bool throwErr)
{
    if (throwErr) {
        napi_value errJs = nullptr;
        napi_get_null(env, &errJs);
        return errJs;
    }
    return GetErrorCodeValue(env, 0);
}

napi_value GenerateBusinessError(napi_env env, int32_t nativeErrCode, bool throwErr)
{
    if (throwErr) {
        return GenerateBusinessError(env, nativeErrCode);
    }
    return GetErrorCodeValue(env, nativeErrCode);
}

napi_value GenerateBusinessError(napi_env env, int32_t nativeErrCode)
{
    int32_t jsErrCode = GenerateBusinessErrorCode(nativeErrCode);
    std::string jsErrMsg = ConvertToJsErrMsg(jsErrCode);
    return GenerateBusinessError(env, jsErrCode, jsErrMsg);
}

napi_value GenerateAuthorizationBusinessError(napi_env env, int32_t nativeErrCode)
{
    int32_t jsErrCode = AuthorizationConvertToJsErrCode(nativeErrCode);
    std::string jsErrMsg = ConvertToJsErrMsg(jsErrCode);
    return GenerateBusinessError(env, jsErrCode, jsErrMsg);
}

napi_value GenerateAuthorizationPublicBusinessError(napi_env env, int32_t nativeErrCode)
{
    int32_t jsErrCode = AuthorizationPublicConvertToJsErrCode(nativeErrCode);
    return GenerateBusinessError(env, jsErrCode, ConvertToJsErrMsg(jsErrCode));
}

void AccountNapiThrow(napi_env env, int32_t nativeErrCode, bool throwErr)
{
    if (throwErr) {
        napi_throw(env, GenerateBusinessError(env, nativeErrCode));
    }
}

void AccountNapiThrow(napi_env env, int32_t jsErrCode, const std::string &jsErrMsg, bool throwErr)
{
    if (throwErr) {
        napi_throw(env, GenerateBusinessError(env, jsErrCode, jsErrMsg));
    }
}

void AccountIAMNapiThrow(napi_env env, int32_t jsErrCode, bool throwErr)
{
    if (throwErr) {
        napi_throw(env, GenerateBusinessError(env, jsErrCode, ConvertToJsErrMsg(jsErrCode)));
    }
}

static std::string GetTypedArrayTypeName(napi_env env, napi_value value)
{
    napi_typedarray_type arrayType = napi_uint8_array;
    size_t length = 0;
    void* data = nullptr;
    napi_value arraybuffer = nullptr;
    size_t byteOffset = 0;
    napi_status status = napi_get_typedarray_info(env, value, &arrayType, &length, &data, &arraybuffer, &byteOffset);
    if (status != napi_ok) {
        return "typedarray";
    }
    switch (arrayType) {
        case napi_int8_array:
            return "Int8Array";
        case napi_uint8_array:
            return "Uint8Array";
        case napi_uint8_clamped_array:
            return "Uint8ClampedArray";
        case napi_int16_array:
            return "Int16Array";
        case napi_uint16_array:
            return "Uint16Array";
        case napi_int32_array:
            return "Int32Array";
        case napi_uint32_array:
            return "Uint32Array";
        case napi_float32_array:
            return "Float32Array";
        case napi_float64_array:
            return "Float64Array";
        default:
            return "typedarray";
    }
}

static std::string GetObjectTypeName(napi_env env, napi_value value)
{
    bool isArray = false;
    if (napi_is_array(env, value, &isArray) != napi_ok) {
        return "object";
    }
    if (isArray) {
        return "array";
    }
    bool isTypedarray = false;
    if (napi_is_typedarray(env, value, &isTypedarray) != napi_ok) {
        return "object";
    }
    if (isTypedarray) {
        return GetTypedArrayTypeName(env, value);
    }
    return "object";
}

std::string GetNapiValueTypeName(napi_env env, napi_value value)
{
    napi_valuetype type = napi_undefined;
    napi_status status = napi_typeof(env, value, &type);
    if (status != napi_ok) {
        return "unknown";
    }
    switch (type) {
        case napi_string:
            return "string";
        case napi_number:
            return "number";
        case napi_boolean:
            return "boolean";
        case napi_object:
            return GetObjectTypeName(env, value);
        case napi_function:
            return "function";
        case napi_undefined:
            return "undefined";
        case napi_null:
            return "null";
        case napi_symbol:
            return "symbol";
        case napi_bigint:
            return "bigint";
        default:
            return "unknown";
    }
}

napi_value GetPropertyValue(napi_env env, napi_value container, const std::string &propName)
{
    napi_value propValue = nullptr;
    if (napi_get_named_property(env, container, propName.c_str(), &propValue) != napi_ok) {
        return nullptr;
    }
    return propValue;
}

std::string BuildParamCountError(size_t expectCount, size_t actualCount)
{
    return "Parameter error. Incorrect parameter count. Expect: " + std::to_string(expectCount) +
        ", Actual: " + std::to_string(actualCount) + ".";
}

std::string BuildParamAtLeastCountError(size_t expectCount, size_t actualCount)
{
    return "Parameter error. Incorrect parameter count. Expect: at least " + std::to_string(expectCount) +
        ", Actual: " + std::to_string(actualCount) + ".";
}

std::string BuildParamTypeError(int index, const std::string &expected, const std::string &actual)
{
    return "Parameter error. Param[" + std::to_string(index) + "] type mismatch. Expected: " + expected +
        ", Actual: " + actual + ".";
}

std::string BuildParamTypeError(napi_env env, int index, napi_value value, const std::string &expected)
{
    return BuildParamTypeError(index, expected, GetNapiValueTypeName(env, value));
}

std::string BuildParamRequiredError(int index)
{
    return "Parameter error. Param[" + std::to_string(index) +
        "] is required, cannot be null or undefined.";
}

std::string BuildPropertyTypeError(int index, const std::string &propName, const std::string &expected,
    const std::string &actual)
{
    return "Parameter error. Param[" + std::to_string(index) + "]." + propName +
        " type mismatch. Expected: " + expected + ", Actual: " + actual + ".";
}

std::string BuildPropertyTypeError(napi_env env, int index, const std::string &propName, napi_value value,
    const std::string &expected)
{
    return BuildPropertyTypeError(index, propName, expected, GetNapiValueTypeName(env, value));
}

std::string BuildPropertyRequiredError(int index, const std::string &propName)
{
    return "Parameter error. Param[" + std::to_string(index) + "]." + propName +
        " is required, cannot be null or undefined.";
}

void ThrowParamTypeError(napi_env env, int index, napi_value value, const std::string &expected, bool throwErr)
{
    std::string errMsg = BuildParamTypeError(env, index, value, expected);
    ACCOUNT_LOGE("%{public}s", errMsg.c_str());
    AccountNapiThrow(env, ERR_JS_PARAMETER_ERROR, errMsg, throwErr);
}

void ThrowParamCountError(napi_env env, size_t expectCount, size_t actualCount, bool throwErr)
{
    std::string errMsg = BuildParamCountError(expectCount, actualCount);
    ACCOUNT_LOGE("%{public}s", errMsg.c_str());
    AccountNapiThrow(env, ERR_JS_PARAMETER_ERROR, errMsg, throwErr);
}

void ThrowParamAtLeastCountError(napi_env env, size_t expectCount, size_t actualCount, bool throwErr)
{
    std::string errMsg = BuildParamAtLeastCountError(expectCount, actualCount);
    ACCOUNT_LOGE("%{public}s", errMsg.c_str());
    AccountNapiThrow(env, ERR_JS_PARAMETER_ERROR, errMsg, throwErr);
}

void ThrowParamRequiredError(napi_env env, int index, bool throwErr)
{
    std::string errMsg = BuildParamRequiredError(index);
    ACCOUNT_LOGE("%{public}s", errMsg.c_str());
    AccountNapiThrow(env, ERR_JS_PARAMETER_ERROR, errMsg, throwErr);
}

void ThrowPropertyTypeError(napi_env env, int index, const std::string &propName, napi_value value,
    const std::string &expected)
{
    std::string errMsg = BuildPropertyTypeError(env, index, propName, value, expected);
    ACCOUNT_LOGE("%{public}s", errMsg.c_str());
    AccountNapiThrow(env, ERR_JS_PARAMETER_ERROR, errMsg, true);
}

void ThrowPropertyRequiredError(napi_env env, int index, const std::string &propName, bool throwErr)
{
    std::string errMsg = BuildPropertyRequiredError(index, propName);
    ACCOUNT_LOGE("%{public}s", errMsg.c_str());
    AccountNapiThrow(env, ERR_JS_PARAMETER_ERROR, errMsg, throwErr);
}
} // namespace AccountJsKit
} // namespace OHOS
