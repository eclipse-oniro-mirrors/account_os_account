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

#include "subscribeosaccountstub_fuzzer.h"
#include <string>
#include <thread>
#include <vector>

#include "fuzz_data.h"
#include "ios_account.h"
#include "os_account_manager_service.h"

using namespace std;
using namespace OHOS::AccountSA;

class TestOsAccountSubscriber : public OsAccountSubscriber {
public:
    void OnAccountsChanged(const int& id) {}
};

namespace OHOS {
const int CONSTANTS_NUMBER_TEN = 10;
const std::u16string IOS_ACCOUNT_DESCRIPTOR = u"ohos.accountfwk.IOsAccount";
bool SubscribeOsAccountStubFuzzTest(const uint8_t *data, size_t size)
{
    if ((data == nullptr) || (size == 0)) {
        return false;
    }

    MessageParcel datas;
    datas.WriteInterfaceToken(IOS_ACCOUNT_DESCRIPTOR);
    FuzzData fuzzData(data, size);
    OsAccountSubscribeInfo subscribeInfo;
    subscribeInfo.SetName(fuzzData.GenerateString());
    OS_ACCOUNT_SUBSCRIBE_TYPE testType = static_cast<OS_ACCOUNT_SUBSCRIBE_TYPE>(
        fuzzData.GetData<int>() % CONSTANTS_NUMBER_TEN);
    subscribeInfo.SetOsAccountSubscribeType(testType);

    if (!datas.WriteParcelable(&subscribeInfo)) {
        return false;
    }

    std::shared_ptr<OsAccountSubscriber> subscriber = make_shared<TestOsAccountSubscriber>();

    sptr<OsAccountEventListener> listener = new (std::nothrow) OsAccountEventListener(subscriber);
    if (listener == nullptr) {
        return false;
    }
    sptr<IRemoteObject> osAccountEventListener = listener->AsObject();

    if (!datas.WriteRemoteObject(osAccountEventListener)) {
        return false;
    }

    MessageParcel reply;
    MessageOption option;

    auto osAccountManagerService_ = std::make_shared<OsAccountManagerService>();

    osAccountManagerService_ ->OnRemoteRequest(
        static_cast<int32_t>(OsAccountInterfaceCode::SUBSCRIBE_OS_ACCOUNT), datas, reply, option);

    return true;
}
} // namespace OHOS

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    /* Run your code on data */
    OHOS::SubscribeOsAccountStubFuzzTest(data, size);
    return 0;
}
