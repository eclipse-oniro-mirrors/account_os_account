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

#include "checkoauthtokenvisibility_fuzzer.h"

#include "account_log_wrapper.h"
#include "fuzz_data.h"
#include "app_account_manager.h"
#include <string>
#include <vector>

using namespace std;
using namespace OHOS::AccountSA;

namespace OHOS {
    bool CheckOAuthTokenVisibilityFuzzTest(const uint8_t* data, size_t size)
    {
        bool result = false;
        if (size > 0) {
            FuzzData fuzzData(data, size);
            std::string testName(fuzzData.GenerateRandomString());
            std::string testAuthType(fuzzData.GenerateRandomString());
            std::string testBundleName(fuzzData.GenerateRandomString());
            bool isVisible = false;
            result = AppAccountManager::CheckOAuthTokenVisibility(testName, testAuthType, testBundleName, isVisible);
        }
        return result;
    }
}

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    /* Run your code on data */
    OHOS::CheckOAuthTokenVisibilityFuzzTest(data, size);
    return 0;
}

