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

#ifndef OS_ACCOUNT_SERVICES_ACCOUNTMGR_INCLUDE_APPACCOUNT_APP_ACCOUNT_AUTHENTICATION_EXTENSION_SERVICE_H
#define OS_ACCOUNT_SERVICES_ACCOUNTMGR_INCLUDE_APPACCOUNT_APP_ACCOUNT_AUTHENTICATION_EXTENSION_SERVICE_H

#include <string>
#include "account_error_no.h"
#include "app_account_common.h"
#include "app_account_authentication_extension_callback_client.h"
#include "app_account_authentication_extension_stub.h"
#include "napi_app_account_authentication_extension.h"
#include "want.h"

namespace OHOS {
namespace AccountSA {
class AppAccountAuthenticationExtensionService : public AppAccountAuthenticationExtensionStub {
public:
    explicit AppAccountAuthenticationExtensionService(
        const std::shared_ptr<AccountJsKit::JsAuthenticationExtension> &extension);
    ~AppAccountAuthenticationExtensionService() override;
    ErrCode StartAuthentication(const AuthenticationRequest &request) override;

private:
    ErrCode CheckAndInitExecEnv(const sptr<IAppAccountAuthenticationExtensionCallback> &callback,
        AppAccountAuthenticationExtensionCallbackClient **callbackClient);
    std::shared_ptr<AccountJsKit::JsAuthenticationExtension> innerExtension_;
    DISALLOW_COPY_AND_MOVE(AppAccountAuthenticationExtensionService);
};
}  // namespace AccountSA
}  // namespace OHOS
#endif  // OS_ACCOUNT_SERVICES_ACCOUNTMGR_INCLUDE_APPACCOUNT_APP_ACCOUNT_AUTHENTICATION_EXTENSION_SERVICE_H
