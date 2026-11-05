/*
 * Copyright (c) 2024-2026 Huawei Device Co., Ltd.
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

#include <pthread.h>
#include <thread>
#include "account_constants.h"
#include "account_info.h"
#include "account_log_wrapper.h"
#include "account_permission_manager.h"
#include "distributed_account_subscribe_death_recipient.h"
#include "distributed_account_subscribe_manager.h"
#include "ipc_skeleton.h"
#include "ohos_account_dfx_constants.h"

namespace OHOS {
namespace AccountSA {
namespace {
#ifndef FUZZ_TEST
const char THREAD_DISTRIBUTED_ACCOUNT_EVENT[] = "distSevEvent";
#endif
}

DistributedAccountSubscribeManager::DistributedAccountSubscribeManager()
    : subscribeDeathRecipient_(sptr<IRemoteObject::DeathRecipient>(
        new (std::nothrow) DistributedAccountSubscribeDeathRecipient()))
{}

DistributedAccountSubscribeManager &DistributedAccountSubscribeManager::GetInstance()
{
    static DistributedAccountSubscribeManager *instance = new (std::nothrow) DistributedAccountSubscribeManager();
    return *instance;
}

ErrCode DistributedAccountSubscribeManager::SubscribeDistributedAccountEvent(
    const DISTRIBUTED_ACCOUNT_SUBSCRIBE_TYPE type, const sptr<IRemoteObject> &eventListener)
{
    ACCOUNT_LOGI("Subscribe distributed account in submanager.");
    if (eventListener == nullptr) {
        ACCOUNT_LOGE("EventListener is nullptr.");
        return ERR_ACCOUNT_COMMON_NULL_PTR_ERROR;
    }
    int32_t callingPid = IPCSkeleton::GetCallingPid();
    std::lock_guard<std::mutex> lock(subscribeRecordMutex_);
    auto it = subscribeRecords_.find(callingPid);
    if (it != subscribeRecords_.end()) {
        const auto &oldListener = it->second->eventListener_;
        if (oldListener == eventListener) {
            it->second->types_.insert(type);
            ACCOUNT_LOGI("Subscribe already exists, update type only, type size=%{public}zu.",
                it->second->types_.size());
            return ERR_OK;
        }
        // kit always sends DistributedAccountEventService::GetInstance()->AsObject() (process-level singleton),
        // so oldListener == eventListener for kit-originated calls and this branch is not reached.
        // This branch only applies to callers that bypass the kit and send a different listener.
        if (oldListener != nullptr && !oldListener->IsObjectDead()) {
            ACCOUNT_LOGE("CallingPid=%{public}d already has a different eventListener.", callingPid);
            return ERR_OHOSACCOUNT_KIT_CALLBACK_ALREADY_REGISTERED_ERROR;
        }
    }

    auto subscribeRecordPtr = std::make_shared<DistributedSubscribeRecord>(eventListener);
    if (it != subscribeRecords_.end()) {
        const auto &oldListener = it->second->eventListener_;
        if (oldListener != nullptr && subscribeDeathRecipient_ != nullptr) {
            oldListener->RemoveDeathRecipient(subscribeDeathRecipient_);
        }
        ACCOUNT_LOGI("CallingPid=%{public}d stale record cleaned, re-subscribe.", callingPid);
        subscribeRecords_.erase(it);
    }
    if (subscribeDeathRecipient_ != nullptr) {
        eventListener->AddDeathRecipient(subscribeDeathRecipient_);
    }
    subscribeRecordPtr->types_.insert(type);
    ACCOUNT_LOGI("Subscribe add, callingPid=%{public}d, type size=%{public}zu.", callingPid,
        subscribeRecordPtr->types_.size());
    subscribeRecords_[callingPid] = subscribeRecordPtr;
    return ERR_OK;
}

ErrCode DistributedAccountSubscribeManager::UnsubscribeDistributedAccountEvent(
    const DISTRIBUTED_ACCOUNT_SUBSCRIBE_TYPE type, const sptr<IRemoteObject> &eventListener)
{
    ACCOUNT_LOGI("Unsubscribe distributed account in submanager.");
    if (eventListener == nullptr) {
        ACCOUNT_LOGE("EventListener is nullptr.");
        return ERR_ACCOUNT_COMMON_NULL_PTR_ERROR;
    }
    int32_t callingPid = IPCSkeleton::GetCallingPid();
    std::lock_guard<std::mutex> lock(subscribeRecordMutex_);
    auto it = subscribeRecords_.find(callingPid);
    if (it == subscribeRecords_.end() || it->second->eventListener_ != eventListener) {
        ACCOUNT_LOGE("Unsubscribe failed, subscribe record not found, callingPid=%{public}d.", callingPid);
        return ERR_OHOSACCOUNT_KIT_NO_SPECIFIED_CALLBACK_HAS_BEEN_REGISTERED;
    }
    it->second->types_.erase(type);
    if (!it->second->types_.empty()) {
        return ERR_OK;
    }
    if (subscribeDeathRecipient_ != nullptr) {
        eventListener->RemoveDeathRecipient(subscribeDeathRecipient_);
    }
    subscribeRecords_.erase(it);
    return ERR_OK;
}

ErrCode DistributedAccountSubscribeManager::UnsubscribeDistributedAccountEvent(
    const sptr<IRemoteObject> &eventListener)
{
    ACCOUNT_LOGI("Unsubscribe distributed account in submanager.");
    if (eventListener == nullptr) {
        ACCOUNT_LOGE("EventListener is nullptr.");
        return ERR_ACCOUNT_COMMON_NULL_PTR_ERROR;
    }

    std::lock_guard<std::mutex> lock(subscribeRecordMutex_);
    if (subscribeDeathRecipient_ != nullptr) {
        eventListener->RemoveDeathRecipient(subscribeDeathRecipient_);
    }
    bool found = false;
    for (auto it = subscribeRecords_.begin(); it != subscribeRecords_.end(); ++it) {
        if (eventListener == it->second->eventListener_) {
            subscribeRecords_.erase(it);
            found = true;
            break;
        }
    }
    if (found) {
        ACCOUNT_LOGI("Unsubscribe by death recipient, record erased.");
    } else {
        ACCOUNT_LOGI("Unsubscribe by death recipient, no matching record.");
    }
    return ERR_OK;
}

/**
 * @brief Send distributed account event notification via IPC.
 * @param ipcCall IPC call function to execute.
 * @param logPrefix Log message prefix for error reporting.
 * @return ErrCode result of the IPC call.
 */
template<typename Func>
ErrCode SendDistributedAccountEventNotify(Func ipcCall, const std::string& logPrefix)
{
    int32_t retryTimes = 0;
    ErrCode result = ERR_OK;
    while (retryTimes < Constants::MAX_RETRY_TIMES) {
        result = ipcCall();
        if (result == ERR_OK || (result != Constants::E_IPC_ERROR &&
            result != Constants::E_IPC_SA_DIED)) {
            break;
        }
        retryTimes++;
        ACCOUNT_LOGE("%{public}s failed, code=%{public}d, retryTimes=%{public}d",
            logPrefix.c_str(), result, retryTimes);
        std::this_thread::sleep_for(std::chrono::milliseconds(Constants::DELAY_FOR_EXCEPTION));
    }
    return result;
}

/**
 * @brief Execute distributed account event callback notification asynchronously in a detached thread.
 * @param task Task function to execute asynchronously.
 * @note In FUZZ_TEST mode, task is executed synchronously.
 */
void ExecuteDistributedAccountEventNotifyAsync(std::function<void()> task)
{
#ifdef FUZZ_TEST
    task();
#else
    std::thread taskThread(task);
    pthread_setname_np(taskThread.native_handle(), THREAD_DISTRIBUTED_ACCOUNT_EVENT);
    taskThread.detach();
#endif
}

bool DistributedAccountSubscribeManager::OnAccountsChanged(
    const sptr<IDistributedAccountEvent> &eventProxy, const int id, DISTRIBUTED_ACCOUNT_SUBSCRIBE_TYPE subscribeType,
    const int32_t subProfileId)
{
    if (eventProxy == nullptr) {
        ACCOUNT_LOGE("Get app account event proxy failed.");
        return false;
    }
    DistributedAccountEventData eventData;
    eventData.id_ = id;
    eventData.type_ = subscribeType;
    eventData.subspaceId_ = subProfileId;
    ErrCode result = SendDistributedAccountEventNotify(
        [&]() { return eventProxy->OnAccountsChanged(eventData); }, "Send distributed account event request");
    if (result != ERR_OK) {
        ACCOUNT_LOGE("SendRequest for account changed failed, result=%{public}d eventData.id=%{public}d.",
            result, eventData.id_);
        REPORT_OHOS_ACCOUNT_FAIL(eventData.id_, Constants::OPERATION_EVENT_PUBLISH,
            result, "Send OnAccountsChanged failed.");
        return false;
    }
    return true;
}

ErrCode DistributedAccountSubscribeManager::Publish(const int id, DISTRIBUTED_ACCOUNT_SUBSCRIBE_TYPE subscribeType,
    int32_t subProfileId)
{
    std::lock_guard<std::mutex> lock(subscribeRecordMutex_);
    uint32_t sendCnt = 0;
    for (auto it = subscribeRecords_.begin(); it != subscribeRecords_.end(); ++it) {
        if (it->second->types_.find(subscribeType) != it->second->types_.end()) {
            auto eventProxy = iface_cast<IDistributedAccountEvent>(it->second->eventListener_);
            if (eventProxy == nullptr) {
                ACCOUNT_LOGE("Get eventProxy failed");
                continue;
            }
            auto task = [this, eventProxy, id, subscribeType, subProfileId] {
                this->OnAccountsChanged(eventProxy, id, subscribeType, subProfileId);
            };
            ExecuteDistributedAccountEventNotifyAsync(task);
            ++sendCnt;
        }
    }
    if (sendCnt == 0) {
        ACCOUNT_LOGW("Publish DistributedAccountEvent %{public}d no subscriber dispatched, id=%{public}d.",
            subscribeType, id);
    } else {
        ACCOUNT_LOGI("Publish DistributedAccountEvent %{public}d succeed, id=%{public}d, sendCnt=%{public}u.",
            subscribeType, id, sendCnt);
    }
    return ERR_OK;
}
}  // namespace AccountSA
}  // namespace OHOS
