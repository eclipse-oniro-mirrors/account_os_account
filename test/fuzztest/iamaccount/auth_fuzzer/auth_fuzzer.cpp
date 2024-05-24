/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#include "auth_fuzzer.h"

#include <string>
#include <vector>
#include "account_iam_client.h"


using namespace std;
using namespace OHOS::AccountSA;

class MockIDMCallback : public OHOS::AccountSA::IDMCallback {
public:
    virtual ~MockIDMCallback() {}
    void OnAcquireInfo(int32_t module, uint32_t acquireInfo, const Attributes &extraInfo) override
    {
        return;
    }
    void OnResult(int32_t result, const Attributes &extraInfo) override
    {
        return;
    }
};

namespace OHOS {
    bool AuthFuzzTest(const uint8_t* data, size_t size)
    {
        std::vector<uint8_t> challenge = {static_cast<uint8_t>(size)};
        AuthType authType = static_cast<AuthType>(size);
        AuthTrustLevel authTrustLevel = static_cast<AuthTrustLevel>(size);
        std::shared_ptr<IDMCallback> callback = make_shared<MockIDMCallback>();
        AuthOptions authOptions;
        uint64_t result = AccountIAMClient::GetInstance().Auth(authOptions,
            challenge, authType, authTrustLevel, callback);
        return result == ERR_OK;
    }
}

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    /* Run your code on data */
    OHOS::AuthFuzzTest(data, size);
    return 0;
}

