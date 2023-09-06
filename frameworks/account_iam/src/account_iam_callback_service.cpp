/*
 * Copyright (c) 2022-2023 Huawei Device Co., Ltd.
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

#include "account_iam_callback_service.h"

#include "account_error_no.h"
#include "account_iam_client.h"
#include "account_log_wrapper.h"
#include "domain_account_client.h"

namespace OHOS {
namespace AccountSA {
IDMCallbackService::IDMCallbackService(int32_t userId, const std::shared_ptr<IDMCallback> &callback)
    : userId_(userId), callback_(callback)
{}

void IDMCallbackService::OnAcquireInfo(int32_t module, uint32_t acquireInfo, const Attributes &extraInfo)
{
    if (callback_ == nullptr) {
        ACCOUNT_LOGE("callback is nullptr");
        return;
    }
    callback_->OnAcquireInfo(module, acquireInfo, extraInfo);
}

void IDMCallbackService::OnResult(int32_t result, const Attributes &extraInfo)
{
    if (callback_ == nullptr) {
        ACCOUNT_LOGE("callback is nullptr");
        return;
    }
    callback_->OnResult(result, extraInfo);
    AccountIAMClient::GetInstance().ClearCredential(userId_);
}

GetCredInfoCallbackService::GetCredInfoCallbackService(const std::shared_ptr<GetCredInfoCallback> &callback)
    : callback_(callback)
{}

void GetCredInfoCallbackService::OnCredentialInfo(const std::vector<CredentialInfo> &infoList)
{
    if (callback_ == nullptr) {
        ACCOUNT_LOGE("callback is nullptr");
        return;
    }
    callback_->OnCredentialInfo(ERR_OK, infoList);
}

GetSetPropCallbackService::GetSetPropCallbackService(const std::shared_ptr<GetSetPropCallback> &callback)
    : callback_(callback)
{}

void GetSetPropCallbackService::OnResult(int32_t result, const Attributes &extraInfo)
{
    if (callback_ == nullptr) {
        ACCOUNT_LOGE("callback is nullptr");
        return;
    }
    callback_->OnResult(result, extraInfo);
}

DomainAuthCallbackAdapter::DomainAuthCallbackAdapter(
    const std::shared_ptr<IDMCallback> &callback) : callback_(callback)
{}

void DomainAuthCallbackAdapter::OnResult(const int32_t errCode, Parcel &parcel)
{
    if (callback_ == nullptr) {
        ACCOUNT_LOGE("callback is nullptr");
        return;
    }
    std::shared_ptr<DomainAuthResult> authResult(DomainAuthResult::Unmarshalling(parcel));
    if (authResult == nullptr) {
        ACCOUNT_LOGE("authResult is nullptr");
        return;
    }
    Attributes attr;
    attr.SetUint8ArrayValue(Attributes::AttributeKey::ATTR_SIGNATURE, (*authResult).token);
    attr.SetInt32Value(Attributes::AttributeKey::ATTR_REMAIN_TIMES, (*authResult).authStatusInfo.remainingTimes);
    attr.SetInt32Value(Attributes::AttributeKey::ATTR_FREEZING_TIME, (*authResult).authStatusInfo.freezingTime);
    callback_->OnResult(errCode, attr);
}

DomainCredentialRecipient::DomainCredentialRecipient(int32_t userId, const std::shared_ptr<IDMCallback> &callback)
    : userId_(userId), idmCallback_(callback)
{}

DomainCredentialRecipient::~DomainCredentialRecipient()
{}

void DomainCredentialRecipient::OnSetData(int32_t authSubType, std::vector<uint8_t> data)
{
    auto callback = std::make_shared<DomainAuthCallbackAdapter>(idmCallback_);
    if (callback == nullptr) {
        ACCOUNT_LOGE("fail to create DomainAuthCallbackAdapter");
        if (idmCallback_ != nullptr) {
            ACCOUNT_LOGE("idmCallback is nullptr");
            Attributes attr;
            idmCallback_->OnResult(ERR_ACCOUNT_COMMON_INSUFFICIENT_MEMORY_ERROR, attr);
        }
        return;
    }
    ErrCode errCode = DomainAccountClient::GetInstance().AuthUser(userId_, data, callback);
    if (errCode != ERR_OK) {
        Parcel emptyParcel;
        AccountSA::DomainAuthResult emptyResult;
        if (!emptyResult.Marshalling(emptyParcel)) {
            ACCOUNT_LOGE("authResult Marshalling failed");
            return;
        }
        callback->OnResult(errCode, emptyParcel);
    }
}

IAMInputerData::IAMInputerData(int32_t userId, const std::shared_ptr<IInputerData> &inputerData)
    : userId_(userId), innerInputerData_(inputerData)
{}

IAMInputerData::~IAMInputerData()
{}

void IAMInputerData::OnSetData(int32_t authSubType, std::vector<uint8_t> data)
{
    if (innerInputerData_ == nullptr) {
        ACCOUNT_LOGE("innerInputerData_ is nullptr");
        return;
    }
    innerInputerData_->OnSetData(authSubType, data);
    innerInputerData_ = nullptr;
    AccountIAMClient::GetInstance().SetCredential(userId_, data);
}

IAMInputer::IAMInputer(int32_t userId, const std::shared_ptr<IInputer> &inputer)
    : userId_(userId), innerInputer_(inputer)
{}

IAMInputer::~IAMInputer()
{}

void IAMInputer::OnGetData(int32_t authSubType, std::shared_ptr<IInputerData> inputerData)
{
    if (inputerData == nullptr) {
        ACCOUNT_LOGE("inputerData is nullptr");
        return;
    }
    IAMState state = AccountIAMClient::GetInstance().GetAccountState(userId_);
    if (authSubType == 0) {
        authSubType = AccountIAMClient::GetInstance().GetAuthSubType(userId_);
    }
    if (state >= AFTER_ADD_CRED) {
        CredentialItem credItem;
        AccountIAMClient::GetInstance().GetCredential(userId_, credItem);
        inputerData->OnSetData(authSubType, credItem.credential);
    }
    if (innerInputer_ == nullptr) {
        ACCOUNT_LOGE("innerInputer_ is nullptr");
        return;
    }
    auto iamInputerData = std::make_shared<IAMInputerData>(userId_, inputerData);
    innerInputer_->OnGetData(authSubType, iamInputerData);
}

void IAMInputer::ResetInnerInputer(const std::shared_ptr<IInputer> &inputer)
{
    innerInputer_ = inputer;
}
}  // namespace AccountSA
}  // namespace OHOS
