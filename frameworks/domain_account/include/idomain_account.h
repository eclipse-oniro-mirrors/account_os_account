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

#ifndef OS_ACCOUNT_INTERFACES_INNERKITS_DOMAIN_ACCOUNT_INCLUDE_IDOMAIN_ACCOUNT_H
#define OS_ACCOUNT_INTERFACES_INNERKITS_DOMAIN_ACCOUNT_INCLUDE_IDOMAIN_ACCOUNT_H

#include <string>
#include <iremote_broker.h>
#include "idomain_account_callback.h"
#include "idomain_account_plugin.h"
#include "want.h"

namespace OHOS {
namespace AccountSA {
class IDomainAccount : public IRemoteBroker {
public:
    DECLARE_INTERFACE_DESCRIPTOR(u"ohos.accountfwk.IDomainAccount");
    enum Message {
        REGISTER_PLUGIN = 0,
        UNREGISTER_PLUGIN = 1,
        DOMAIN_AUTH = 2,
        DOMAIN_AUTH_USER = 3,
        DOMAIN_AUTH_WITH_POPUP = 4,
        DOMAIN_HAS_DOMAIN_ACCOUNT = 5,
        DOMAIN_UPDATE_ACCOUNT_TOKEN = 6,
        DOMAIN_GET_ACCESS_TOKEN = 7,
        DOMAIN_ACCOUNT_STATUS_ENQUIRY = 8,
        DOMAIN_ACCOUNT_STATUS_LISTENER_REGISTER = 9,
        DOMAIN_ACCOUNT_STATUS_LISTENER_UNREGISTER = 10
    };

    virtual ErrCode RegisterPlugin(const sptr<IDomainAccountPlugin> &plugin) = 0;
    virtual ErrCode UnregisterPlugin() = 0;
    virtual ErrCode GetAccountStatus(
        const std::string &domain, const std::string &accountName, DomainAccountStatus &status) = 0;
    virtual ErrCode RegisterAccountStatusListener(
        const DomainAccountInfo &info, const sptr<IDomainAccountCallback> &listener) = 0;
    virtual ErrCode UnregisterAccountStatusListener(const DomainAccountInfo &info) = 0;
    virtual ErrCode Auth(const DomainAccountInfo &info, const std::vector<uint8_t> &password,
        const sptr<IDomainAuthCallback> &callback) = 0;
    virtual ErrCode AuthUser(int32_t userId, const std::vector<uint8_t> &password,
        const sptr<IDomainAuthCallback> &callback) = 0;
    virtual ErrCode AuthWithPopup(int32_t userId, const sptr<IDomainAuthCallback> &callback) = 0;
    virtual ErrCode HasDomainAccount(const DomainAccountInfo &info, const sptr<IDomainAccountCallback> &callback) = 0;
    virtual ErrCode UpdateAccountToken(const DomainAccountInfo &info, const std::vector<uint8_t> &token) = 0;
    virtual ErrCode GetAccessToken(const DomainAccountInfo &info, const AAFwk::WantParams &parameters,
        const sptr<IDomainAccountCallback> &callback) = 0;
};
}  // namespace AccountSA
}  // namespace OHOS
#endif  // OS_ACCOUNT_INTERFACES_INNERKITS_DOMAIN_ACCOUNT_INCLUDE_IDOMAIN_ACCOUNT_H
