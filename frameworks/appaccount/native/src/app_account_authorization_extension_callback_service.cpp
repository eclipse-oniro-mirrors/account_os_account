/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#include "app_account_authorization_extension_callback_service.h"
#include "account_log_wrapper.h"

namespace OHOS {
namespace AccountSA {
AppAccountAuthorizationExtensionCallbackService::AppAccountAuthorizationExtensionCallbackService(
    const std::shared_ptr<AppAccountAuthorizationExtensionCallback>& callback)
    : innerCallback_(callback)
{}

AppAccountAuthorizationExtensionCallbackService::~AppAccountAuthorizationExtensionCallbackService()
{}

void AppAccountAuthorizationExtensionCallbackService::OnResult(
    const AsyncCallbackError &businessError, const AAFwk::WantParams &parameters)
{
    if (innerCallback_ == nullptr) {
        ACCOUNT_LOGE("innerCallback is nullptr");
        return;
    }
    return innerCallback_->OnResult(businessError, parameters);
}

void AppAccountAuthorizationExtensionCallbackService::OnRequestRedirected(const AAFwk::Want& request)
{
    if (innerCallback_ == nullptr) {
        ACCOUNT_LOGE("innerCallback is nullptr");
        return;
    }
    return innerCallback_->OnRequestRedirected(request);
}
} // namespace AccountSA
} // namespace OHOS