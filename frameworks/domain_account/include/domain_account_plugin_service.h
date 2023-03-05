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

#ifndef OS_ACCOUNT_INTERFACES_INNERKITS_DOMAIN_ACCOUNT_INCLUDE_DOMAIN_ACCOUNT_PLUGIN_SERVICE_H
#define OS_ACCOUNT_INTERFACES_INNERKITS_DOMAIN_ACCOUNT_INCLUDE_DOMAIN_ACCOUNT_PLUGIN_SERVICE_H

#include <string>
#include "account_error_no.h"
#include "domain_account_common.h"
#include "domain_account_plugin.h"
#include "domain_account_plugin_stub.h"

namespace OHOS {
namespace AccountSA {
class DomainAccountPluginService : public DomainAccountPluginStub {
public:
    explicit DomainAccountPluginService(const std::shared_ptr<DomainAccountPlugin> &plugin);
    ~DomainAccountPluginService() override;
    ErrCode Auth(const DomainAccountInfo &info, const std::vector<uint8_t> &password,
        const sptr<IDomainAuthCallback> &callback) override;
    ErrCode AuthWithPopup(const DomainAccountInfo &info, const sptr<IDomainAuthCallback> &callback) override;
    ErrCode AuthWithToken(const DomainAccountInfo &info, const std::vector<uint8_t> &token,
        const sptr<IDomainAuthCallback> &callback) override;
    ErrCode GetAuthStatusInfo(const DomainAccountInfo &info, const sptr<IDomainAccountCallback> &callback) override;
    ErrCode GetDomainAccountInfo(const std::string &domain, const std::string &accountName,
        const sptr<IDomainAccountCallback> &callback) override;
    ErrCode OnAccountBound(const DomainAccountInfo &info, const int32_t localId) override;
    ErrCode OnAccountUnBound(const DomainAccountInfo &info) override;

private:
    ErrCode AuthCommonInterface(const DomainAccountInfo &info, const std::vector<uint8_t> &authData,
        const sptr<IDomainAuthCallback> &callback, AuthMode authMode);
    std::shared_ptr<DomainAccountPlugin> innerPlugin_;
    DISALLOW_COPY_AND_MOVE(DomainAccountPluginService);
};
}  // namespace AccountSA
}  // namespace OHOS
#endif  // OS_ACCOUNT_INTERFACES_INNERKITS_DOMAIN_ACCOUNT_INCLUDE_DOMAIN_ACCOUNT_PLUGIN_SERVICE_H
