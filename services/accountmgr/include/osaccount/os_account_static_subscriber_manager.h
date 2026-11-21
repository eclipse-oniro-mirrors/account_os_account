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

#ifndef OS_ACCOUNT_SERVICES_ACCOUNTMGR_INCLUDE_OS_ACCOUNT_STATIC_SUBSCRIBER_MANAGER_H
#define OS_ACCOUNT_SERVICES_ACCOUNTMGR_INCLUDE_OS_ACCOUNT_STATIC_SUBSCRIBER_MANAGER_H

#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>
#include "account_error_no.h"
#include "os_account_static_subscriber_config.h"
#include "os_account_subscribe_info.h"

namespace OHOS {
namespace AccountSA {
struct StaticSubscriber {
    ~StaticSubscriber();
    std::string name;
    std::string path;
    void *handle = nullptr;
    void *callback = nullptr;
    bool withHandshake = false;
};

class OsAccountStaticSubscriberManager {
public:
    static OsAccountStaticSubscriberManager& GetInstance();
    ErrCode Publish(int32_t fromId, OsAccountState state, int32_t toId, uint64_t displayId = 0);
    ErrCode SyncPublish(int32_t fromId, OsAccountState state, int32_t toId, uint64_t displayId = 0);
    int32_t GetStateWaitTimeMs(OsAccountState state);

private:
    OsAccountStaticSubscriberManager();
    ~OsAccountStaticSubscriberManager() = default;
    void Init(const std::map<OsAccountState, std::set<std::string>> &staticSubscriberConfig);
    std::shared_ptr<StaticSubscriber> ParseStaticSubscriber(const std::string &path);
#ifdef OS_ACCOUNT_SUPPORT_STATIC_SUBSCRIPTION
    void LoadSubscribers(const std::vector<SubscriberConfig> &configs);
    bool IsSubscriberNameExists(const std::string &name);
    ErrCode SyncPublishToSubscribers(const std::set<std::shared_ptr<StaticSubscriber>> &subscribers,
        const COsAccountStateData &data);
#endif
    static ErrCode PublishToSubscriber(
        const std::shared_ptr<StaticSubscriber> &subscriber, const COsAccountStateData &data);
    DISALLOW_COPY_AND_MOVE(OsAccountStaticSubscriberManager);

private:
    std::recursive_mutex mutex_;
    std::map<std::string, std::shared_ptr<StaticSubscriber>> staticSubscribers_;
    std::map<OsAccountState, std::set<std::shared_ptr<StaticSubscriber>>> state2Subscribers_;
    std::map<OsAccountState, int32_t> stateTimeouts_;
};
} // AccountSA
} // OHOS
#endif // OS_ACCOUNT_SERVICES_ACCOUNTMGR_INCLUDE_OS_ACCOUNT_STATIC_SUBSCRIBER_MANAGER_H
