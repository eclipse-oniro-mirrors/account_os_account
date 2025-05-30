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

#include "os_account_domain_account_callback.h"

#include "account_error_no.h"
#include "account_event_provider.h"
#include "account_log_wrapper.h"
#ifdef HAS_CES_PART
#include "common_event_support.h"
#endif // HAS_CES_PART
#include "iinner_os_account_manager.h"
#include "ios_account_control.h"
#include "os_account_constants.h"
#include "os_account_control_file_manager.h"

namespace OHOS {
namespace AccountSA {
CheckAndCreateDomainAccountCallback::CheckAndCreateDomainAccountCallback(
    std::shared_ptr<IOsAccountControl> &osAccountControl, const OsAccountType &type,
    const sptr<IDomainAccountCallback> &callback, const CreateOsAccountForDomainOptions &accountOptions)
    : type_(type), osAccountControl_(osAccountControl), accountOptions_(accountOptions), innerCallback_(callback)
{}

void CheckAndCreateDomainAccountCallback::OnResult(int32_t errCode, Parcel &parcel)
{
    if (innerCallback_ == nullptr) {
        ACCOUNT_LOGE("InnerPlugin_ is nullptr");
        return;
    }
    OsAccountInfo osAccountInfo;
    Parcel resultParcel;
    osAccountInfo.Marshalling(resultParcel);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("Check domain account failed");
        return innerCallback_->OnResult(errCode, resultParcel);
    }
    std::shared_ptr<AAFwk::WantParams> parameters(AAFwk::WantParams::Unmarshalling(parcel));
    if (parameters == nullptr) {
        ACCOUNT_LOGE("Parameters unmarshalling error");
        return innerCallback_->OnResult(ERR_JS_SYSTEM_SERVICE_EXCEPTION, resultParcel);
    }
    DomainAccountInfo domainAccountInfo;
    domainAccountInfo.accountName_ = parameters->GetStringParam("accountName");
    domainAccountInfo.domain_ = parameters->GetStringParam("domain");
    domainAccountInfo.accountId_ = parameters->GetStringParam("accountId");
    domainAccountInfo.serverConfigId_ = parameters->GetStringParam("serverConfigId");
    if ((domainAccountInfo.accountName_.empty()) || (domainAccountInfo.domain_.empty())) {
        ACCOUNT_LOGE("Domain account not found");
        return innerCallback_->OnResult(ERR_JS_ACCOUNT_NOT_FOUND, resultParcel);
    }
    errCode = IInnerOsAccountManager::GetInstance().BindDomainAccount(type_, domainAccountInfo,
        osAccountInfo, accountOptions_);
    if (errCode != ERR_OK) {
        return innerCallback_->OnResult(errCode, resultParcel);
    }
    auto callbackWrapper =
        std::make_shared<BindDomainAccountCallback>(osAccountControl_, osAccountInfo, innerCallback_);
    if (callbackWrapper == nullptr) {
        ACCOUNT_LOGE("Create BindDomainAccountCallback failed");
        return innerCallback_->OnResult(ERR_ACCOUNT_COMMON_INSUFFICIENT_MEMORY_ERROR, resultParcel);
    }
    errCode = InnerDomainAccountManager::GetInstance().OnAccountBound(domainAccountInfo,
        osAccountInfo.GetLocalId(), callbackWrapper);
    if (errCode != ERR_OK) {
        return innerCallback_->OnResult(errCode, resultParcel);
    }
}

BindDomainAccountCallback::BindDomainAccountCallback(
    std::shared_ptr<IOsAccountControl> &osAccountControl, const OsAccountInfo &osAccountInfo,
    const sptr<IDomainAccountCallback> &callback)
    : osAccountControl_(osAccountControl), osAccountInfo_(osAccountInfo), innerCallback_(callback)
{}

void BindDomainAccountCallback::OnResult(int32_t errCode, Parcel &parcel)
{
    if (innerCallback_ == nullptr) {
        ACCOUNT_LOGE("Inner callback is nullptr");
        return;
    }
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("Failed to bind domain account");
        if (osAccountInfo_.GetLocalId() != Constants::START_USER_ID) {
            (void)osAccountControl_->DelOsAccount(osAccountInfo_.GetLocalId());
        }
        return innerCallback_->OnResult(errCode, parcel);
    }
    Parcel resultParcel;
    if (osAccountInfo_.GetLocalId() != Constants::START_USER_ID) {
        errCode = IInnerOsAccountManager::GetInstance().SendMsgForAccountCreate(osAccountInfo_);
        if (errCode != ERR_OK) {
            DomainAccountInfo curDomainInfo;
            osAccountInfo_.GetDomainInfo(curDomainInfo);
            if (InnerDomainAccountManager::GetInstance().OnAccountUnBound(curDomainInfo, nullptr,
                osAccountInfo_.GetLocalId()) != ERR_OK) {
                ACCOUNT_LOGE("Failed to bind domain account");
                return;
            }
            (void)osAccountControl_->DelOsAccount(osAccountInfo_.GetLocalId());
        }
        osAccountInfo_.Marshalling(resultParcel);
        return innerCallback_->OnResult(errCode, resultParcel);
    }

#ifdef HAS_CES_PART
    AccountEventProvider::EventPublish(
        EventFwk::CommonEventSupport::COMMON_EVENT_USER_INFO_UPDATED, Constants::START_USER_ID, nullptr);
#else  // HAS_CES_PART
    ACCOUNT_LOGI("No common event part! Publish nothing!");
#endif // HAS_CES_PART
    osAccountInfo_.Marshalling(resultParcel);
    innerCallback_->OnResult(errCode, resultParcel);
}
} // namespace AccountSA
} // namespace OHOS