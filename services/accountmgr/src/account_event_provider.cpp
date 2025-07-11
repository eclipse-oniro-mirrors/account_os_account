/*
 * Copyright (c) 2021-2023 Huawei Device Co., Ltd.
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

#include "account_event_provider.h"
#include "account_info.h"
#ifdef HAS_CES_PART
#include <common_event_data.h>
#include <common_event_support.h>
#endif // HAS_CES_PART
#include "account_log_wrapper.h"
#ifdef HAS_CES_PART
#include "common_event_manager.h"
#include "want.h"
#endif // HAS_CES_PART
#include "account_hisysevent_adapter.h"
#include "hitrace_adapter.h"
#include "common_event_constant.h"

#ifdef HAS_CES_PART
using namespace OHOS::EventFwk;
#endif // HAS_CES_PART

namespace OHOS {
namespace AccountSA {
bool AccountEventProvider::EventPublish(const std::string& event, int32_t userId, const DomainAccountEventData *report)
{
#ifdef HAS_CES_PART
    Want want;
    want.SetAction(event);
    CommonEventData data;
    if (event == EventFwk::CommonEventSupport::COMMON_EVENT_USER_INFO_UPDATED) {
        data.SetCode(userId);
    } else if (event == EventFwk::CommonEventSupport::COMMON_EVENT_DOMAIN_ACCOUNT_STATUS_CHANGED) {
        if (report == nullptr) {
            ACCOUNT_LOGE("Report is nullptr");
            return false;
        }
        want.SetParam("domain", report->domainAccountInfo.domain_);
        want.SetParam("accountName", report->domainAccountInfo.accountName_);
        want.SetParam("event", report->event);
        want.SetParam("status", report->status);
    } else {
        want.SetParam("userId", userId);
    }

    data.SetWant(want);
    StartTraceAdapter("Ohos account event publish.");
    /* publish */
    if (!CommonEventManager::PublishCommonEvent(data)) {
        ACCOUNT_LOGE("PublishCommonEvent failed! event %{public}s. userId is %{public}d", event.c_str(), userId);
        REPORT_OHOS_ACCOUNT_FAIL(userId, EVENT_PUBLISH, false,
            "PublishCommonEvent failed, event=" + event + ", userId=" + std::to_string(userId));
        FinishTraceAdapter();
        return false;
    } else {
        ACCOUNT_LOGI("PublishCommonEvent succeed! event %{public}s.", event.c_str());
    }
    FinishTraceAdapter();
#else // HAS_CES_PART
    ACCOUNT_LOGI("No common event part, do not publish anything! event %{public}s.", event.c_str());
#endif // HAS_CES_PART
    return true;
}

bool AccountEventProvider::EventPublishAsUser(const std::string& event, int32_t userId)
{
    if (userId == UNDEFINED_USER) {
        ACCOUNT_LOGE("EventPublishAsUser failed, userId is UNDEFINED_USER");
        return EventPublish(event, userId, nullptr);
    }
#ifdef HAS_CES_PART
    Want want;
    want.SetAction(event);
    CommonEventData data;
    if (event == EventFwk::CommonEventSupport::COMMON_EVENT_USER_INFO_UPDATED) {
        data.SetCode(userId);
    } else {
        want.SetParam("userId", userId);
    }

    data.SetWant(want);
    StartTraceAdapter("Ohos account event publish.");
    /* publish */
    if (!CommonEventManager::PublishCommonEventAsUser(data, userId)) {
        ACCOUNT_LOGE("PublishCommonEventAsUser failed! event %{public}s. userId is %{public}d", event.c_str(), userId);
        REPORT_OHOS_ACCOUNT_FAIL(userId, EVENT_PUBLISH, false,
            "PublishCommonEventAsUser failed, event=" + event + ", userId=" + std::to_string(userId));
        FinishTraceAdapter();
        return false;
    } else {
        ACCOUNT_LOGI("PublishCommonEventAsUser succeed! event %{public}s.", event.c_str());
    }
    FinishTraceAdapter();
#else // HAS_CES_PART
    ACCOUNT_LOGI("No common event part, do not publish anything! event %{public}s.", event.c_str());
#endif // HAS_CES_PART
    return true;
}
} // namespace AccountSA
} // namespace OHOS