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

#include "napi_account_iam_inputer_manager.h"

#include "account_iam_client.h"
#include "account_log_wrapper.h"
#include "napi_account_common.h"
#include "napi_account_error.h"
#include "napi_account_iam_common.h"

namespace OHOS {
namespace AccountJsKit {
using namespace OHOS::AccountSA;

napi_value NapiAccountIAMInputerManager::Init(napi_env env, napi_value exports)
{
    napi_property_descriptor clzDes[] = {
        DECLARE_NAPI_STATIC_FUNCTION("registerInputer", RegisterInputer),
        DECLARE_NAPI_STATIC_FUNCTION("unregisterInputer", UnregisterInputer),
        DECLARE_NAPI_FUNCTION("registerInputer", RegisterInputer),
        DECLARE_NAPI_FUNCTION("unregisterInputer", UnregisterInputer)
    };
    napi_value cons;
    NAPI_CALL(env, napi_define_class(env, "InputerManager", NAPI_AUTO_LENGTH, InputerManagerConstructor,
        nullptr, sizeof(clzDes) / sizeof(napi_property_descriptor), clzDes, &cons));
    NAPI_CALL(env, napi_set_named_property(env, exports, "InputerManager", cons));
    return exports;
}

napi_value NapiAccountIAMInputerManager::InputerManagerConstructor(napi_env env, napi_callback_info info)
{
    napi_value thisVar = nullptr;
    NAPI_CALL(env, napi_get_cb_info(env, info, nullptr, nullptr, &thisVar, nullptr));
    return thisVar;
}

static bool ParseContextForRegisterInputer(
    napi_env env, napi_callback_info info, int32_t &authType, napi_ref &callback)
{
    size_t argc = ARG_SIZE_TWO;
    napi_value argv[ARG_SIZE_TWO] = {nullptr};
    NAPI_CALL_BASE(env, napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr), false);
    if (argc != ARG_SIZE_TWO) {
        ThrowParamCountError(env, ARG_SIZE_TWO, argc, true);
        return false;
    }
    if (!GetIntProperty(env, argv[PARAM_ZERO], authType)) {
        ThrowParamTypeError(env, PARAM_ZERO, argv[PARAM_ZERO], "AuthType", true);
        return false;
    }
    bool hasProp = false;
    napi_has_named_property(env, argv[PARAM_ONE], "onGetData", &hasProp);
    if (!hasProp) {
        ThrowPropertyRequiredError(env, PARAM_ONE, "onGetData", true);
        return false;
    }
    napi_value onGetData = nullptr;
    napi_get_named_property(env, argv[PARAM_ONE], "onGetData", &onGetData);
    if (!GetCallbackProperty(env, onGetData, callback, 1)) {
        ThrowPropertyTypeError(env, PARAM_ONE, "onGetData", onGetData, "function");
        return false;
    }
    return true;
}

napi_value NapiAccountIAMInputerManager::RegisterInputer(napi_env env, napi_callback_info info)
{
    int32_t authType = -1;
    napi_ref callback = nullptr;
    if (!ParseContextForRegisterInputer(env, info, authType, callback)) {
        return nullptr;
    }
    auto napiCallback = std::make_shared<NapiCallbackRef>(env, callback);
    auto inputer = std::make_shared<NapiGetDataCallback>(env, napiCallback);
    ErrCode errCode = AccountIAMClient::GetInstance().RegisterInputer(authType, inputer);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("Failed to register inputer, errCode=%{public}d", errCode);
        AccountIAMNapiThrow(env, AccountIAMConvertToJSErrCode(errCode), true);
    }
    return nullptr;
}

napi_value NapiAccountIAMInputerManager::UnregisterInputer(napi_env env, napi_callback_info info)
{
    size_t argc = ARG_SIZE_ONE;
    napi_value argv[ARG_SIZE_ONE] = {nullptr};
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr));
    if (argc != ARG_SIZE_ONE) {
        ThrowParamCountError(env, ARG_SIZE_ONE, argc, true);
        return nullptr;
    }
    int32_t authType = -1;
    if (!GetIntProperty(env, argv[PARAM_ZERO], authType)) {
        ThrowParamTypeError(env, PARAM_ZERO, argv[PARAM_ZERO], "AuthType", true);
        return nullptr;
    }
    ErrCode errCode = AccountIAMClient::GetInstance().UnregisterInputer(authType);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("Failed to unregister inputer, errCode=%{public}d", errCode);
        AccountIAMNapiThrow(env, AccountIAMConvertToJSErrCode(errCode), true);
    }
    return nullptr;
}
}  // namespace AccountJsKit
}  // namespace OHOS
