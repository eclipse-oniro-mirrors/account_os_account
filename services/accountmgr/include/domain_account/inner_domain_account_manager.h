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

#ifndef OS_ACCOUNT_SERVICES_DOMAIN_ACCOUNT_INCLUDE_INNER_DOMAIN_ACCOUNT_MANAGER_H
#define OS_ACCOUNT_SERVICES_DOMAIN_ACCOUNT_INCLUDE_INNER_DOMAIN_ACCOUNT_MANAGER_H

#include <mutex>
#include "domain_account_plugin_death_recipient.h"
#include "domain_account_plugin_proxy.h"
#include "domain_account_callback.h"
#include "domain_auth_callback.h"
#include "domain_auth_callback_stub.h"
#include "os_account_info.h"

namespace OHOS {
namespace AccountSA {
class InnerDomainAccountManager {
public:
    static InnerDomainAccountManager &GetInstance();
    ErrCode RegisterPlugin(const sptr<IDomainAccountPlugin> &plugin);
    void UnregisterPlugin();
    ErrCode Auth(const DomainAccountInfo &info, const std::vector<uint8_t> &password,
        const std::shared_ptr<DomainAuthCallback> &callback);
    ErrCode Auth(const DomainAccountInfo &info, const std::vector<uint8_t> &password,
        const sptr<IDomainAuthCallback> &callback);
    ErrCode AuthUser(int32_t userId, const std::vector<uint8_t> &password,
        const sptr<IDomainAuthCallback> &callback);
    ErrCode AuthWithPopup(int32_t userId, const sptr<IDomainAuthCallback> &callback);
    ErrCode AuthWithToken(int32_t userId, const std::vector<uint8_t> &token);
    ErrCode GetAuthStatusInfo(const DomainAccountInfo &info, const sptr<IDomainAccountCallback> &callback);
    ErrCode HasDomainAccount(const DomainAccountInfo &info, const sptr<IDomainAccountCallback> &callback);
    ErrCode GetDomainAccountInfo(const std::string &domain, const std::string &accountName,
        const std::shared_ptr<DomainAccountCallback> &callback);
    ErrCode OnAccountBound(const DomainAccountInfo &info, const int32_t localId,
        const std::shared_ptr<DomainAccountCallback> &callback);
    ErrCode OnAccountUnBound(const DomainAccountInfo &info, const std::shared_ptr<DomainAccountCallback> &callback);
    bool IsPluginAvailable();
    void InsertTokenToMap(int32_t userId, const std::vector<uint8_t> &token);
    std::vector<uint8_t> GetTokenFromMap(int32_t userId);
    void RemoveTokenFromMap(int32_t userId);

private:
    void StartGetDomainAccountInfo(const sptr<IDomainAccountPlugin> &plugin, const std::string &domain,
        const std::string &accountName, const sptr<IDomainAccountCallback> &callback);
    void StartOnAccountUnBound(const sptr<IDomainAccountPlugin> &plugin, const DomainAccountInfo &info,
        const sptr<IDomainAccountCallback> &callback);
    void StartOnAccountBound(const sptr<IDomainAccountPlugin> &plugin, const DomainAccountInfo &info,
        const int32_t localId, const sptr<IDomainAccountCallback> &callback);
    ErrCode StartHasDomainAccount(const sptr<IDomainAccountPlugin> &plugin, const DomainAccountInfo &info,
        const sptr<IDomainAccountCallback> &callback);
    ErrCode StartAuth(const sptr<IDomainAccountPlugin> &plugin, const DomainAccountInfo &info,
        const std::vector<uint8_t> &password, const sptr<IDomainAuthCallback> &callback, AuthMode authMode);
    sptr<IRemoteObject::DeathRecipient> GetDeathRecipient();
    ErrCode InnerAuth(int32_t userId, const std::vector<uint8_t> &authData,
        const sptr<IDomainAuthCallback> &callback, AuthMode authMode);
    ErrCode GetDomainAccountInfoByUserId(int32_t userId, DomainAccountInfo &domainInfo);

private:
    std::mutex mutex_;
    sptr<IRemoteObject::DeathRecipient> deathRecipient_;
    sptr<IDomainAccountPlugin> plugin_;
    std::map<int32_t, std::vector<uint8_t>> userTokenMap_;
};

class InnerDomainAuthCallback final: public DomainAuthCallbackStub {
public:
    InnerDomainAuthCallback(int32_t userId, const sptr<IDomainAuthCallback> &callback);
    virtual ~InnerDomainAuthCallback();
    void OnResult(int32_t resultCode, const DomainAuthResult &result) override;

private:
    int32_t userId_;
    sptr<IDomainAuthCallback> callback_;
};
}  // namespace AccountSA
}  // namespace OHOS
#endif // OS_ACCOUNT_SERVICES_DOMAIN_ACCOUNT_INCLUDE_INNER_DOMAIN_ACCOUNT_MANAGER_H
