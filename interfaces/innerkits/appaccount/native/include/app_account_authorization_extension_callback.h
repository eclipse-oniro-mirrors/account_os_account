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

#ifndef APP_ACCOUNT_INTERFACES_INNERKITS_APPACCOUNT_NATIVE_INCLUDE_APP_ACCOUNT_AUTHENTICATION_EXTENSION_CALLBACK_H
#define APP_ACCOUNT_INTERFACES_INNERKITS_APPACCOUNT_NATIVE_INCLUDE_APP_ACCOUNT_AUTHENTICATION_EXTENSION_CALLBACK_H

#include "app_account_common.h"
#include <cstdint>
#include "want.h"

namespace OHOS {
namespace AccountSA {
class AppAccountAuthorizationExtensionCallback {
public:
    virtual void OnResult(const AsyncCallbackError &businessData, const AAFwk::WantParams &parameters) = 0;
    virtual void OnRequestRedirected(const AAFwk::Want& request) = 0;
};
} // namespace AccountSA
} // namespace OHOS
#endif // APP_ACCOUNT_INTERFACES_INNERKITS_APPACCOUNT_NATIVE_INCLUDE_APP_ACCOUNT_AUTHENTICATION_EXTENSION_CALLBACK_H
