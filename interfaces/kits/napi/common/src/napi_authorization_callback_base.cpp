/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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

#include "napi_authorization_callback_base.h"
#include <thread>
#include "napi_authorization_manager.h"
#include "account_log_wrapper.h"
#include "napi_account_error.h"
#include "napi_base_context.h"
#include "ui_extension_context.h"
#include "ability_context.h"
#include "authorization_ui_extension_callback.h"
#include "want.h"

using namespace OHOS::AccountSA;

namespace OHOS {
namespace AccountJsKit {

namespace {
constexpr int32_t MAX_RETRY_TIMES = 10;
constexpr int32_t DELAY_FOR_EXCEPTION = 100;
constexpr int32_t E_IPC_ERROR = 201;
constexpr int32_t E_IPC_SA_DIED = 202;

ErrCode NotifyAuthorizationResultWithRetry(const sptr<IRemoteObject>& callback, int32_t errCode,
    const std::vector<uint8_t> &iamToken = {}, int32_t accountId = -1, int32_t resultCode = 0)
{
    auto connectCallback = iface_cast<AccountSA::IConnectAbilityCallback>(callback);
    if (connectCallback == nullptr) {
        ACCOUNT_LOGE("ConnectAbilityCallback proxy is nullptr");
        return ERR_AUTHORIZATION_CREATE_UI_EXTENSION_ERROR;
    }
    ErrCode ret = ERR_OK;
    int retryTimes = 0;
    while (retryTimes < MAX_RETRY_TIMES) {
        ret = connectCallback->OnResult(errCode, iamToken, accountId, resultCode);
        if (ret == ERR_OK || (ret != E_IPC_ERROR && ret != E_IPC_SA_DIED)) {
            break;
        }
        retryTimes++;
        ACCOUNT_LOGE("Send OnResult failed, code=%{public}d, retryTimes=%{public}d", ret, retryTimes);
        std::this_thread::sleep_for(std::chrono::milliseconds(DELAY_FOR_EXCEPTION));
    }
    return ret;
}

static Ace::UIContent* GetUIContent(const std::shared_ptr<AcquireAuthorizationContext> &context)
{
    if (context == nullptr) {
        return nullptr;
    }
    Ace::UIContent* uiContent = nullptr;
    if (context->uiAbilityFlag) {
        uiContent = context->abilityContext->GetUIContent();
    } else if ((context->uiExtensionContext == nullptr) ||
        (context->uiExtensionContext->GetApplicationInfo() == nullptr)) {
        return uiContent;
    } else {
        uiContent = context->uiExtensionContext->GetUIContent();
    }

    return uiContent;
}
} // namespace
UIExtensionCallback::UIExtensionCallback(const std::shared_ptr<AcquireAuthorizationContext>& context)
    : context_(context)
{
    isHandled_ = false;
}

void UIExtensionCallback::ReleaseHandler(int32_t errCode, AuthorizationResultCode resultCode,
    const std::vector<uint8_t> &iamToken, int32_t accountId)
{
    ACCOUNT_LOGI("enter ReleaseHandler code:%{public}d, resultCode:%{public}d", errCode,
        static_cast<int32_t>(resultCode));
    int32_t resultCodeInt = static_cast<int32_t>(resultCode);
    NotifyAuthorizationResultWithRetry(callback_, errCode, iamToken, accountId, resultCodeInt);
    CloseUIExtension();
    context_ = nullptr;
}

OHOS::Ace::UIContent* UIExtensionCallback::GetUIContent()
{
    if (context_ == nullptr) {
        ACCOUNT_LOGE("Context is nullptr");
        return nullptr;
    }
    OHOS::Ace::UIContent* uiContent = nullptr;
    if (context_->uiAbilityFlag) {
        if (context_->abilityContext != nullptr) {
            uiContent = context_->abilityContext->GetUIContent();
        }
    } else if (context_->uiExtensionContext != nullptr) {
        uiContent = context_->uiExtensionContext->GetUIContent();
    }
    return uiContent;
}

void UIExtensionCallback::CloseUIExtension()
{
    OHOS::Ace::UIContent* uiContent = GetUIContent();
    if (uiContent == nullptr) {
        ACCOUNT_LOGE("Get ui content failed!");
        return;
    }
    uiContent->CloseModalUIExtension(sessionId_);
    ACCOUNT_LOGI("Close end, sessionId: %{public}d", sessionId_);
}

std::function<void()> OnConnectAbilityTask(const std::shared_ptr<AcquireAuthorizationContext> &context,
    const ConnectAbilityInfo &info, const sptr<IRemoteObject> &callback)
{
    return [context, info = std::move(info), callback] {
        if (context == nullptr) {
            ACCOUNT_LOGE("Context is nullptr");
            return;
        }

        auto uiExtCallback = std::make_shared<UIExtensionCallback>(context);
        AAFwk::Want want = UIExtensionCallbackBase::BuildWantFromConnectInfo(info);
        Ace::ModalUIExtensionCallbacks uiExtensionCallbacks =
            UIExtensionCallbackBase::CreateUIExtensionCallbacks(uiExtCallback);

        uiExtCallback->SetCallBack(callback);

        OHOS::Ace::UIContent* uiContent = uiExtCallback->GetUIContent();
        if (uiContent == nullptr) {
            ACCOUNT_LOGE("Get ui content failed!");
            auto connectCallback = iface_cast<AccountSA::IConnectAbilityCallback>(callback);
            if (connectCallback != nullptr) {
                std::vector<uint8_t> iamToken;
                connectCallback->OnResult(ERR_AUTHORIZATION_GET_CONTENT_ERROR, iamToken, -1, -1);
            }
            return;
        }

        Ace::ModalUIExtensionConfig config;
        int32_t sessionId = uiContent->CreateModalUIExtension(want, uiExtensionCallbacks, config);
        if (sessionId == 0) {
            ACCOUNT_LOGE("Create component failed, sessionId is 0");
            NotifyAuthorizationResultWithRetry(callback, ERR_AUTHORIZATION_CREATE_UI_EXTENSION_ERROR);
            return;
        }

        uiExtCallback->SetSessionId(sessionId);
        context->sessionId = sessionId;
        ACCOUNT_LOGI("CreateUIExtension success, sessionId: %{public}d", sessionId);
    };
}

ErrCode NapiAuthorizationResultCallback::OnConnectAbility(const ConnectAbilityInfo &info,
    const sptr<IRemoteObject> &callback)
{
    ACCOUNT_LOGI("NapiAuthorizationResultCallback OnConnectAbility");
    if (context_ == nullptr) {
        ACCOUNT_LOGI("CreateUIExtension has not context.");
        return ERR_AUTHORIZATION_CREATE_UI_EXTENSION_ERROR;
    }
    if (!context_->hasOptions || !context_->options.hasContext) {
        ACCOUNT_LOGI("CreateUIExtension has not context.");
        return ERR_AUTHORIZATION_CREATE_UI_EXTENSION_ERROR;
    }
    auto task = OnConnectAbilityTask(context_, info, callback);
    if (napi_ok != napi_send_event(env_, task, napi_eprio_vip, "AuthorizationCallback OnConnectAbility")) {
        ACCOUNT_LOGE("Post authorizationCallback OnConnectAbility failed.");
        return ERR_AUTHORIZATION_CREATE_UI_EXTENSION_ERROR;
    }
    ACCOUNT_LOGI("Post authorizationCallback OnConnectAbility finish.");
    return ERR_OK;
}

ErrCode NapiBatchAuthorizationResultCallback::OnConnectAbility(const ConnectAbilityInfo &info,
    const sptr<IRemoteObject> &callback)
{
    ACCOUNT_LOGI("NapiBatchAuthorizationResultCallback OnConnectAbility");
    if (context_ == nullptr) {
        ACCOUNT_LOGI("CreateUIExtension has not context.");
        return ERR_AUTHORIZATION_CREATE_UI_EXTENSION_ERROR;
    }
    if (!context_->hasOptions || !context_->options.hasContext) {
        ACCOUNT_LOGI("CreateUIExtension has not context.");
        return ERR_AUTHORIZATION_CREATE_UI_EXTENSION_ERROR;
    }
    auto task = OnConnectAbilityTask(context_, info, callback);
    if (napi_ok != napi_send_event(env_, task, napi_eprio_vip, "BatchAuthorizationCallback OnConnectAbility")) {
        ACCOUNT_LOGE("Post batchAuthorizationCallback OnConnectAbility failed.");
        return ERR_AUTHORIZATION_CREATE_UI_EXTENSION_ERROR;
    }
    ACCOUNT_LOGI("Post batchAuthorizationCallback OnConnectAbility finish.");
    return ERR_OK;
}

ErrCode NapiBatchAuthorizationResultCallback::OnResult(int32_t resultCode, const AuthorizationResult& result)
{
    return OnBatchResult(resultCode, {result});
}

ErrCode NapiBatchAuthorizationResultCallback::OnBatchResult(int32_t resultCode,
    const std::vector<AuthorizationResult>& results)
{
    bool expected = false;
    if (!resolved_.compare_exchange_strong(expected, true)) {
        return ERR_OK;
    }
    auto env = env_;
    auto deferred = deferred_;
    auto task = [env, deferred, resultCode, results] {
        napi_handle_scope scope = nullptr;
        napi_open_handle_scope(env, &scope);
        if (resultCode != ERR_OK) {
            napi_value errObj = GenerateAuthorizationPublicBusinessError(env, resultCode);
            napi_reject_deferred(env, deferred, errObj);
        } else {
            bool allServiceBusy = !results.empty();
            bool allInteractionNotAllowed = !results.empty();
            for (const auto &r : results) {
                if (r.resultCode != AccountSA::AuthorizationResultCode::AUTHORIZATION_SERVICE_BUSY) {
                    allServiceBusy = false;
                }
                if (r.resultCode !=
                    AccountSA::AuthorizationResultCode::AUTHORIZATION_INTERACTION_NOT_ALLOWED) {
                    allInteractionNotAllowed = false;
                }
            }
            if (allServiceBusy) {
                napi_value errObj = GenerateAuthorizationPublicBusinessError(env,
                    ERR_JS_AUTHORIZATION_SERVICE_BUSY);
                napi_reject_deferred(env, deferred, errObj);
            } else if (allInteractionNotAllowed) {
                napi_value errObj = GenerateAuthorizationPublicBusinessError(env,
                    ERR_JS_AUTHORIZATION_INTERACTION_NOT_ALLOWED);
                napi_reject_deferred(env, deferred, errObj);
            } else {
                napi_value global = nullptr;
                napi_get_global(env, &global);
                napi_value mapConstructor = nullptr;
                napi_get_named_property(env, global, "Map", &mapConstructor);
                napi_value resultMap = nullptr;
                napi_new_instance(env, mapConstructor, 0, nullptr, &resultMap);
                napi_value setFunc = nullptr;
                napi_get_named_property(env, resultMap, "set", &setFunc);
                for (size_t i = 0; i < results.size(); ++i) {
                    napi_value resultObj = nullptr;
                    napi_create_object(env, &resultObj);
                    napi_value resultCodeVal = nullptr;
                    napi_create_int32(env, static_cast<int32_t>(results[i].resultCode), &resultCodeVal);
                    napi_set_named_property(env, resultObj, "resultCode", resultCodeVal);
                    napi_value privilegeVal = nullptr;
                    napi_create_string_utf8(env, results[i].privilege.c_str(), results[i].privilege.length(),
                        &privilegeVal);
                    napi_set_named_property(env, resultObj, "privilege", privilegeVal);
                    napi_value args[] = {privilegeVal, resultObj};
                    napi_call_function(env, resultMap, setFunc, 2, args, nullptr);
                }
                napi_resolve_deferred(env, deferred, resultMap);
            }
        }
        napi_close_handle_scope(env, scope);
    };
    if (napi_ok != napi_send_event(env_, task, napi_eprio_vip, "BatchAuthorizationCallback OnBatchResult")) {
        ACCOUNT_LOGE("Post batchAuthorizationCallback OnBatchResult failed.");
    }
    deferred_ = nullptr;
    return ERR_OK;
}

std::function<void()> OnAuthorizationResultTask(
    const std::shared_ptr<AcquireAuthorizationContext> &asyncContextPtr,
    const NapiAuthorizationResultCallback::ResultToJsFunc &resultToJsFunc, bool isPublicApi)
{
    return [asyncContextPtr, resultToJsFunc, isPublicApi] {
        napi_handle_scope scope = nullptr;
        napi_open_handle_scope(asyncContextPtr->env, &scope);
        napi_value errJs = nullptr;
        napi_value resultJs = nullptr;
        if (asyncContextPtr->errCode == ERR_OK) {
            errJs = GenerateBusinessSuccess(asyncContextPtr->env, asyncContextPtr->throwErr);
            if (resultToJsFunc != nullptr) {
                resultToJsFunc(asyncContextPtr->env, asyncContextPtr->authorizationResult, resultJs);
            }
        } else {
            if (isPublicApi) {
                errJs = GenerateAuthorizationPublicBusinessError(asyncContextPtr->env, asyncContextPtr->errCode);
            } else {
                errJs = GenerateAuthorizationBusinessError(asyncContextPtr->env, asyncContextPtr->errCode);
            }
        }
        ReturnPromise(asyncContextPtr->env, asyncContextPtr.get(), errJs, resultJs);
        napi_close_handle_scope(asyncContextPtr->env, scope);
    };
}

ErrCode NapiAuthorizationResultCallback::OnResult(int32_t errCode, const AuthorizationResult& result)
{
    bool expected = false;
    if (!resolved_.compare_exchange_strong(expected, true)) {
        return ERR_OK;
    }
    auto asyncContextPtr = std::make_shared<AcquireAuthorizationContext>(env_, true);
    asyncContextPtr->errCode = errCode;
    asyncContextPtr->deferred = deferred_;
    asyncContextPtr->authorizationResult = result;
    if (asyncContextPtr->authorizationResult.privilege.empty() && context_ != nullptr) {
        asyncContextPtr->authorizationResult.privilege = context_->privilege;
    }
    if (isPublicApi_ && errCode == ERR_OK) {
        auto rc = static_cast<int32_t>(result.resultCode);
        if (rc == static_cast<int32_t>(AccountSA::AuthorizationResultCode::AUTHORIZATION_INTERACTION_NOT_ALLOWED)) {
            asyncContextPtr->errCode = ERR_JS_AUTHORIZATION_INTERACTION_NOT_ALLOWED;
        } else if (rc == static_cast<int32_t>(AccountSA::AuthorizationResultCode::AUTHORIZATION_SERVICE_BUSY)) {
            asyncContextPtr->errCode = ERR_JS_AUTHORIZATION_SERVICE_BUSY;
        }
    }
    if (context_ != nullptr && context_->hasOptions && context_->options.hasContext) {
        UIExtensionCallbackBase::CloseUIExtension(GetUIContent(context_), context_->sessionId);
    }
    auto task = OnAuthorizationResultTask(asyncContextPtr, resultToJsFunc_, isPublicApi_);
    if (napi_ok != napi_send_event(asyncContextPtr->env, task, napi_eprio_vip, "AuthorizationCallback OnResult")) {
        ACCOUNT_LOGE("Post AuthorizationCallback OnResult failed.");
    }
    asyncContextPtr.reset();
    deferred_ = nullptr;
    return ERR_OK;
}

bool ConvertContextObject(napi_env env, napi_value contextValue,
    AcquireAuthorizationContext *asyncContext)
{
    if (asyncContext == nullptr) {
        return false;
    }
    bool stageMode = false;
    napi_status status = OHOS::AbilityRuntime::IsStageContext(env, contextValue, stageMode);
    if (status != napi_ok || !stageMode) {
        ACCOUNT_LOGE("It is not a stage mode");
        return false;
    }
    auto context = AbilityRuntime::GetStageModeContext(env, contextValue);
    if (context == nullptr) {
        ACCOUNT_LOGE("Get context is nullptr");
        return false;
    }
    asyncContext->abilityContext = AbilityRuntime::Context::ConvertTo<AbilityRuntime::AbilityContext>(context);
    if (asyncContext->abilityContext != nullptr &&
        asyncContext->abilityContext->GetApplicationInfo() != nullptr) {
        ACCOUNT_LOGI("Convert to ability context success");
        asyncContext->uiAbilityFlag = true;
        asyncContext->options.isContextValid = true;
        return true;
    }
    ACCOUNT_LOGI("Convert to ability context failed, try ui extension context");
    asyncContext->uiExtensionContext =
        AbilityRuntime::Context::ConvertTo<AbilityRuntime::UIExtensionContext>(context);
    if (asyncContext->uiExtensionContext == nullptr ||
        asyncContext->uiExtensionContext->GetApplicationInfo() == nullptr) {
        ACCOUNT_LOGE("Convert to ui extension context failed");
        return false;
    }
    asyncContext->options.isContextValid = true;
    return true;
}

} // namespace AccountJsKit
} // namespace OHOS
