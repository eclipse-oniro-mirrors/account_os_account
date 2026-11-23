/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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

#include "napi_account_iam_onsetdata.h"
#include "account_log_wrapper.h"
#include "napi_account_error.h"
#include "napi_account_iam_constant.h"

namespace OHOS {
namespace AccountJsKit {
#ifdef HAS_PIN_AUTH_PART
napi_value InputDataConstructor(napi_env env, napi_callback_info info)
{
    napi_value thisVar;
    void *data;
    size_t argc = ARG_SIZE_ONE;
    napi_value argv[ARG_SIZE_ONE] = {nullptr};
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, argv, &thisVar, &data));
    if (thisVar == nullptr) {
        ACCOUNT_LOGE("ThisVar is nullptr");
        return nullptr;
    }
    InputerContext *context = static_cast<InputerContext *>(data);
    if (context == nullptr) {
        ACCOUNT_LOGE("InputerData is nullptr");
        return nullptr;
    }
    NAPI_CALL(env, napi_wrap(env, thisVar, context,
        [](napi_env env, void *data, void *hint) {
            InputerContext *context = static_cast<InputerContext *>(data);
            if (context != nullptr) {
                delete context;
            }
        },
        nullptr, nullptr));
    return thisVar;
}

napi_value OnSetData(napi_env env, napi_callback_info info)
{
    if (!IsSystemApp(env)) {
        return nullptr;
    }
    size_t argc = ARG_SIZE_TWO;
    napi_value thisVar = nullptr;
    napi_value argv[ARG_SIZE_TWO] = {nullptr};
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, argv, &thisVar, nullptr));
    if (argc != ARG_SIZE_TWO) {
        ThrowParamCountError(env, ARG_SIZE_TWO, argc, true);
        return nullptr;
    }
    InputerContext *context = nullptr;
    NAPI_CALL(env, napi_unwrap(env, thisVar, (void **)&context));
    if (context == nullptr || context->inputerData == nullptr) {
        ACCOUNT_LOGE("Context or inputerData is nullptr");
        return nullptr;
    }
    int32_t authSubType;
    if (!GetIntProperty(env, argv[PARAM_ZERO], authSubType)) {
        ThrowParamTypeError(env, PARAM_ZERO, argv[PARAM_ZERO], "AuthSubType", true);
        return nullptr;
    }
    std::vector<uint8_t> data;
    if (ParseUint8TypedArrayToVector(env, argv[PARAM_ONE], data) != napi_ok) {
        ThrowParamTypeError(env, PARAM_ONE, argv[PARAM_ONE], "Uint8Array", true);
        return nullptr;
    }
    ACCOUNT_LOGI("Call OnSetData, authSubType: %{public}d", authSubType);
    context->inputerData->OnSetData(authSubType, data);
    context->inputerData = nullptr;
    return nullptr;
}

napi_value GetCtorIInputerData(napi_env env, const std::shared_ptr<AccountSA::IInputerData> &inputerData)
{
    if (inputerData == nullptr) {
        ACCOUNT_LOGE("InputerData nullptr");
        return nullptr;
    }
    InputerContext *context = new (std::nothrow) InputerContext();
    if (context == nullptr) {
        ACCOUNT_LOGE("Inputer context is nullptr");
        return nullptr;
    }
    napi_property_descriptor clzDes[] = {
        DECLARE_NAPI_FUNCTION("onSetData", OnSetData),
    };
    context->inputerData = inputerData;
    napi_value cons;
    NAPI_CALL(env, napi_define_class(env, "InputerData", NAPI_AUTO_LENGTH,
        InputDataConstructor, reinterpret_cast<void *>(context),
        sizeof(clzDes) / sizeof(napi_property_descriptor), clzDes, &cons));
    return cons;
}

napi_status GetInputerInstance(InputerContext *context, napi_value *inputerDataVarCtor)
{
    napi_value cons = GetCtorIInputerData(context->env, context->inputerData);
    if (cons == nullptr) {
        return napi_generic_failure;
    }
    return napi_new_instance(context->env, cons, 0, nullptr, inputerDataVarCtor);
}
#endif // HAS_PIN_AUTH_PART
}
}