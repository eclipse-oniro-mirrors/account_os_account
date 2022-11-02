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

#include "opensession_fuzzer.h"

#include <string>
#include <vector>
#include "account_iam_client.h"


using namespace std;
using namespace OHOS::AccountSA;

namespace OHOS {
    bool OpenSessionFuzzTest(const uint8_t* data, size_t size)
    {
        int32_t result = ERR_OK;
        if (size > 0) {
            int32_t userId = static_cast<int32_t>(size);
            std::vector<uint8_t> challenge;
            result = AccountIAMClient::GetInstance().OpenSession(userId, challenge);
        }
        return result == ERR_OK;
    }
}

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    /* Run your code on data */
    OHOS::OpenSessionFuzzTest(data, size);
    return 0;
}

