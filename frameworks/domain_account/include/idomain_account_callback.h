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

#ifndef OS_ACCOUNT_INTERFACES_INNERKITS_DOMAIN_ACCOUNT_INCLUDE_IDOMAIN_ACCOUNT_CALLBACK_H
#define OS_ACCOUNT_INTERFACES_INNERKITS_DOMAIN_ACCOUNT_INCLUDE_IDOMAIN_ACCOUNT_CALLBACK_H

#include <iremote_broker.h>
#include "accountmgr_service_ipc_interface_code.h"
#include "domain_account_common.h"
#include "parcel.h"

namespace OHOS {
namespace AccountSA {
class IDomainAccountCallback : public IRemoteBroker {
public:
    DECLARE_INTERFACE_DESCRIPTOR(u"ohos.accountfwk.IDomainAccountCallback");

    virtual void OnResult(const int32_t errCode, Parcel &parcel) = 0;
};
}  // namespace AccountSA
}  // namespace OHOS
#endif  // OS_ACCOUNT_INTERFACES_INNERKITS_DOMAIN_ACCOUNT_INCLUDE_IDOMAIN_ACCOUNT_CALLBACK_H
