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

#ifndef OS_ACCOUNT_INTERFACES_KITS_NAPI_APPACCOUNT_INCLUDE_NAPI_DOMAIN_AUTH_CALLBACK_H
#define OS_ACCOUNT_INTERFACES_KITS_NAPI_APPACCOUNT_INCLUDE_NAPI_DOMAIN_AUTH_CALLBACK_H

#include "domain_account_common.h"
#include "domain_auth_callback.h"
#include "napi_account_common.h"
#include "napi/native_api.h"

namespace OHOS {
namespace AccountJsKit {
class NapiDomainAuthCallback {
public:
    explicit NapiDomainAuthCallback(const std::shared_ptr<AccountSA::DomainAuthCallback> &callback);
    ~NapiDomainAuthCallback();
    std::shared_ptr<AccountSA::DomainAuthCallback> GetDomainAuthCallback();
    static napi_value Init(napi_env env, napi_value exports);

private:
    static napi_value JsOnResult(napi_env env, napi_callback_info cbInfo);
    static napi_value JsConstructor(napi_env env, napi_callback_info cbInfo);
private:
    std::shared_ptr<AccountSA::DomainAuthCallback> callback_;
};

struct CallbackParam : public CommonAsyncContext {
    CallbackParam() {};
    CallbackParam(napi_env napiEnv);
    ~CallbackParam();
    std::shared_ptr<AccountSA::DomainAuthCallback> callback = nullptr;
    AccountSA::DomainAuthResult authResult;
    ThreadLockInfo *lockInfo = nullptr;
};

class NapiDomainAccountCallback final: public AccountSA::DomainAuthCallback {
public:
    NapiDomainAccountCallback(napi_env env, napi_ref callback);
    ~NapiDomainAccountCallback();

    void OnResult(int32_t resultCode, const AccountSA::DomainAuthResult &result) override;
private:
    napi_env env_;
    napi_ref callbackRef_;
    ThreadLockInfo lockInfo_;
};
}  // namespace AccountJsKit
}  // namespace OHOS
#endif  // OS_ACCOUNT_INTERFACES_KITS_NAPI_APPACCOUNT_INCLUDE_NAPI_DOMAIN_AUTH_CALLBACK_H
