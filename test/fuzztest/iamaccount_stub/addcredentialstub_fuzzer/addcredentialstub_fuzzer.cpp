/*
 * Copyright (c) 2023-2024 Huawei Device Co., Ltd.
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

#include "addcredentialstub_fuzzer.h"

#include <string>
#include <vector>
#include "account_iam_callback_service.h"
#include "account_iam_client.h"
#include "account_iam_service.h"
#include "account_log_wrapper.h"
#include "fuzz_data.h"
#include "iaccount_iam.h"

using namespace std;
using namespace OHOS::AccountSA;
namespace OHOS {
const std::u16string IAMACCOUNT_TOKEN = u"ohos.accountfwk.IAccountIAM";

class MockIDMCallback : public OHOS::AccountSA::IDMCallback {
public:
    virtual ~MockIDMCallback()
    {
    }
    void OnAcquireInfo(int32_t module, uint32_t acquireInfo, const Attributes &extraInfo) override
    {
        return;
    }
    void OnResult(int32_t result, const Attributes &extraInfo) override
    {
        return;
    }
};

bool AddCredentialStubFuzzTest(const uint8_t *data, size_t size)
{
    if ((data == nullptr) || (size == 0)) {
        return false;
    }
    FuzzData fuzzData(data, size);
    int32_t userId = fuzzData.GetData<int32_t>();
    AuthType authType = static_cast<AuthType>(fuzzData.GenerateRandomEnmu(IAMAuthType::TYPE_END));
    std::optional<PinSubType> pinType = {fuzzData.GenerateRandomEnmu(PinSubType::PIN_MAX)};
    std::vector<uint8_t> token = {fuzzData.GetData<uint8_t>()};
    std::shared_ptr<IDMCallback> ptr = make_shared<MockIDMCallback>();
    sptr<IIDMCallback> callback = new (std::nothrow) IDMCallbackService(userId, ptr);

    MessageParcel dataTemp;
    if (!dataTemp.WriteInterfaceToken(IAMACCOUNT_TOKEN)) {
        return false;
    }
    if (!dataTemp.WriteInt32(userId)) {
        return false;
    }
    if (!dataTemp.WriteInt32(authType)) {
        return false;
    }
    PinSubType pin = pinType.value_or(PinSubType::PIN_MAX);
    if (!dataTemp.WriteInt32(pin)) {
        return false;
    }
    if (!dataTemp.WriteUInt8Vector(token)) {
        return false;
    }
    if (!dataTemp.WriteRemoteObject(callback->AsObject())) {
        return false;
    }
    MessageParcel reply;
    MessageOption option;
    uint32_t code = static_cast<uint32_t>(AccountIAMInterfaceCode::ADD_CREDENTIAL);
    auto iamAccountManagerService = std::make_shared<AccountIAMService>();
    iamAccountManagerService->OnRemoteRequest(code, dataTemp, reply, option);

    return true;
}
} // namespace OHOS

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    /* Run your code on data */
    OHOS::AddCredentialStubFuzzTest(data, size);
    return 0;
}
