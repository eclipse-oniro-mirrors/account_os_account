/*
* Copyright (c) 2025 Huawei Device Co., Ltd.
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#include "taihe_common.h"
#include "account_log_wrapper.h"
namespace OHOS {
namespace AccountSA {

bool IsAccountIdValid(int32_t accountId)
{
    if (accountId < 0) {
        ACCOUNT_LOGI("The account id is invalid");
        return false;
    }
    return true;
}

bool IsSystemApp()
{
    uint64_t tokenId = OHOS::IPCSkeleton::GetSelfTokenID();
    return OHOS::Security::AccessToken::TokenIdKit::IsSystemAppByFullTokenID(tokenId);
}

AccountSA::DomainAccountInfo ConvertToDomainAccountInfoInner(const ohos::account::osAccount::DomainAccountInfo &domainAccountInfo)
{
    AccountSA::DomainAccountInfo domainAccountInfoInner(std::string(domainAccountInfo.domain.c_str()),
                                                        std::string(domainAccountInfo.accountName.c_str()));
    if (domainAccountInfo.accountId.has_value()) {
        domainAccountInfoInner.accountId_ = domainAccountInfo.accountId.value();
    }
    if (domainAccountInfo.serverConfigId.has_value()) {
        domainAccountInfoInner.serverConfigId_ = domainAccountInfo.serverConfigId.value();
    }
    return domainAccountInfoInner;
}
}
}