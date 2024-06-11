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

#include "setproperty_fuzzer.h"

#include <string>
#include <vector>
#include "account_iam_client.h"
#include "fuzz_data.h"


using namespace std;
using namespace OHOS::AccountSA;

class MockGetSetPropCallback : public OHOS::AccountSA::GetSetPropCallback {
public:
    virtual ~MockGetSetPropCallback() {}
    void OnResult(int32_t result, const Attributes &extraInfo) override
    {
        return;
    }
};

namespace OHOS {
    bool SetPropertyFuzzTest(const uint8_t* data, size_t size)
    {
        FuzzData fuzzData(data, size);
        int32_t userId = fuzzData.GetData<int32_t>();
        std::vector<uint8_t> attr = {fuzzData.GetData<uint8_t>()};
        SetPropertyRequest request = {
            .authType = fuzzData.GenerateRandomEnmu(UserIam::UserAuth::RECOVERY_KEY),
            .mode = fuzzData.GenerateRandomEnmu(UserIam::UserAuth::PROPERTY_MODE_NOTIFY_COLLECTOR_READY),
            .attrs = Attributes(attr),
        };
        std::shared_ptr<GetSetPropCallback> callback = make_shared<MockGetSetPropCallback>();
        AccountIAMClient::GetInstance().SetProperty(userId, request, callback);
        return false;
    }
}

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    /* Run your code on data */
    OHOS::SetPropertyFuzzTest(data, size);
    return 0;
}

