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

#ifndef TAIHE_AUTHORIZATION_CALLBACK_BASE_H
#define TAIHE_AUTHORIZATION_CALLBACK_BASE_H

#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>
#include "account_constants.h"
#include "account_error_no.h"
#include "account_log_wrapper.h"
#include "ani_ui_extension.h"
#include "authorization_callback.h"

namespace OHOS {
namespace AccountSA {

ErrCode NotifyAuthorizationResultWithRetry(const sptr<IRemoteObject>& callback, int32_t errCode,
    const std::vector<uint8_t> &iamToken = {}, int32_t accountId = -1, int32_t resultCode = 0);

static inline bool CheckAllBatchResultsMatch(const std::vector<AuthorizationResult> &results,
    AuthorizationResultCode code)
{
    if (results.empty()) {
        return false;
    }
    for (const auto &result : results) {
        if (result.resultCode != code) {
            return false;
        }
    }
    return true;
}

template <typename ResultType>
class TaiheAuthorizationCallback : public AuthorizationCallback {
public:
    using BuildResultFunc = std::function<ResultType(const AuthorizationResult&, const std::string&)>;
    using BuildBatchResultFunc = std::function<ResultType(
        const std::vector<AuthorizationResult>&)>;

    TaiheAuthorizationCallback(
        std::shared_ptr<TaiheAcquireAuthorizationContext> &context,
        std::string privilege, BuildResultFunc buildResultFunc, ResultType defaultResult)
        : buildResultFunc_(std::move(buildResultFunc)), defaultResult_(std::move(defaultResult))
    {
        context_ = context;
        privilege_ = std::move(privilege);
    }

    TaiheAuthorizationCallback(
        std::shared_ptr<TaiheAcquireAuthorizationContext> &context,
        BuildBatchResultFunc buildBatchResultFunc, ResultType defaultResult)
        : buildBatchResultFunc_(std::move(buildBatchResultFunc)), defaultResult_(std::move(defaultResult))
    {
        context_ = context;
    }

    ~TaiheAuthorizationCallback() override = default;

    ErrCode OnResult(int32_t errCode, const AuthorizationResult &result) override
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (onResultCalled_) {
            ACCOUNT_LOGE("OnResult has been called.");
            return ERR_OK;
        }
        if (context_ != nullptr && context_->hasOptions && context_->options.hasContext) {
            CloseUIExtension(context_);
        }
        errCode_ = errCode;
        if (context_ != nullptr && context_->isPublicApi && errCode == ERR_OK) {
            auto nativeRC = static_cast<int32_t>(result.resultCode);
            if (nativeRC == static_cast<int32_t>(
                    AccountSA::AuthorizationResultCode::AUTHORIZATION_INTERACTION_NOT_ALLOWED)) {
                errCode_ = ERR_JS_AUTHORIZATION_INTERACTION_NOT_ALLOWED;
            } else if (nativeRC == static_cast<int32_t>(
                    AccountSA::AuthorizationResultCode::AUTHORIZATION_SERVICE_BUSY)) {
                errCode_ = ERR_JS_AUTHORIZATION_SERVICE_BUSY;
            }
        }
        if (errCode_ == ERR_OK) {
            rawResult_ = result;
        }
        onResultCalled_ = true;
        cv_.notify_one();
        return ERR_OK;
    }

    ErrCode OnBatchResult(int32_t resultCode,
        const std::vector<AuthorizationResult> &results) override
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (onResultCalled_) {
            ACCOUNT_LOGE("OnBatchResult has been called.");
            return ERR_OK;
        }
        if (context_ != nullptr && context_->hasOptions && context_->options.hasContext) {
            CloseUIExtension(context_);
        }
        errCode_ = resultCode;
        if (context_ != nullptr && context_->isPublicApi && errCode_ == ERR_OK) {
            if (CheckAllBatchResultsMatch(results,
                    AccountSA::AuthorizationResultCode::AUTHORIZATION_SERVICE_BUSY)) {
                errCode_ = ERR_JS_AUTHORIZATION_SERVICE_BUSY;
            } else if (CheckAllBatchResultsMatch(results,
                    AccountSA::AuthorizationResultCode::AUTHORIZATION_INTERACTION_NOT_ALLOWED)) {
                errCode_ = ERR_JS_AUTHORIZATION_INTERACTION_NOT_ALLOWED;
            }
        }
        if (errCode_ == ERR_OK) {
            rawResults_ = results;
        }
        onResultCalled_ = true;
        cv_.notify_one();
        return ERR_OK;
    }

    ErrCode OnConnectAbility(const ConnectAbilityInfo &info,
        const sptr<IRemoteObject> &callback) override
    {
        ACCOUNT_LOGI("TaiheAuthorizationCallback OnConnectAbility");
        if (context_ == nullptr || !context_->hasOptions || !context_->options.hasContext) {
            SetErrorIfNotFinalized(ERR_JS_SYSTEM_SERVICE_EXCEPTION);
            return ERR_AUTHORIZATION_CREATE_UI_EXTENSION_ERROR;
        }
        ErrCode errCode = CreateUIExtension(context_, info, callback);
        if (errCode == ERR_OK) {
            return ERR_OK;
        }
        auto connectCallback = iface_cast<IConnectAbilityCallback>(callback);
        if (connectCallback == nullptr) {
            SetErrorIfNotFinalized(ERR_JS_SYSTEM_SERVICE_EXCEPTION);
            return ERR_OK;
        }
        std::vector<uint8_t> iamToken;
        NotifyAuthorizationResultWithRetry(callback, errCode, iamToken, -1, 0);
        {
            std::unique_lock<std::mutex> lock(mutex_);
            errCode_ = errCode;
            cv_.notify_one();
        }
        return ERR_OK;
    }

    /**
     * @brief Build taihe result on main thread after cv.wait wakes up.
     * @note Must be called while holding mutex_ (caller is responsible for locking).
     * @return Constructed ResultType, or defaultResult_ on error or empty result.
     */
    ResultType BuildTaiheResult() const
    {
        if (errCode_ != ERR_OK) {
            return defaultResult_;
        }
        if (buildBatchResultFunc_ != nullptr) {
            return buildBatchResultFunc_(rawResults_);
        }
        if (buildResultFunc_ != nullptr) {
            if (!rawResults_.empty()) {
                const std::string &privilege = rawResults_[0].privilege.empty() ? privilege_ : rawResults_[0].privilege;
                return buildResultFunc_(rawResults_[0], privilege);
            }
            if (rawResult_.has_value()) {
                const std::string &privilege = rawResult_->privilege.empty() ? privilege_ : rawResult_->privilege;
                return buildResultFunc_(*rawResult_, privilege);
            }
        }
        return defaultResult_;
    }

    int32_t errCode_ = -1;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool onResultCalled_ = false;
    std::shared_ptr<TaiheAcquireAuthorizationContext> context_;
    std::string privilege_;
    ResultType defaultResult_;
    std::optional<AccountSA::AuthorizationResult> rawResult_;
    std::vector<AccountSA::AuthorizationResult> rawResults_;

private:
    void SetErrorIfNotFinalized(int32_t errCode)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (onResultCalled_) {
            ACCOUNT_LOGI("OnResult already called, skip error errCode=%{public}d", errCode);
            return;
        }
        errCode_ = errCode;
        onResultCalled_ = true;
        cv_.notify_one();
    }

    BuildResultFunc buildResultFunc_;
    BuildBatchResultFunc buildBatchResultFunc_;
};

} // namespace AccountSA
} // namespace OHOS

#endif // TAIHE_AUTHORIZATION_CALLBACK_BASE_H
