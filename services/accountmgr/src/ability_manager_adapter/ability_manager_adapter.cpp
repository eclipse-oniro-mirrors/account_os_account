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

#include "ability_manager_adapter.h"
#include "ability_manager_errors.h"
#include "ability_manager_ipc_interface_code.h"
#include "account_error_no.h"
#include "account_log_wrapper.h"
#include "account_hisysevent_adapter.h"
#include "iservice_registry.h"
#include "system_ability_definition.h"
#ifdef ENABLE_DEACTIVATE_OS_ACCOUNTS
#include "app_mgr_interface.h"
#include "app_mgr_proxy.h"
#endif

namespace OHOS {
namespace AccountSA {
namespace {
const std::u16string ABILITY_MGR_DESCRIPTOR = u"ohos.aafwk.AbilityManager";
constexpr int32_t INVALID_ID = -1;
}
using namespace AAFwk;
AbilityManagerAdapter *AbilityManagerAdapter::GetInstance()
{
    static AbilityManagerAdapter *instance = new (std::nothrow) AbilityManagerAdapter();
    return instance;
}

AbilityManagerAdapter::AbilityManagerAdapter()
{}

AbilityManagerAdapter::~AbilityManagerAdapter()
{}

ErrCode AbilityManagerAdapter::ConnectAbility(const AAFwk::Want &want, const sptr<AAFwk::IAbilityConnection> &connect,
    const sptr<IRemoteObject> &callerToken, int32_t userId)
{
    auto abms = GetAbilityManager();
    if (abms == nullptr) {
        ACCOUNT_LOGE("ability manager proxy is nullptr.");
        return ERR_ACCOUNT_COMMON_CONNECT_ABILITY_MANAGER_SERVICE_ERROR;
    }

    return DoConnectAbility(abms, want, connect, callerToken, userId);
}

ErrCode AbilityManagerAdapter::DisconnectAbility(const sptr<AAFwk::IAbilityConnection> &connect)
{
    auto abms = GetAbilityManager();
    if (abms == nullptr) {
        ACCOUNT_LOGE("ability manager proxy is nullptr.");
        return ERR_ACCOUNT_COMMON_CONNECT_ABILITY_MANAGER_SERVICE_ERROR;
    }

    int error;
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    if (connect == nullptr) {
        ACCOUNT_LOGE("disconnect ability fail, connect is nullptr");
        return ERR_INVALID_VALUE;
    }
    if (!data.WriteInterfaceToken(ABILITY_MGR_DESCRIPTOR)) {
        ACCOUNT_LOGE("write interface token failed.");
        return INNER_ERR;
    }
    if (!data.WriteRemoteObject(connect->AsObject())) {
        ACCOUNT_LOGE("connect write failed.");
        return ERR_INVALID_VALUE;
    }

    error = abms->SendRequest(
        static_cast<uint32_t>(AbilityManagerInterfaceCode::DISCONNECT_ABILITY), data, reply, option);
    if (error != NO_ERROR) {
        ACCOUNT_LOGE("Send request error: %{public}d", error);
        return error;
    }
    return reply.ReadInt32();
}

void AbilityManagerAdapter::Connect()
{
    if (proxy_ != nullptr) {
        return;
    }
    sptr<ISystemAbilityManager> systemManager = SystemAbilityManagerClient::GetInstance().GetSystemAbilityManager();
    if (systemManager == nullptr) {
        ACCOUNT_LOGE("Fail to get system ability registry.");
        return;
    }
    sptr<IRemoteObject> remoteObj = systemManager->CheckSystemAbility(ABILITY_MGR_SERVICE_ID);
    if (remoteObj == nullptr) {
        ACCOUNT_LOGE("Fail to connect ability manager service.");
        return;
    }

    deathRecipient_ = sptr<IRemoteObject::DeathRecipient>(new (std::nothrow) AbilityMgrDeathRecipient());
    if (deathRecipient_ == nullptr) {
        ACCOUNT_LOGE("Failed to create AbilityMgrDeathRecipient!");
        return;
    }
    if ((remoteObj->IsProxyObject()) && (!remoteObj->AddDeathRecipient(deathRecipient_))) {
        ACCOUNT_LOGE("Add death recipient to AbilityManagerService failed.");
        return;
    }
    proxy_ = remoteObj;
}

ErrCode AbilityManagerAdapter::StartUser
    (int32_t accountId, const sptr<AAFwk::IUserCallback> &callback, bool isAppRecovery)
{
    auto abms = GetAbilityManager();
    if (abms == nullptr) {
        ACCOUNT_LOGE("ability manager proxy is nullptr.");
        return ERR_ACCOUNT_COMMON_CONNECT_ABILITY_MANAGER_SERVICE_ERROR;
    }

    int error;
    MessageParcel data;
    MessageParcel reply;
    MessageOption option(MessageOption::TF_ASYNC);

    if (!data.WriteInterfaceToken(ABILITY_MGR_DESCRIPTOR)) {
        ACCOUNT_LOGE("write interface token failed.");
        return INNER_ERR;
    }

    if (!data.WriteInt32(accountId)) {
        ACCOUNT_LOGE("StartUser:WriteInt32 fail.");
        return ERR_INVALID_VALUE;
    }

    if (callback == nullptr) {
        data.WriteBool(false);
    } else {
        data.WriteBool(true);
        if (!data.WriteRemoteObject(callback->AsObject())) {
            ACCOUNT_LOGE("StartUser:write IUserCallback fail.");
            return ERR_INVALID_VALUE;
        }
    }

    if (!data.WriteBool(isAppRecovery)) {
        ACCOUNT_LOGE("StartUser:WriteBool fail.");
        return ERR_INVALID_VALUE;
    }

    error = abms->SendRequest(static_cast<uint32_t>(AbilityManagerInterfaceCode::START_USER), data, reply, option);
    if (error != NO_ERROR) {
        ACCOUNT_LOGE("StartUser:SendRequest error: %{public}d", error);
        return error;
    }

    return reply.ReadInt32();
}

ErrCode AbilityManagerAdapter::StopUser(int32_t accountId, const sptr<AAFwk::IUserCallback> &callback)
{
    auto abms = GetAbilityManager();
    if (abms == nullptr) {
        ACCOUNT_LOGE("ability manager proxy is nullptr.");
        return ERR_ACCOUNT_COMMON_CONNECT_ABILITY_MANAGER_SERVICE_ERROR;
    }

    int error;
    MessageParcel data;
    MessageParcel reply;
    MessageOption option(MessageOption::TF_ASYNC);

    if (!data.WriteInterfaceToken(ABILITY_MGR_DESCRIPTOR)) {
        ACCOUNT_LOGE("write interface token failed.");
        return INNER_ERR;
    }
    if (!data.WriteInt32(accountId)) {
        ACCOUNT_LOGE("StopUser:WriteInt32 fail.");
        return ERR_INVALID_VALUE;
    }

    if (callback == nullptr) {
        data.WriteBool(false);
    } else {
        data.WriteBool(true);
        if (!data.WriteRemoteObject(callback->AsObject())) {
            ACCOUNT_LOGE("StopUser:write IUserCallback fail.");
            return ERR_INVALID_VALUE;
        }
    }
    error = abms->SendRequest(static_cast<uint32_t>(AbilityManagerInterfaceCode::STOP_USER), data, reply, option);
    if (error != NO_ERROR) {
        ACCOUNT_LOGE("StopUser:SendRequest error: %{public}d", error);
        return error;
    }
    return reply.ReadInt32();
}

ErrCode AbilityManagerAdapter::LogoutUser(int32_t accountId, const sptr<IUserCallback> &callback)
{
    auto abms = GetAbilityManager();
    if (abms == nullptr) {
        ACCOUNT_LOGE("ability manager proxy is nullptr.");
        return ERR_ACCOUNT_COMMON_CONNECT_ABILITY_MANAGER_SERVICE_ERROR;
    }

    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(ABILITY_MGR_DESCRIPTOR)) {
        ACCOUNT_LOGE("write interface token failed.");
        return INNER_ERR;
    }
    if (!data.WriteInt32(accountId)) {
        ACCOUNT_LOGE("write accountId failed.");
        return ERR_INVALID_VALUE;
    }
    if (callback == nullptr) {
        if (!data.WriteBool(false)) {
            ACCOUNT_LOGE("Write without callback fail.");
            return ERR_INVALID_VALUE;
        }
    } else {
        if (!data.WriteBool(true)) {
            ACCOUNT_LOGE("Write with callback fail.");
            return ERR_INVALID_VALUE;
        }
        if (!data.WriteRemoteObject(callback->AsObject())) {
            ACCOUNT_LOGE("Write IUserCallback fail.");
            return ERR_INVALID_VALUE;
        }
    }
    int32_t errCode =
        abms->SendRequest(static_cast<uint32_t>(AbilityManagerInterfaceCode::LOGOUT_USER), data, reply, option);
    if (errCode != NO_ERROR) {
        ACCOUNT_LOGE("SendRequest error: %{public}d", errCode);
        return errCode;
    }
    if (!reply.ReadInt32(errCode)) {
        ACCOUNT_LOGE("read result failed.");
        return ERR_INVALID_VALUE;
    }
    return errCode;
}

ErrCode AbilityManagerAdapter::DoConnectAbility(const sptr<IRemoteObject> proxy, const AAFwk::Want &want,
    const sptr<AAFwk::IAbilityConnection> &connect, const sptr<IRemoteObject> &callerToken, int32_t userId)
{
    int error;
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (proxy == nullptr || connect == nullptr) {
        ACCOUNT_LOGE("connect ability fail, proxy or connect is nullptr");
        return ERR_INVALID_VALUE;
    }
    if (!data.WriteInterfaceToken(ABILITY_MGR_DESCRIPTOR)) {
        ACCOUNT_LOGE("write interface token failed.");
        return INNER_ERR;
    }
    if (!data.WriteParcelable(&want)) {
        ACCOUNT_LOGE("want write failed.");
        return ERR_INVALID_VALUE;
    }
    if (connect->AsObject()) {
        if (!data.WriteBool(true) || !data.WriteRemoteObject(connect->AsObject())) {
            ACCOUNT_LOGE("flag and connect write failed.");
            return ERR_INVALID_VALUE;
        }
    } else if (!data.WriteBool(false)) {
        ACCOUNT_LOGE("flag write failed.");
        return ERR_INVALID_VALUE;
    }
    if (callerToken) {
        if (!data.WriteBool(true) || !data.WriteRemoteObject(callerToken)) {
            ACCOUNT_LOGE("flag and callerToken write failed.");
            return ERR_INVALID_VALUE;
        }
    } else if (!data.WriteBool(false)) {
        ACCOUNT_LOGE("flag write failed.");
        return ERR_INVALID_VALUE;
    }
    if (!data.WriteInt32(userId)) {
        ACCOUNT_LOGE("userId write failed.");
        return INNER_ERR;
    }
    error = proxy->SendRequest(
        static_cast<uint32_t>(AbilityManagerInterfaceCode::CONNECT_ABILITY), data, reply, option);
    if (error != NO_ERROR) {
        ACCOUNT_LOGE("Send request error: %{public}d", error);
        return error;
    }
    return reply.ReadInt32();
}

sptr<IRemoteObject> AbilityManagerAdapter::GetAbilityManager()
{
    std::lock_guard<std::mutex> lock(proxyMutex_);
    if (!proxy_) {
        Connect();
    }
    return proxy_;
}

void AbilityManagerAdapter::ResetProxy(const wptr<IRemoteObject>& remote)
{
    std::lock_guard<std::mutex> lock(proxyMutex_);
    if ((proxy_ != nullptr) && (proxy_ == remote.promote())) {
        proxy_->RemoveDeathRecipient(deathRecipient_);
        proxy_ = nullptr;
    }
}

void AbilityManagerAdapter::AbilityMgrDeathRecipient::OnRemoteDied(const wptr<IRemoteObject>& remote)
{
    ACCOUNT_LOGI("AbilityMgrDeathRecipient handle remote died.");
    ReportOsAccountOperationFail(INVALID_ID, "resetService", ERR_OK, "AbilityMgr remote died");
    AbilityManagerAdapter::GetInstance()->ResetProxy(remote);
}

bool AbilityManagerAdapter::IsAllAppDied(int32_t accountId)
{
#ifdef ENABLE_DEACTIVATE_OS_ACCOUNTS
    sptr<ISystemAbilityManager> abilityMgr = SystemAbilityManagerClient::GetInstance().GetSystemAbilityManager();
    if (abilityMgr == nullptr) {
        ACCOUNT_LOGE("Failed to get ISystemAbilityManager");
        return false;
    }

    sptr<IRemoteObject> remoteObject = abilityMgr->CheckSystemAbility(APP_MGR_SERVICE_ID);
    if (remoteObject == nullptr) {
        ACCOUNT_LOGE("Failed to get app Manager service");
        return false;
    }
    sptr<AppExecFwk::IAppMgr> appMgrProxy = iface_cast<AppExecFwk::IAppMgr>(remoteObject);
    if (appMgrProxy == nullptr || !appMgrProxy->AsObject()) {
        ACCOUNT_LOGE("Failed to get app mgr proxy");
        return false;
    }
    std::vector<AppExecFwk::RunningProcessInfo> appList;
    int ret = appMgrProxy->GetProcessRunningInfosByUserId(appList, accountId);
    ACCOUNT_LOGI("GetProcessRunningInfosByUserId ret: %{public}d", ret);
    if (ret != 0) {
        return false;
    }
    return appList.empty();
#else
    return true;
#endif
}
}  // namespace AAFwk
}  // namespace OHOS
