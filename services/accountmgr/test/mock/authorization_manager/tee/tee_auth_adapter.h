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

#ifndef MOCK_AUTHORIZATION_TEE_ADAPTER_H
#define MOCK_AUTHORIZATION_TEE_ADAPTER_H

#include <securec.h>

namespace OHOS {
namespace AccountSA {
constexpr int32_t USER_TOKEN_LEN = 2048;
constexpr int32_t AUTH_TOKEN_LEN = 1024;
constexpr int32_t PERMISSION_MAX_LEN = 1024;
constexpr int32_t MAX_PERMISSIONS_NUM = 32;
constexpr size_t MAX_TOKEN_SIZE = 1024;
constexpr size_t CHALLENGE_LEN = 32;
constexpr size_t TOKEN_DATA_CIPHER_SIZE = 4;
constexpr size_t TOKEN_DATA_TAG_SIZE = 16;
constexpr size_t TOKEN_DATA_IV_SIZE = 12;
constexpr size_t TOKEN_CRYPTO_SIGN_SIZE = 32;

typedef struct {
    uint32_t pid;
    uint32_t privilege;
    uint32_t grantTime;
    int32_t grantValidityPeriod;
    uint8_t challenge[CHALLENGE_LEN];
    uint8_t authToken[AUTH_TOKEN_LEN];
    size_t authTokenSize;
} __attribute__((__packed__)) UserTokenDataPlain;

typedef struct {
    int32_t grantUserId;
} UserTokenDataToEncrypt;

typedef struct {
    UserTokenDataPlain userTokenDataPlain;
    UserTokenDataToEncrypt userTokenDataToEncrypt;
} UserTokenPlain;

typedef struct {
    uint8_t dataCipher[TOKEN_DATA_CIPHER_SIZE];
    uint8_t tag[TOKEN_DATA_TAG_SIZE];
    uint8_t iv[TOKEN_DATA_IV_SIZE];
} __attribute__((__packed__)) UserTokenDataCipher;

typedef struct {
    uint32_t version;
    UserTokenDataPlain userTokenDataPlain;
    UserTokenDataCipher userTokenDataCipher;
    uint8_t sign[TOKEN_CRYPTO_SIGN_SIZE];
} __attribute__((__packed__)) UserTokenCrypto;

typedef struct {
    UserTokenPlain userTokenPlain;
    int32_t remainValidityTime;
} __attribute__((__packed__)) VerifyUserTokenResult;

struct VerifyGrantTimeResult {
    int32_t isEffective = 0;
    int32_t remainValidityTime = 0;
};

typedef struct ApplyUserTokenParam {
    uint32_t pid;
    uint8_t permission[PERMISSION_MAX_LEN + 1];
    uint8_t permissionSize;
    int32_t grantValidityPeriod;
    int32_t grantUserId;
    uint8_t authToken[AUTH_TOKEN_LEN];
    size_t authTokenSize;
    uint8_t challenge[CHALLENGE_LEN];
} __attribute__((__packed__)) ApplyUserTokenParam;

typedef struct {
    uint8_t userToken[USER_TOKEN_LEN];
    size_t userTokenSize;
    int32_t remainValidityTime;
    uint32_t grantTime;
} __attribute__((__packed__)) ApplyUserTokenResult;

typedef struct {
    uint32_t pid;
    uint32_t privileges[MAX_PERMISSIONS_NUM];
    uint32_t privilegesNum;
    uint32_t grantTime;
    int32_t grantValidityPeriod;
    uint8_t challenge[CHALLENGE_LEN];
    uint8_t authToken[AUTH_TOKEN_LEN];
    size_t authTokenSize;
} __attribute__((__packed__)) UserTokenDataPlainV2;

typedef struct {
    UserTokenDataPlainV2 userTokenDataPlain;
    UserTokenDataToEncrypt userTokenDataToEncrypt;
} UserTokenPlainV2;

typedef struct ApplyUserTokenResultV2 {
    uint8_t userToken[USER_TOKEN_LEN];
    uint32_t userTokenSize;
    uint32_t remainValidityTime;
    uint32_t grantTime;
    ~ApplyUserTokenResultV2()
    {
        (void)memset_s(&userToken, USER_TOKEN_LEN, 0, USER_TOKEN_LEN);
    }
} __attribute__((__packed__)) ApplyUserTokenResultV2;

typedef struct ApplyUserTokenParamV2 {
    uint32_t pid = 0;
    uint8_t permission[MAX_PERMISSIONS_NUM][PERMISSION_MAX_LEN + 1];
    uint32_t permissionSizes[MAX_PERMISSIONS_NUM];
    uint32_t permissionsNum = 0;
    int32_t grantValidityPeriod = 0;
    int32_t grantUserId = 0;
    uint8_t authToken[AUTH_TOKEN_LEN];
    uint32_t authTokenSize = 0;
    uint8_t challenge[CHALLENGE_LEN];
    ~ApplyUserTokenParamV2()
    {
        (void)memset_s(&authToken, AUTH_TOKEN_LEN * sizeof(uint8_t), 0, AUTH_TOKEN_LEN * sizeof(uint8_t));
        (void)memset_s(&challenge, CHALLENGE_LEN * sizeof(uint8_t), 0, CHALLENGE_LEN * sizeof(uint8_t));
    }
} __attribute__((__packed__)) ApplyUserTokenParamV2;

/**
 * @brief An adapter class for managing OS account operations with Trusted Application (TA) in TEE.
 *
 * This class provides a unified interface for interacting with TEE to perform various
 * OS account operations such as setting account type, creating accounts, deleting accounts, etc.
 */
class OsAccountTeeAdapter {
public:
    OsAccountTeeAdapter() = default;
    ~OsAccountTeeAdapter() = default;

    ErrCode TaAcquireAuthorization(const ApplyUserTokenParam &param, ApplyUserTokenResult &result);
    ErrCode TaAcquireAuthorizationBatch(const ApplyUserTokenParamV2 &param,
        ApplyUserTokenResultV2 &result);
    ErrCode VerifyToken(const std::vector<uint8_t>& token,
        const std::string &privilege, std::vector<uint8_t>& tokenResult);
};
} // namespace AccountSA
} // namespace OHOS
#endif // OS_ACCOUNT_AUTHORIZATION_TEE_ADAPTER_H
