/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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

#ifndef OS_ACCOUNT_TEST_MOCK_SHADOW_USER_IDM_CLIENT_H
#define OS_ACCOUNT_TEST_MOCK_SHADOW_USER_IDM_CLIENT_H

#include <memory>
#include <vector>

#include "iam_common_defines.h"
#include "user_idm_client_callback.h"
#include "user_idm_client_defines.h"

namespace OHOS {
namespace UserIam {
namespace UserAuth {
class UserIdmClient {
public:
    static UserIdmClient &GetInstance();

    virtual ~UserIdmClient() = default;

    virtual std::vector<uint8_t> OpenSession(int32_t userId) = 0;
    virtual void CloseSession(int32_t userId) = 0;
    virtual void AddCredential(int32_t userId, const CredentialParameters &para,
        const std::shared_ptr<UserIdmClientCallback> &callback) = 0;
    virtual void UpdateCredential(int32_t userId, const CredentialParameters &para,
        const std::shared_ptr<UserIdmClientCallback> &callback) = 0;
    virtual int32_t Cancel(int32_t userId) = 0;
    virtual void DeleteCredential(int32_t userId, uint64_t credentialId,
        const std::vector<uint8_t> &authToken, const std::shared_ptr<UserIdmClientCallback> &callback) = 0;
    virtual void DeleteUser(int32_t userId, const std::vector<uint8_t> &authToken,
        const std::shared_ptr<UserIdmClientCallback> &callback) = 0;
    virtual int32_t EraseUser(int32_t userId, const std::shared_ptr<UserIdmClientCallback> &callback) = 0;
    virtual int32_t GetCredentialInfo(int32_t userId, AuthType authType,
        const std::shared_ptr<GetCredentialInfoCallback> &callback) = 0;
    virtual int32_t GetSecUserInfo(int32_t userId, const std::shared_ptr<GetSecUserInfoCallback> &callback) = 0;
    virtual int32_t RegistCredChangeEventListener(const std::vector<AuthType> &authTypes,
        const std::shared_ptr<CredChangeEventListener> &listener) = 0;
    virtual int32_t UnRegistCredChangeEventListener(
        const std::shared_ptr<CredChangeEventListener> &listener) = 0;
    virtual int32_t GetCredentialInfoSync(int32_t userId, AuthType authType,
        std::vector<CredentialInfo> &credentialInfoList) = 0;
    virtual void DeleteSubProfile(int32_t userId, int32_t subProfileId,
        const std::shared_ptr<UserIdmClientCallback> &callback) = 0;
};
} // namespace UserAuth
} // namespace UserIam
} // namespace OHOS
#endif // OS_ACCOUNT_TEST_MOCK_SHADOW_USER_IDM_CLIENT_H
