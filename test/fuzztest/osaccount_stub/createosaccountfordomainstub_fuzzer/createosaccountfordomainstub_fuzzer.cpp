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

#include "createosaccountfordomainstub_fuzzer.h"
#include <string>
#include <thread>
#include <vector>

#include "domain_account_callback_service.h"
#include "ios_account.h"
#include "os_account_manager_service.h"

using namespace std;
using namespace OHOS::AccountSA;

namespace OHOS {
const int CONSTANTS_NUMBER_FIVE = 5;
const std::u16string IOS_ACCOUNT_DESCRIPTOR = u"ohos.accountfwk.IOsAccount";
bool CreateOsAccountForDomainStubFuzzTest(const uint8_t *data, size_t size)
{
    if ((data == nullptr) || (size == 0)) {
        return false;
    }

    std::string accountName(reinterpret_cast<const char*>(data), size);
    std::string domain(reinterpret_cast<const char*>(data), size);
    DomainAccountInfo domainInfo(accountName, domain);
    OsAccountType testType = static_cast<OsAccountType>(size % CONSTANTS_NUMBER_FIVE);
    MessageParcel datas;
    datas.WriteInterfaceToken(IOS_ACCOUNT_DESCRIPTOR);

    if (!datas.WriteInt32(testType)) {
        return false;
    }

    if (!datas.WriteParcelable(&domainInfo)) {
        return false;
    }

    sptr<DomainAccountCallbackService> callbackService = new (std::nothrow) DomainAccountCallbackService(nullptr);
    if ((callbackService == nullptr) || (!datas.WriteRemoteObject(callbackService->AsObject()))) {
        return false;
    }

    uint32_t code = static_cast<uint32_t>(IOsAccount::Message::CREATE_OS_ACCOUNT_FOR_DOMAIN);

    MessageParcel reply;
    MessageOption option;

    auto osAccountManagerService_ = std::make_shared<OsAccountManagerService>();

    osAccountManagerService_ ->OnRemoteRequest(
        code, datas, reply, option);

    return true;
}
} // namespace OHOS

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    /* Run your code on data */
    OHOS::CreateOsAccountForDomainStubFuzzTest(data, size);
    return 0;
}