/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
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

#ifndef OS_ACCOUNT_FRAMEWORKS_COMMON_DATABASE_INCLUDE_IACCOUNT_INFO_H
#define OS_ACCOUNT_FRAMEWORKS_COMMON_DATABASE_INCLUDE_IACCOUNT_INFO_H

#include <string>

namespace OHOS {
namespace AccountSA {

class IAccountInfo {
public:
    virtual std::string ToString() const = 0;
    virtual std::string GetPrimeKey() const = 0;
};
}  // namespace AccountSA
}  // namespace OHOS

#endif  // OS_ACCOUNT_FRAMEWORKS_COMMON_DATABASE_INCLUDE_IACCOUNT_INFO_H
