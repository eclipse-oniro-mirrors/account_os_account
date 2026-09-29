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
#include "account_info_report.h"
#include "account_log_wrapper.h"
#include "iinner_os_account_manager.h"
#include "json_utils.h"
#ifdef SECURITY_GUARDE_ENABLE
#include "sg_collect_client.h"
#include "time_service_client.h"
#endif

namespace OHOS {
namespace AccountSA {
namespace {
#ifdef SECURITY_GUARDE_ENABLE
static constexpr int NANO_TO_MILLI = 1000000LL;
#endif
}
std::string TransformIntoJson(const std::string &user, int32_t id, ReportEvent event, int32_t result)
{
    auto jsonResult = CreateJson();
#ifdef SECURITY_GUARDE_ENABLE
    if (jsonResult == nullptr) {
        return "";
    }
    bool success = true;
    success &= AddIntToJson(jsonResult, "type", 0);
    success &= AddIntToJson(jsonResult, "subType", static_cast<int32_t>(event));
    auto userJson = CreateJson();
    if (userJson == nullptr) {
        return "";
    }
    success &= AddStringToJson(userJson, "userName", user);
    success &= AddIntToJson(userJson, "userId", id);

    success &= AddObjToJson(jsonResult, "caller", userJson);
    success &= AddStringToJson(jsonResult, "bootTime",
        std::to_string(MiscServices::TimeServiceClient::GetInstance()->GetBootTimeNs()));
    success &= AddStringToJson(jsonResult, "wallTime",
        std::to_string(MiscServices::TimeServiceClient::GetInstance()->GetWallTimeNs()));
    success &= AddStringToJson(jsonResult, "outcome", (result == 0) ? "Success" : "Fail");
    success &= AddStringToJson(jsonResult, "sourceInfo", "");
    success &= AddStringToJson(jsonResult, "targetInfo", "");
    success &= AddStringToJson(jsonResult, "extra", "");
    if (!success) {
        return "";
    }
#endif
    return PackJsonToString(jsonResult);
}

void AccountInfoReport::ReportSecurityInfo(const std::string &user, int32_t id, ReportEvent event, int32_t result)
{
#ifdef SECURITY_GUARDE_ENABLE
    using namespace Security::SecurityGuard;
    std::string userName = user;
    if (user.empty()) {
        OsAccountInfo osAccountInfo;
        (void)IInnerOsAccountManager::GetInstance().GetRealOsAccountInfoById(id, osAccountInfo);
        userName = osAccountInfo.GetLocalName();
    }
    int64_t eventId = 1011015001; // 1011015001: report event id
    std::string content = TransformIntoJson(userName, id, event, result);
    std::shared_ptr<EventInfo> eventInfo = std::make_shared<EventInfo>(eventId, "1.0", content);
    NativeDataCollectKit::ReportSecurityInfoAsync(eventInfo);
#endif
}

std::string TransformIntoAccountOperationJson(
    const AccountOperationInfo &accountOperationInfo, AccountOperationType operationType)
{
#ifdef SECURITY_GUARDE_ENABLE
    auto jsonResult = CreateJson();
    if (jsonResult == nullptr) {
        return "";
    }
    bool success = true;
    success &= AddIntToJson(jsonResult, "operationType", static_cast<int32_t>(operationType));
    auto sourceJson = CreateJson();
    if (sourceJson == nullptr) {
        return "";
    }
    success &= AddIntToJson(sourceJson, "sourcePid", accountOperationInfo.pid);
    success &= AddIntToJson(sourceJson, "sourceUid", accountOperationInfo.uid);
    success &= AddStringToJson(sourceJson, "sourceUserName", accountOperationInfo.sourceUserName);
    success &= AddIntToJson(sourceJson, "sourceUserId", accountOperationInfo.sourceUserId);

    auto targetJson = CreateJson();
    if (targetJson == nullptr) {
        return "";
    }
    success &= AddStringToJson(targetJson, "targetUserName", accountOperationInfo.targetUserName);
    success &= AddIntToJson(targetJson, "targetUserId", accountOperationInfo.targetUserId);

    success &= AddObjToJson(jsonResult, "source", sourceJson);
    success &= AddObjToJson(jsonResult, "target", targetJson);
    int64_t bootTime = MiscServices::TimeServiceClient::GetInstance()->GetBootTimeNs();
    int64_t wallTimeNS = MiscServices::TimeServiceClient::GetInstance()->GetWallTimeNs();
    int64_t happenTime = wallTimeNS / NANO_TO_MILLI;
    int64_t happenTimeNS = wallTimeNS % NANO_TO_MILLI;
    success &= AddStringToJson(jsonResult, "bootTime", std::to_string(bootTime));
    success &= AddDoubleToJson(jsonResult, "happenTime", static_cast<double>(happenTime));
    success &= AddDoubleToJson(jsonResult, "happenTimeNS", static_cast<double>(happenTimeNS));
    if (!success) {
        return "";
    }
    return PackJsonToString(jsonResult);
#else
    return "";
#endif
}

void AccountInfoReport::ReportAccountOperation(
    const AccountOperationInfo &accountOperationInfo, AccountOperationType operationType)
{
#ifdef SECURITY_GUARDE_ENABLE
    using namespace Security::SecurityGuard;
    int64_t eventId = 0x010000103;
    std::string content = TransformIntoAccountOperationJson(accountOperationInfo, operationType);
    std::shared_ptr<EventInfo> eventInfo = std::make_shared<EventInfo>(eventId, "1.0", content);
    NativeDataCollectKit::ReportSecurityInfoAsync(eventInfo);
#endif
}
} // namespace AccountSA
} // namespace OHOS
