/*
 * Copyright (c) 2021-2024 Huawei Device Co., Ltd.
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

#ifndef OS_ACCOUNT_INTERFACES_INNERKITS_OSACCOUNT_NATIVE_INCLUDE_OS_ACCOUNT_SUBSCRIBE_INFO_H
#define OS_ACCOUNT_INTERFACES_INNERKITS_OSACCOUNT_NATIVE_INCLUDE_OS_ACCOUNT_SUBSCRIBE_INFO_H

#include <set>
#include <string>
#include "account_error_no.h"
#include "parcel.h"

namespace OHOS {
namespace AccountSA {
typedef enum {
    INVALID_TYPE = -1,
    ACTIVATED = 0,
    ACTIVATING,
    UNLOCKED,
    CREATED,
    REMOVED,
    STOPPING,
    STOPPED,
    SWITCHING,
    SWITCHED,
    CREATING,
    LOCKING,
    LOCKED,
} OS_ACCOUNT_SUBSCRIBE_TYPE, OsAccountState;

#ifdef __cplusplus
extern "C" {
#endif
struct COsAccountStateData {
    OsAccountState state = OsAccountState::INVALID_TYPE;
    int32_t fromId = -1;
    int32_t toId = -1;
    uint64_t displayId = 0;
};

typedef int32_t (*OnOsAccountStateChangedFunc)(const COsAccountStateData *stateData);
#ifdef __cplusplus
}
#endif

/**
 * Convert an OS account state to its enum name for logging, e.g. SWITCHING.
 */
inline const char *StateName(OsAccountState state)
{
    switch (state) {
        case OsAccountState::ACTIVATED:
            return "activated";
        case OsAccountState::ACTIVATING:
            return "activating";
        case OsAccountState::UNLOCKED:
            return "unlocked";
        case OsAccountState::CREATED:
            return "created";
        case OsAccountState::REMOVED:
            return "removed";
        case OsAccountState::STOPPING:
            return "stopping";
        case OsAccountState::STOPPED:
            return "stopped";
        case OsAccountState::SWITCHING:
            return "switching";
        case OsAccountState::SWITCHED:
            return "switched";
        case OsAccountState::CREATING:
            return "creating";
        case OsAccountState::LOCKING:
            return "locking";
        case OsAccountState::LOCKED:
            return "locked";
        default:
            return "unknown";
    }
}

/**
 * Check whether the subscription handshake is required for the given OS account state.
 * The publisher attaches a reply callback only for these states, and the subscriber
 * waits for its callbacks before replying. Both sides share this single definition.
 */
inline bool IsStateNeedHandshake(OsAccountState state)
{
    switch (state) {
        case OsAccountState::STOPPING:
        case OsAccountState::LOCKING:
            return true;
#ifdef OS_ACCOUNT_SUPPORT_STATIC_SUBSCRIPTION
        case OsAccountState::SWITCHING:
            return true;
#endif
        default:
            return false;
    }
}

class OsAccountSubscribeInfo : public Parcelable {
public:
    OsAccountSubscribeInfo();
    OsAccountSubscribeInfo(const std::set<OsAccountState> &states, bool withHandshake = false);
    OsAccountSubscribeInfo(const OS_ACCOUNT_SUBSCRIBE_TYPE &osAccountSubscribeType, const std::string &name);
    ~OsAccountSubscribeInfo() override;

    void GetOsAccountSubscribeType(OS_ACCOUNT_SUBSCRIBE_TYPE &osAccountSubscribeType) const;
    void SetOsAccountSubscribeType(const OS_ACCOUNT_SUBSCRIBE_TYPE &osAccountSubscribeType);
    void GetName(std::string &name) const;
    void SetName(const std::string &name);
    void GetStates(std::set<OsAccountState> &states) const;
    bool IsWithHandshake() const;

    bool Marshalling(Parcel &parcel) const override;
    static OsAccountSubscribeInfo *Unmarshalling(Parcel &parcel);

private:
    bool ReadFromParcel(Parcel &parcel);

private:
    OS_ACCOUNT_SUBSCRIBE_TYPE osAccountSubscribeType_ = INVALID_TYPE;
    std::string name_;
    std::set<OsAccountState> states_;
    bool withHandshake_ = false;
};
}  // namespace AccountSA
}  // namespace OHOS

#endif  // OS_ACCOUNT_INTERFACES_INNERKITS_OSACCOUNT_NATIVE_INCLUDE_OS_ACCOUNT_SUBSCRIBE_INFO_H
