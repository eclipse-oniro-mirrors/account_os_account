/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
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
#include "os_account_stop_user_callback.h"
#include "account_error_no.h"
#include "account_log_wrapper.h"
#include "os_account_manager.h"

namespace OHOS {
namespace AccountSA {
void OsAccountStopUserCallback::OnStopUserDone(int userId, int errcode)
{
    ACCOUNT_LOGE("in call back account, id is %{public}d", userId);
    ACCOUNT_LOGE("in call back account, errcode is %{public}d", errcode);
    isCallBackOk_ = true;
    if (errcode == 0) {
        isReturnOk_ = true;
    }
}
}  // namespace AccountSA
}  // namespace OHOS
