/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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
#include "acquireauthorizationforpublic_fuzzer.h"
#include <cstddef>
#include <memory>
#include <string>
#include <vector>
#include "authorization_callback.h"
#include "authorization_client.h"
#include "authorization_common.h"
#include "fuzz_data.h"

using namespace std;
using namespace OHOS::AccountSA;

class MockAuthCallback : public AuthorizationCallback {
public:
    MockAuthCallback() = default;
    ~MockAuthCallback() override = default;
    OHOS::ErrCode OnResult(int32_t resultCode, const AuthorizationResult& result) override
    {
        return OHOS::ERR_OK;
    }
    OHOS::ErrCode OnBatchResult(int32_t resultCode, const std::vector<AuthorizationResult>& results) override
    {
        return OHOS::ERR_OK;
    }
    OHOS::ErrCode OnConnectAbility(const ConnectAbilityInfo& info,
        const OHOS::sptr<OHOS::IRemoteObject>& callback) override
    {
        return OHOS::ERR_OK;
    }
};

namespace OHOS {
bool AcquireAuthorizationForPublicFuzzTest(const uint8_t* data, size_t size)
{
    if ((data == nullptr) || (size == 0)) {
        return false;
    }
    FuzzData fuzzData(data, size);
    bool isContextValid = fuzzData.GenerateBool();

    std::vector<std::string> privileges;
    uint32_t count = fuzzData.GetData<uint32_t>() %
        (static_cast<uint32_t>(MAX_BATCH_PRIVILEGE_COUNT) + 1);
    for (uint32_t i = 0; i < count; ++i) {
        privileges.push_back(fuzzData.GenerateString());
    }

    uint32_t callbackType = fuzzData.GetData<uint32_t>() % 2;
    if (callbackType == 1) {
        auto callback = std::make_shared<MockAuthCallback>();
        AuthorizationClient::GetInstance().AcquireAuthorizationForPublic(
            privileges, isContextValid, callback);
    } else {
        AuthorizationClient::GetInstance().AcquireAuthorizationForPublic(
            privileges, isContextValid, nullptr);
    }
    return true;
}
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    OHOS::AcquireAuthorizationForPublicFuzzTest(data, size);
    return 0;
}
