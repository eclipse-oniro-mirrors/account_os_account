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

#include "status_listener_death_recipient.h"

#include "account_log_wrapper.h"
#include "status_listener_manager.h"

namespace OHOS {
namespace AccountSA {

void StatusListenerDeathRecipient::OnRemoteDied(const wptr<IRemoteObject>& remote)
{
    ACCOUNT_LOGI("OnRemoteDied enter.");
    if (remote == nullptr) {
        ACCOUNT_LOGE("remote is nullptr.");
        return;
    }

    sptr<IRemoteObject> object = remote.promote();
    if (object == nullptr) {
        ACCOUNT_LOGE("object is nullptr.");
        return;
    }
    StatusListenerManager::GetInstance().RemoveListenerByObject(object);
    ACCOUNT_LOGI("end");
}
} // namespace AccessToken
} // namespace OHOS
