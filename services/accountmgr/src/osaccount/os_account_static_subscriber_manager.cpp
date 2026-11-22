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

#include "os_account_static_subscriber_manager.h"

#include <chrono>
#include <cinttypes>
#include <condition_variable>
#include <dlfcn.h>
#include <pthread.h>
#include <thread>
#include <vector>
#include "account_constants.h"
#include "account_log_wrapper.h"
#include "account_hisysevent_adapter.h"
#include "os_account_state_reply_callback_service.h"
#include "os_account_thread_helper.h"
#include "safe_queue.h"
#ifdef HICOLLIE_ENABLE
#include "account_timer.h"
#include "xcollie/xcollie.h"
#endif // HICOLLIE_ENABLE

namespace OHOS {
namespace AccountSA {

namespace {
#ifdef _ARM64_
static const std::string ROOT_LIB_PATH = "/system/lib64/";
#else
static const std::string ROOT_LIB_PATH = "/system/lib/";
#endif

// Log identifier for a static subscriber: names are unique (duplicate config
// names are rejected at load), so the name alone identifies it; an empty name
// (config missing the field) falls back to the so path.
std::string SubscriberKey(const std::shared_ptr<StaticSubscriber> &subscriber)
{
    return subscriber->name.empty() ? subscriber->path : subscriber->name;
}
}

StaticSubscriber::~StaticSubscriber()
{
    callback = nullptr;
    if (handle != nullptr) {
        dlclose(handle);
        handle = nullptr;
    }
}

OsAccountStaticSubscriberManager::OsAccountStaticSubscriberManager()
{
    std::map<OsAccountState, std::set<std::string>> defaultConfig;
    defaultConfig[OsAccountState::CREATING] = { ROOT_LIB_PATH + "libtheme_manager_client.z.so" };
    Init(defaultConfig);
    // The default subscriber has no config file; name it so log/pending-reply keys
    // identify it like config subscribers. Must precede LoadSubscribers so this
    // name participates in the duplicate-name check.
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        auto it = staticSubscribers_.find(ROOT_LIB_PATH + "libtheme_manager_client.z.so");
        if (it != staticSubscribers_.end()) {
            it->second->name = "theme_manager";
        }
    }
#ifdef OS_ACCOUNT_SUPPORT_STATIC_SUBSCRIPTION
    SubscriberConfigData configs = OsAccountStaticSubscriberConfig::LoadDefault();
    stateTimeouts_ = configs.stateTimeouts;
    if (!configs.subscribers.empty()) {
        LoadSubscribers(configs.subscribers);
    }
#endif
}

OsAccountStaticSubscriberManager &OsAccountStaticSubscriberManager::GetInstance()
{
    static OsAccountStaticSubscriberManager instance;
    return instance;
}

int32_t OsAccountStaticSubscriberManager::GetStateWaitTimeMs(OsAccountState state)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    auto it = stateTimeouts_.find(state);
    return it != stateTimeouts_.end() ? it->second : 0;
}

void OsAccountStaticSubscriberManager::Init(
    const std::map<OsAccountState, std::set<std::string>> &staticSubscriberConfig)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    for (const auto &it : staticSubscriberConfig) {
        std::set<std::shared_ptr<StaticSubscriber>> subscribers;
        for (const auto &path : it.second) {
            auto subscriber = ParseStaticSubscriber(path);
            if (subscriber != nullptr) {
                subscribers.insert(subscriber);
            }
        }
        if (!subscribers.empty()) {
            state2Subscribers_[it.first].insert(subscribers.begin(), subscribers.end());
        }
    }
}

std::shared_ptr<StaticSubscriber> OsAccountStaticSubscriberManager::ParseStaticSubscriber(const std::string &path)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    auto subIt = staticSubscribers_.find(path);
    if (subIt != staticSubscribers_.end()) {
        ACCOUNT_LOGE("Subscriber already exists");
        return subIt->second;
    }
    void* handle = dlopen(path.c_str(), RTLD_LAZY);
    if (handle == nullptr) {
        auto errMsg = dlerror();
        ACCOUNT_LOGE("Failed to dlopen, path: %{public}s, error: %{public}s", path.c_str(), errMsg);
        REPORT_OS_ACCOUNT_FAIL(0, Constants::OPERATION_EVENT_PUBLISH, ERR_ACCOUNT_COMMON_DLOPEN_ERROR,
            "Failed to dlopen, path: " + path + ", error: " + (errMsg ? std::string(errMsg) : ""));
        return nullptr;
    }
    void *func = dlsym(handle, "OnOsAccountStateChanged");
    if (func == nullptr) {
        auto errMsg = dlerror();
        ACCOUNT_LOGE("Failed to dlsym, path: %{public}s, error=%{public}s", path.c_str(), errMsg);
        REPORT_OS_ACCOUNT_FAIL(0, Constants::OPERATION_EVENT_PUBLISH, ERR_ACCOUNT_COMMON_DLSYM_ERROR,
            "Failed to dlsym, path: " + path + ", error: " + (errMsg ? std::string(errMsg) : ""));
        dlclose(handle);
        handle = nullptr;
        return nullptr;
    }
    auto subscriber = std::make_shared<StaticSubscriber>();
    subscriber->path = path;
    subscriber->handle = handle;
    subscriber->callback = func;
    staticSubscribers_[path] = subscriber;
    ACCOUNT_LOGI("Parse static subscriber successfully, path: %{public}s", path.c_str());
    return subscriber;
}

ErrCode OsAccountStaticSubscriberManager::PublishToSubscriber(
    const std::shared_ptr<StaticSubscriber> &subscriber, const COsAccountStateData &data)
{
    if (subscriber == nullptr || subscriber->callback == nullptr) {
        ACCOUNT_LOGE("Invalid subscriber");
        return ERR_ACCOUNT_COMMON_INVALID_PARAMETER;
    }
    const char *handshakeInfo = subscriber->withHandshake ? " with handshake" : "";
    ACCOUNT_LOGI(
        "Static publish %{public}s start, %{public}d->%{public}d on %{public}llu, to %{public}s%{public}s",
        StateName(data.state), data.fromId, data.toId, static_cast<unsigned long long>(data.displayId),
        SubscriberKey(subscriber).c_str(), handshakeInfo);
#ifdef HICOLLIE_ENABLE
    AccountTimer timer;
#endif
    // Callback will be invalid when the singleton destroyed (i.e. the process exits), so lock protection is not added,
    // otherwise it will increase performance overhead
    ErrCode errCode = (*reinterpret_cast<OnOsAccountStateChangedFunc>(subscriber->callback))(&data);
    ACCOUNT_LOGI("Static publish %{public}s end, %{public}d->%{public}d on %{public}llu, to %{public}s"
        "%{public}s, result=%{public}d", StateName(data.state), data.fromId, data.toId,
        static_cast<unsigned long long>(data.displayId), SubscriberKey(subscriber).c_str(),
        handshakeInfo, errCode);
    if (errCode != ERR_OK) {
        REPORT_OS_ACCOUNT_FAIL(data.toId, OsAccountStateReplyCallbackService::ConvertStateToSceneFlag(
            data.state), errCode, std::string("Static publish ") + StateName(data.state) + " failed, to " +
            SubscriberKey(subscriber) + handshakeInfo);
    }
    return errCode;
}

ErrCode OsAccountStaticSubscriberManager::Publish(int32_t fromId, OsAccountState state,
    int32_t toId, uint64_t displayId)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    auto it = state2Subscribers_.find(state);
    if (it == state2Subscribers_.end()) {
        ACCOUNT_LOGI("Static publish %{public}s, no subscriber", StateName(state));
        return ERR_OK;
    }
    for (const std::shared_ptr<StaticSubscriber> &subscriber : it->second) {
        if (subscriber == nullptr) {
            ACCOUNT_LOGE("Subscriber is nullptr");
            continue;
        }
        auto task = [subscriber, state, fromId, toId, displayId] {
            COsAccountStateData data;
            data.fromId = fromId;
            data.state = state;
            data.toId = toId;
            data.displayId = displayId;

            PublishToSubscriber(subscriber, data); // failure report is emitted inside
        };
#ifdef FUZZ_TEST
        task();
#else
        std::thread publishThread(task);
        pthread_setname_np(publishThread.native_handle(), "StaticPublish");
        publishThread.detach();
#endif
    }
    return ERR_OK;
}

ErrCode OsAccountStaticSubscriberManager::SyncPublish(int32_t fromId, OsAccountState state,
    int32_t toId, uint64_t displayId)
{
#ifdef OS_ACCOUNT_SUPPORT_STATIC_SUBSCRIPTION
    COsAccountStateData data;
    data.fromId = fromId;
    data.state = state;
    data.toId = toId;
    data.displayId = displayId;
    std::set<std::shared_ptr<StaticSubscriber>> subscribers;
    std::vector<std::shared_ptr<StaticSubscriber>> nonHandshakeSubscribers;
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        auto it = state2Subscribers_.find(state);
        if (it == state2Subscribers_.end()) {
            ACCOUNT_LOGI("Static publish %{public}s, no subscriber", StateName(state));
            return ERR_OK;
        }
        for (const auto &subscriber : it->second) {
            if (subscriber == nullptr) {
                continue;
            }
            if (subscriber->withHandshake) {
                subscribers.insert(subscriber);
            } else {
                // A non-handshake subscriber still gets the notification, dispatched
                // fire-and-forget (like the legacy async Publish) instead of being dropped.
                nonHandshakeSubscribers.push_back(subscriber);
            }
        }
    }
    for (const auto &subscriber : nonHandshakeSubscribers) {
        auto task = [subscriber, data] { PublishToSubscriber(subscriber, data); }; // failure report inside
        if (!StartDetachedThread(task, "StaticPublish")) {
            ACCOUNT_LOGE("Static publish %{public}s dispatch failed, to %{public}s",
                StateName(data.state), SubscriberKey(subscriber).c_str());
            REPORT_OS_ACCOUNT_FAIL(data.toId, Constants::OPERATION_EVENT_PUBLISH, ERR_ACCOUNT_COMMON_BUSY,
                std::string("Static publish ") + StateName(data.state) + " dispatch failed, to " +
                SubscriberKey(subscriber));
        }
    }
    if (subscribers.empty()) {
        return ERR_OK;
    }
    return SyncPublishToSubscribers(subscribers, data);
#else
    return ERR_OK;
#endif
}

#ifdef OS_ACCOUNT_SUPPORT_STATIC_SUBSCRIPTION
static ErrCode WaitForAllReplies(const std::shared_ptr<std::condition_variable> &cvPtr,
    const std::shared_ptr<std::mutex> &mutexPtr, const std::shared_ptr<SafeQueue<uint8_t>> &safeQueue,
    int32_t waitTimeMs)
{
    std::unique_lock<std::mutex> waitLock(*mutexPtr);
    auto result = cvPtr->wait_for(waitLock, std::chrono::milliseconds(waitTimeMs),
        [safeQueue]() { return safeQueue->Size() == 0; });
    if (!result) {
        return ERR_ACCOUNT_COMMON_OPERATION_TIMEOUT;
    }
    return ERR_OK;
}

bool OsAccountStaticSubscriberManager::IsSubscriberNameExists(const std::string &name)
{
    for (const auto &stateEntry : state2Subscribers_) {
        for (const auto &sub : stateEntry.second) {
            if (sub != nullptr && sub->name == name) {
                return true;
            }
        }
    }
    return false;
}

void OsAccountStaticSubscriberManager::LoadSubscribers(const std::vector<SubscriberConfig> &configs)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    for (const auto &config : configs) {
        if (IsSubscriberNameExists(config.name)) {
            ACCOUNT_LOGE("Duplicate subscriber name: %{public}s", config.name.c_str());
            REPORT_OS_ACCOUNT_FAIL(0, Constants::OPERATION_EVENT_PUBLISH,
                ERR_ACCOUNT_COMMON_INVALID_PARAMETER,
                "Duplicate subscriber name=" + config.name);
            continue;
        }
        std::string fullPath = ROOT_LIB_PATH + config.soPath;
        if (!OsAccountStaticSubscriberConfig::IsPathAllowed(fullPath)) {
            ACCOUNT_LOGE("Invalid so path: %{public}s", fullPath.c_str());
            REPORT_OS_ACCOUNT_FAIL(0, Constants::OPERATION_EVENT_PUBLISH,
                ERR_ACCOUNT_COMMON_INVALID_PARAMETER,
                "Invalid so path in static subscriber config, path=" + fullPath);
            continue;
        }
        auto base = ParseStaticSubscriber(fullPath);
        if (base == nullptr) {
            continue;
        }
        auto subscriber = std::make_shared<StaticSubscriber>();
        subscriber->name = config.name;
        subscriber->path = base->path;
        // The dlopen handle stays owned by the base entry cached in staticSubscribers_
        // (see ParseStaticSubscriber); this entry only borrows the resolved callback and
        // must not close the handle again.
        subscriber->handle = nullptr;
        subscriber->callback = base->callback;
        subscriber->withHandshake = config.withHandshake;
        for (const auto &state : config.states) {
            state2Subscribers_[state].insert(subscriber);
        }
    }
}

static void ReleasePendingReply(const std::shared_ptr<std::mutex> &mutexPtr,
    const std::shared_ptr<SafeQueue<uint8_t>> &safeQueue,
    const std::shared_ptr<std::condition_variable> &cvPtr)
{
    {
        std::lock_guard<std::mutex> lock(*mutexPtr);
        uint8_t tmp;
        safeQueue->Pop(tmp);
    }
    cvPtr->notify_one();
}

// Single place for the timeout report: pending list + log + HiSysEvent.
static void ReportPublishTimeout(const std::shared_ptr<std::mutex> &mutexPtr,
    const std::shared_ptr<std::set<std::string>> &pendingSubscribers,
    const COsAccountStateData &data, int32_t waitTimeMs)
{
    std::string pendingList;
    {
        std::lock_guard<std::mutex> lock(*mutexPtr);
        for (const auto &key : *pendingSubscribers) {
            if (!pendingList.empty()) {
                pendingList += ",";
            }
            pendingList += key;
        }
    }
    ACCOUNT_LOGE("Static publish %{public}s reply timeout, %{public}d->%{public}d on %{public}llu "
        "with handshake, waitMs=%{public}d, pending=%{public}s", StateName(data.state), data.fromId,
        data.toId, static_cast<unsigned long long>(data.displayId), waitTimeMs, pendingList.c_str());
    REPORT_OS_ACCOUNT_FAIL(data.toId, OsAccountStateReplyCallbackService::ConvertStateToSceneFlag(
        data.state), ERR_ACCOUNT_COMMON_OPERATION_TIMEOUT, std::string("Static publish ") + StateName(data.state) +
        " reply timeout with handshake, pending=" + pendingList);
}

ErrCode OsAccountStaticSubscriberManager::SyncPublishToSubscribers(
    const std::set<std::shared_ptr<StaticSubscriber>> &subscribers, const COsAccountStateData &data)
{
    auto cvPtr = std::make_shared<std::condition_variable>();
    auto mutexPtr = std::make_shared<std::mutex>();
    auto safeQueue = std::make_shared<SafeQueue<uint8_t>>();
    // Keys of subscribers whose reply is still pending; the task erases its key before
    // releasing the queue item, so on timeout the set holds exactly the non-responders.
    auto pendingSubscribers = std::make_shared<std::set<std::string>>();
    // The wait budget is the state-level timeout, the same source the dynamic reply wait uses,
    // so the whole publish settles within one configured window (see AwaitPublish). A
    // per-subscriber copy would go stale for subscribers registered on multiple states.
    int32_t maxWaitMs = GetStateWaitTimeMs(data.state);
    for (const auto &subscriber : subscribers) {
        if (subscriber == nullptr || subscriber->callback == nullptr) {
            ACCOUNT_LOGE("Invalid static subscriber");
            continue;
        }
        safeQueue->Push(1);
        // Unique subscriber name; on timeout the set holds exactly the non-responders.
        std::string subscriberKey = SubscriberKey(subscriber);
        {
            std::lock_guard<std::mutex> lock(*mutexPtr);
            pendingSubscribers->insert(subscriberKey);
        }
        auto task = [subscriber, data, cvPtr, mutexPtr, safeQueue, pendingSubscribers, subscriberKey] {
            PublishToSubscriber(subscriber, data); // failure report is emitted inside
            {
                std::lock_guard<std::mutex> lock(*mutexPtr);
                pendingSubscribers->erase(subscriberKey);
            }
            ReleasePendingReply(mutexPtr, safeQueue, cvPtr);
        };
        if (!StartDetachedThread(task, "SyncStaticPub")) {
            ACCOUNT_LOGE("Static publish %{public}s dispatch failed, to %{public}s with handshake",
                StateName(data.state), SubscriberKey(subscriber).c_str());
            REPORT_OS_ACCOUNT_FAIL(data.toId, Constants::OPERATION_EVENT_PUBLISH, ERR_ACCOUNT_COMMON_BUSY,
                std::string("Static publish ") + StateName(data.state) + " dispatch failed, to " +
                SubscriberKey(subscriber) + " with handshake");
            {
                std::lock_guard<std::mutex> lock(*mutexPtr);
                pendingSubscribers->erase(subscriberKey);
            }
            ReleasePendingReply(mutexPtr, safeQueue, cvPtr);
        }
    }
    if (maxWaitMs <= 0) {
        // No timeout configured for this state: dispatch without waiting for replies.
        return ERR_OK;
    }
    ErrCode waitResult = WaitForAllReplies(cvPtr, mutexPtr, safeQueue, maxWaitMs);
    if (waitResult == ERR_ACCOUNT_COMMON_OPERATION_TIMEOUT) {
        ReportPublishTimeout(mutexPtr, pendingSubscribers, data, maxWaitMs);
    }
    return waitResult;
}
#endif
} // AccountSA
} // OHOS
