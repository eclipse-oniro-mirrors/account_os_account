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

#include "account_iam_mgr_stub.h"

#include "access_token.h"
#include "account_iam_callback_proxy.h"
#include "account_log_wrapper.h"
#include "iaccount_iam_callback.h"
#include "ipc_skeleton.h"
#include "token_setproc.h"

namespace OHOS {
namespace AccountSA {
AccountIAMMgrStub::AccountIAMMgrStub()
{}

AccountIAMMgrStub::~AccountIAMMgrStub()
{}

const std::map<uint32_t, AccountIAMMgrStub::MessageProcFunction> AccountIAMMgrStub::messageProcMap_ = {
    {
        static_cast<uint32_t>(IAccountIAM::Message::OPEN_SESSION),
        &AccountIAMMgrStub::ProcOpenSession
    },
    {
        static_cast<uint32_t>(IAccountIAM::Message::CLOSE_SESSION),
        &AccountIAMMgrStub::ProcCloseSession
    },
    {
        static_cast<uint32_t>(IAccountIAM::Message::ADD_CREDENTIAL),
        &AccountIAMMgrStub::ProcAddCredential
    },
    {
        static_cast<uint32_t>(IAccountIAM::Message::UPDATE_CREDENTIAL),
        &AccountIAMMgrStub::ProcUpdateCredential
    },
    {
        static_cast<uint32_t>(IAccountIAM::Message::DEL_CRED),
        &AccountIAMMgrStub::ProcDelCred
    },
    {
        static_cast<uint32_t>(IAccountIAM::Message::DEL_USER),
        &AccountIAMMgrStub::ProcDelUser
    },
    {
        static_cast<uint32_t>(IAccountIAM::Message::CANCEL),
        &AccountIAMMgrStub::ProcCancel
    },
    {
        static_cast<uint32_t>(IAccountIAM::Message::GET_CREDENTIAL_INFO),
        &AccountIAMMgrStub::ProcGetCredentialInfo
    },
    {
        static_cast<uint32_t>(IAccountIAM::Message::AUTH_USER),
        &AccountIAMMgrStub::ProcAuthUser
    },
    {
        static_cast<uint32_t>(IAccountIAM::Message::CANCEL_AUTH),
        &AccountIAMMgrStub::ProcCancelAuth
    },
    {
        static_cast<uint32_t>(IAccountIAM::Message::GET_AVAILABLE_STATUS),
        &AccountIAMMgrStub::ProcGetAvailableStatus
    },
    {
        static_cast<uint32_t>(IAccountIAM::Message::GET_PROPERTY),
        &AccountIAMMgrStub::ProcGetProperty
    },
    {
        static_cast<uint32_t>(IAccountIAM::Message::SET_PROPERTY),
        &AccountIAMMgrStub::ProcSetProperty
    },
    {
        static_cast<uint32_t>(IAccountIAM::Message::REGISTER_INPUTER),
        &AccountIAMMgrStub::ProcRegisterInputer
    },
    {
        static_cast<uint32_t>(IAccountIAM::Message::UNREGISTER_INPUTER),
        &AccountIAMMgrStub::ProcUnRegisterInputer
    }
};

std::int32_t AccountIAMMgrStub::OnRemoteRequest(
    std::uint32_t code, MessageParcel &data, MessageParcel &reply, MessageOption &option)
{
    ACCOUNT_LOGD("Received stub message: %{public}d, callingPid: %{public}d", code, IPCSkeleton::GetCallingPid());
    Security::AccessToken::AccessTokenID tokenCaller = IPCSkeleton::GetCallingTokenID();
    int result = SetFirstCallerTokenID(tokenCaller);
    ACCOUNT_LOGD("SetFirstCallerTokenID result: %{public}d", result);
    if (data.ReadInterfaceToken() != GetDescriptor()) {
        ACCOUNT_LOGD("check descriptor failed! code %{public}u.", code);
        return ERR_ACCOUNT_COMMON_CHECK_DESCRIPTOR_ERROR;
    }
    const auto &itFunc = messageProcMap_.find(code);
    if (itFunc != messageProcMap_.end()) {
        return (this->*(itFunc->second))(data, reply);
    }
    ACCOUNT_LOGW("remote request unhandled: %{public}d", code);
    return IPCObjectStub::OnRemoteRequest(code, data, reply, option);
}

ErrCode AccountIAMMgrStub::ProcOpenSession(MessageParcel &data, MessageParcel &reply)
{
    int32_t userId;
    if (!data.ReadInt32(userId)) {
        ACCOUNT_LOGD("failed to read userId");
        return ERR_ACCOUNT_IAM_SERVICE_READ_PARCEL_FAIL;
    }
    std::vector<uint8_t> challenge;
    OpenSession(userId, challenge);
    return reply.WriteUInt8Vector(challenge);
}

ErrCode AccountIAMMgrStub::ProcCloseSession(MessageParcel &data, MessageParcel &reply)
{
    int32_t userId;
    if (!data.ReadInt32(userId)) {
        ACCOUNT_LOGD("failed to read userId");
        return ERR_ACCOUNT_IAM_SERVICE_READ_PARCEL_FAIL;
    }
    CloseSession(userId);
    return ERR_NONE;
}

ErrCode AccountIAMMgrStub::ReadUserIdAndAuthType(MessageParcel &data, int32_t &userId, int32_t &authType)
{
    if (!data.ReadInt32(userId)) {
        ACCOUNT_LOGD("failed to read userId");
        return ERR_ACCOUNT_IAM_SERVICE_READ_PARCEL_FAIL;
    }
    if (!data.ReadInt32(authType)) {
        ACCOUNT_LOGD("failed to read authType");
        return ERR_ACCOUNT_IAM_SERVICE_READ_PARCEL_FAIL;
    }
    return ERR_OK;
}

ErrCode AccountIAMMgrStub::AddOrUpdateCredential(MessageParcel &data, MessageParcel &reply, bool isAdd)
{
    ACCOUNT_LOGD("isAddCredentail: %{public}d", isAdd);
    int32_t userId;
    int32_t authType;
    ErrCode ret = ReadUserIdAndAuthType(data, userId, authType);
    if (ret != ERR_OK) {
        return ret;
    }
    int32_t authSubType;
    if (!data.ReadInt32(authSubType)) {
        ACCOUNT_LOGD("failed to read authSubType");
        return ERR_ACCOUNT_IAM_SERVICE_READ_PARCEL_FAIL;
    }
    CredentialParameters credParams;
    if (!data.ReadUInt8Vector(&credParams.token)) {
        ACCOUNT_LOGD("failed to read token");
        return ERR_ACCOUNT_IAM_SERVICE_READ_PARCEL_FAIL;
    }
    sptr<IIDMCallback> callback = iface_cast<IIDMCallback>(data.ReadRemoteObject());
    if (callback == nullptr) {
        ACCOUNT_LOGD("callback is nullptr");
        return ERR_ACCOUNT_IAM_SERVICE_READ_PARCEL_FAIL;
    }
    credParams.authType = static_cast<AuthType>(authType);
    credParams.pinType = static_cast<PinSubType>(authSubType);
    if (isAdd) {
        AddCredential(userId, credParams, callback);
    } else {
        UpdateCredential(userId, credParams, callback);
    }
    return ERR_NONE;
}

ErrCode AccountIAMMgrStub::ProcAddCredential(MessageParcel &data, MessageParcel &reply)
{
    return AddOrUpdateCredential(data, reply);
}

ErrCode AccountIAMMgrStub::ProcUpdateCredential(MessageParcel &data, MessageParcel &reply)
{
    return AddOrUpdateCredential(data, reply, false);
}

ErrCode AccountIAMMgrStub::ProcDelCred(MessageParcel &data, MessageParcel &reply)
{
    int32_t userId;
    if (!data.ReadInt32(userId)) {
        ACCOUNT_LOGD("failed to read userId");
        return ERR_ACCOUNT_IAM_SERVICE_READ_PARCEL_FAIL;
    }
    uint64_t credentialId;
    if (!data.ReadUint64(credentialId)) {
        ACCOUNT_LOGD("failed to read credentialId");
        return ERR_ACCOUNT_IAM_SERVICE_READ_PARCEL_FAIL;
    }
    std::vector<uint8_t> authToken;
    if (!data.ReadUInt8Vector(&authToken)) {
        ACCOUNT_LOGD("failed to read authToken");
        return ERR_ACCOUNT_IAM_SERVICE_READ_PARCEL_FAIL;
    }
    sptr<IIDMCallback> callback = iface_cast<IIDMCallback>(data.ReadRemoteObject());
    if (callback == nullptr) {
        ACCOUNT_LOGD("callback is nullptr");
        return ERR_ACCOUNT_IAM_SERVICE_READ_PARCEL_FAIL;
    }
    DelCred(userId, credentialId, authToken, callback);
    return ERR_NONE;
}

ErrCode AccountIAMMgrStub::ProcDelUser(MessageParcel &data, MessageParcel &reply)
{
    int32_t userId;
    if (!data.ReadInt32(userId)) {
        ACCOUNT_LOGD("failed to read userId");
        return ERR_ACCOUNT_IAM_SERVICE_READ_PARCEL_FAIL;
    }
    std::vector<uint8_t> authToken;
    if (!data.ReadUInt8Vector(&authToken)) {
        ACCOUNT_LOGD("failed to read authToken");
        return ERR_ACCOUNT_IAM_SERVICE_READ_PARCEL_FAIL;
    }
    sptr<IIDMCallback> callback = iface_cast<IIDMCallback>(data.ReadRemoteObject());
    if (callback == nullptr) {
        ACCOUNT_LOGD("callback is nullptr");
        return ERR_ACCOUNT_IAM_SERVICE_READ_PARCEL_FAIL;
    }
    DelUser(userId, authToken, callback);
    return ERR_NONE;
}

ErrCode AccountIAMMgrStub::ProcCancel(MessageParcel &data, MessageParcel &reply)
{
    int32_t userId;
    if (!data.ReadInt32(userId)) {
        ACCOUNT_LOGD("failed to read userId");
        return ERR_ACCOUNT_IAM_SERVICE_READ_PARCEL_FAIL;
    }
    uint64_t challenge;
    if (!data.ReadUint64(challenge)) {
        ACCOUNT_LOGD("failed to read challenge");
        return ERR_ACCOUNT_IAM_SERVICE_READ_PARCEL_FAIL;
    }
    int32_t resultCode = Cancel(userId, challenge);
    return reply.WriteInt32(resultCode);
}

ErrCode AccountIAMMgrStub::ProcGetCredentialInfo(MessageParcel &data, MessageParcel &reply)
{
    int32_t userId;
    int32_t authType;
    ErrCode ret = ReadUserIdAndAuthType(data, userId, authType);
    if (ret != ERR_OK) {
        return ret;
    }
    sptr<IGetCredInfoCallback> callback = iface_cast<IGetCredInfoCallback>(data.ReadRemoteObject());
    if (callback == nullptr) {
        ACCOUNT_LOGD("callback is nullptr");
        return ERR_ACCOUNT_IAM_SERVICE_READ_PARCEL_FAIL;
    }
    GetCredentialInfo(userId, static_cast<AuthType>(authType), callback);
    return ERR_NONE;
}

ErrCode AccountIAMMgrStub::ProcAuthUser(MessageParcel &data, MessageParcel &reply)
{
    int32_t userId;
    if (!data.ReadInt32(userId)) {
        ACCOUNT_LOGD("failed to read userId");
        return ERR_ACCOUNT_IAM_SERVICE_READ_PARCEL_FAIL;
    }
    std::vector<uint8_t> challenge;
    if (!data.ReadUInt8Vector(&challenge)) {
        ACCOUNT_LOGD("failed to read challenge");
        return ERR_ACCOUNT_IAM_SERVICE_READ_PARCEL_FAIL;
    }
    int32_t authType;
    if (!data.ReadInt32(authType)) {
        ACCOUNT_LOGD("failed to read authType");
        return ERR_ACCOUNT_IAM_SERVICE_READ_PARCEL_FAIL;
    }
    uint32_t authTrustLevel;
    if (!data.ReadUint32(authTrustLevel)) {
        ACCOUNT_LOGD("failed to read authTrustLevel");
        return ERR_ACCOUNT_IAM_SERVICE_READ_PARCEL_FAIL;
    }
    sptr<IIDMCallback> callback = iface_cast<IIDMCallback>(data.ReadRemoteObject());
    if (callback == nullptr) {
        ACCOUNT_LOGD("UserAuthCallbackInterface is nullptr");
        return ERR_ACCOUNT_IAM_SERVICE_READ_PARCEL_FAIL;
    }
    uint64_t contextId = AuthUser(userId, challenge, static_cast<AuthType>(authType),
        static_cast<AuthTrustLevel>(authTrustLevel), callback);
    return reply.WriteUint64(contextId);
}

ErrCode AccountIAMMgrStub::ProcCancelAuth(MessageParcel &data, MessageParcel &reply)
{
    uint64_t contextId;
    if (!data.ReadUint64(contextId)) {
        ACCOUNT_LOGD("failed to read contextId");
        return ERR_ACCOUNT_IAM_SERVICE_READ_PARCEL_FAIL;
    }
    int32_t result = CancelAuth(contextId);
    return reply.WriteInt32(result);
}

ErrCode AccountIAMMgrStub::ProcGetAvailableStatus(MessageParcel &data, MessageParcel &reply)
{
    int32_t authType;
    if (!data.ReadInt32(authType)) {
        ACCOUNT_LOGD("failed to read authType");
        return ERR_ACCOUNT_IAM_SERVICE_READ_PARCEL_FAIL;
    }
    uint32_t authTrustLevel;
    if (!data.ReadUint32(authTrustLevel)) {
        ACCOUNT_LOGD("failed to read authTrustLevel");
        return ERR_ACCOUNT_IAM_SERVICE_READ_PARCEL_FAIL;
    }
    int32_t status = GetAvailableStatus(static_cast<AuthType>(authType), static_cast<AuthTrustLevel>(authTrustLevel));
    return reply.WriteInt32(status);
}

ErrCode AccountIAMMgrStub::ProcGetProperty(MessageParcel &data, MessageParcel &reply)
{
    int32_t userId;
    int32_t authType;
    ErrCode ret = ReadUserIdAndAuthType(data, userId, authType);
    if (ret != ERR_OK) {
        return ret;
    }
    std::vector<uint32_t> keys;
    if (!data.ReadUInt32Vector(&keys)) {
        ACCOUNT_LOGD("failed to read attribute keys");
        return ERR_ACCOUNT_IAM_SERVICE_READ_PARCEL_FAIL;
    }
    sptr<IGetSetPropCallback> callback = iface_cast<IGetSetPropCallback>(data.ReadRemoteObject());
    if (callback == nullptr) {
        ACCOUNT_LOGD("IGetSetPropCallback is nullptr");
        return ERR_ACCOUNT_IAM_SERVICE_READ_PARCEL_FAIL;
    }
    GetPropertyRequest request;
    request.authType = static_cast<AuthType>(authType);
    for (auto &key : keys) {
        request.keys.push_back(static_cast<Attributes::AttributeKey>(key));
    }
    GetProperty(userId, request, callback);
    return ERR_NONE;
}

ErrCode AccountIAMMgrStub::ProcSetProperty(MessageParcel &data, MessageParcel &reply)
{
    int32_t userId;
    int32_t authType;
    ErrCode ret = ReadUserIdAndAuthType(data, userId, authType);
    if (ret != ERR_OK) {
        return ret;
    }
    std::vector<uint8_t> attr;
    if (!data.ReadUInt8Vector(&attr)) {
        ACCOUNT_LOGD("failed to read attributes");
        return ERR_ACCOUNT_IAM_SERVICE_READ_PARCEL_FAIL;
    }
    sptr<IGetSetPropCallback> callback = iface_cast<IGetSetPropCallback>(data.ReadRemoteObject());
    if (callback == nullptr) {
        ACCOUNT_LOGD("SetExecutorPropertyCallbackInterface is nullptr");
        return ERR_ACCOUNT_IAM_SERVICE_READ_PARCEL_FAIL;
    }
    SetPropertyRequest request = {
        .authType = static_cast<AuthType>(authType),
        .attrs = Attributes(attr)
    };
    SetProperty(userId, request, callback);
    return ERR_NONE;
}

ErrCode AccountIAMMgrStub::ProcRegisterInputer(MessageParcel &data, MessageParcel &reply)
{
    sptr<IGetDataCallback> inputer = iface_cast<IGetDataCallback>(data.ReadRemoteObject());
    if (inputer == nullptr) {
        ACCOUNT_LOGD("inputer is nullptr");
        return ERR_ACCOUNT_IAM_SERVICE_READ_PARCEL_FAIL;
    }
    bool result = RegisterInputer(inputer);
    return reply.WriteBool(result);
}

ErrCode AccountIAMMgrStub::ProcUnRegisterInputer(MessageParcel &data, MessageParcel &reply)
{
    UnRegisterInputer();
    return ERR_NONE;
}
}  // AccountSA
}  // OHOS