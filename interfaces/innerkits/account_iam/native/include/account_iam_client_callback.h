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

#ifndef OS_ACCOUNT_INTERFACES_INNERKITS_ACCOUNT_IAM_NATIVE_INCLUDE_ACCOUNT_IAM_CLIENT_CALLBACK_H
#define OS_ACCOUNT_INTERFACES_INNERKITS_ACCOUNT_IAM_NATIVE_INCLUDE_ACCOUNT_IAM_CLIENT_CALLBACK_H

#include "account_iam_callback_stub.h"

namespace OHOS {
namespace AccountSA {
class IDMCallback {
public:
    virtual void OnAcquireInfo(int32_t module, uint32_t acquireInfo, const Attributes &extraInfo) = 0;
    virtual void OnResult(int32_t result, const Attributes &extraInfo) = 0;
};

class GetCredInfoCallback {
public:
    virtual void OnCredentialInfo(const std::vector<CredentialInfo> &infoList) = 0;
};

class GetSetPropCallback {
public:
    virtual void OnResult(int32_t result, const Attributes &extraInfo) = 0;
};

class GetDataCallback {
public:
    virtual void OnGetData(int32_t authSubType, const sptr<ISetDataCallback> &inputerData) = 0;
};
}  // namespace AccountSA
}  // namespace OHOS
#endif  // OS_ACCOUNT_INTERFACES_INNERKITS_ACCOUNT_IAM_NATIVE_INCLUDE_ACCOUNT_IAM_CLIENT_CALLBACK_H
