/*
 * Copyright (c) 2021-2025 Huawei Device Co., Ltd.
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

#ifndef OS_ACCOUNT_SERVICES_ACCOUNTMGR_INCLUDE_IOS_ACCOUNT_SUBSCRIBE_H
#define OS_ACCOUNT_SERVICES_ACCOUNTMGR_INCLUDE_IOS_ACCOUNT_SUBSCRIBE_H

#include <optional>
#include <future>
#include "account_error_no.h"
#include "os_account_subscribe_info.h"
#include "iremote_object.h"

namespace OHOS {
namespace AccountSA {
struct OsSubscribeRecord {
    std::shared_ptr<OsAccountSubscribeInfo> subscribeInfoPtr_;
    sptr<IRemoteObject> eventListener_;
    int32_t callingUid_;

    OsSubscribeRecord() : subscribeInfoPtr_(nullptr), eventListener_(nullptr), callingUid_(-1)
    {}
    OsSubscribeRecord(std::shared_ptr<OsAccountSubscribeInfo> subscribeInfoPtr, sptr<IRemoteObject> eventListener,
        int32_t callingUid)
        : subscribeInfoPtr_(subscribeInfoPtr), eventListener_(eventListener), callingUid_(callingUid)
    {}
};

using OsSubscribeRecordPtr = std::shared_ptr<OsSubscribeRecord>;

struct OsAccountConstraintSubscribeRecord {
    std::set<std::string> constraintSet_;
    sptr<IRemoteObject> eventListener_;
    int32_t callingUid_;

    OsAccountConstraintSubscribeRecord() : eventListener_(nullptr), callingUid_(-1)
    {}
    OsAccountConstraintSubscribeRecord(const std::set<std::string> &constraints,
        const sptr<IRemoteObject> eventListener, int32_t callingUid)
        : constraintSet_(constraints), eventListener_(eventListener), callingUid_(callingUid)
    {}
};

using OsAccountConstraintSubscribeRecordPtr = std::shared_ptr<OsAccountConstraintSubscribeRecord>;

class IOsAccountSubscribe {
public:
    virtual ErrCode SubscribeOsAccount(
        const std::shared_ptr<OsAccountSubscribeInfo> &subscribeInfoPtr, const sptr<IRemoteObject> &eventListener) = 0;
    virtual ErrCode UnsubscribeOsAccount(const sptr<IRemoteObject> &eventListener) = 0;
    virtual const std::shared_ptr<OsAccountSubscribeInfo> GetSubscribeRecordInfo(
        const sptr<IRemoteObject> &eventListener) = 0;
    // Single-account event: no counterpart account, the id fills the from/to pair so dispatch
    // logs and subscriber parcels stay consistent.
    virtual std::future<ErrCode> Publish(int32_t id, OsAccountState state) = 0;
    virtual std::future<ErrCode> Publish(int32_t fromId, OsAccountState state, int32_t toId,
        std::optional<uint64_t> displayId = std::nullopt) = 0;
    // Publish and wait the bounded reply window, so callers never touch the bare future.
    virtual ErrCode PublishSync(int32_t id, OsAccountState state) = 0;
    virtual void AwaitPublish(std::future<ErrCode> &notifyFuture, OsAccountState state, int32_t fromId,
        int32_t toId, uint64_t displayId) = 0;
};
}  // namespace AccountSA
}  // namespace OHOS

#endif  // OS_ACCOUNT_SERVICES_ACCOUNTMGR_INCLUDE_IOS_ACCOUNT_SUBSCRIBE_H
