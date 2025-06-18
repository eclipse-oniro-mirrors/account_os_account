/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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

#ifndef OS_ACCOUNT_INTERFACES_INNERKITS_OSACCOUNT_NATIVE_INCLUDE_CONSTRAINT_SUBSCRIBE_INFO_H
#define OS_ACCOUNT_INTERFACES_INNERKITS_OSACCOUNT_NATIVE_INCLUDE_CONSTRAINT_SUBSCRIBE_INFO_H

#include <set>
#include <string>
#include "parcel.h"

namespace OHOS {
namespace AccountSA {
class OsAccountConstraintSubscribeInfo : public Parcelable {
public:
    OsAccountConstraintSubscribeInfo();
    OsAccountConstraintSubscribeInfo(const std::set<std::string> &constraints);
    OsAccountConstraintSubscribeInfo& operator=(const OsAccountConstraintSubscribeInfo&) = default;
    ~OsAccountConstraintSubscribeInfo() = default;
    void SetConstraints(const std::set<std::string> &constraints);
    void AddConstraints(const std::set<std::string> &constraints);
    void GetConstraints(std::set<std::string> &constraints) const;
    bool IsConstraintSetEmpty();
    bool Marshalling(Parcel &parcel) const override;
    static OsAccountConstraintSubscribeInfo *Unmarshalling(Parcel &parcel);

private:
    bool ReadFromParcel(Parcel &parcel);
private:
    std::set<std::string> constraintSet_;
};
}  // namespace AccountSA
}  // namespace OHOS
#endif // OS_ACCOUNT_INTERFACES_INNERKITS_OSACCOUNT_NATIVE_INCLUDE_CONSTRAINT_SUBSCRIBE_INFO_H