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

#include "os_account_subscribe_manager.h"
#include <pthread.h>
#include <algorithm>
#include <chrono>
#include <cinttypes>
#include <thread>
#include "account_constants.h"
#include "account_hisysevent_adapter.h"
#include "account_log_wrapper.h"
#include "ipc_skeleton.h"
#include "os_account_constants.h"
#include "os_account_state_parcel.h"
#include "os_account_state_reply_callback_service.h"
#include "os_account_static_subscriber_config.h"
#include "os_account_static_subscriber_manager.h"
#include "os_account_subscribe_death_recipient.h"
#include "os_account_thread_helper.h"
#ifdef HICOLLIE_ENABLE
#include "account_timer.h"
#include "xcollie/xcollie.h"
#endif // HICOLLIE_ENABLE

namespace OHOS {
namespace AccountSA {

namespace {
const char THREAD_OS_ACCOUNT_EVENT[] = "osAccountEvent";
constexpr int32_t DEACTIVATION_WAIT_SECONDS = 5;
}

SwitchSubcribeWork::SwitchSubcribeWork(const sptr<IOsAccountEvent> &eventProxy,
    const OsAccountStateParcel &stateParcel)
{
    eventProxy_ = eventProxy;
    stateParcel_ = stateParcel;
}

SwitchSubscribeInfo::SwitchSubscribeInfo(OS_ACCOUNT_SUBSCRIBE_TYPE osAccountSubscribeType)
{
    if (osAccountSubscribeType == OS_ACCOUNT_SUBSCRIBE_TYPE::SWITCHING ||
        osAccountSubscribeType == OS_ACCOUNT_SUBSCRIBE_TYPE::SWITCHED) {
        count_ = 1;
    }
}

SwitchSubscribeInfo::~SwitchSubscribeInfo()
{
}

void SwitchSubscribeInfo::AddSubscribeInfo()
{
    count_++;
}

bool SwitchSubscribeInfo::SubSubscribeInfo()
{
    if (count_ == 0) {
        return false;
    }
    count_--;
    return true;
}

bool SwitchSubscribeInfo::IsEmpty()
{
    return count_ == 0;
}

// Shared reply-callback tail of the two dispatch paths: mark the reply start and, on IPC
// failure, release the pending reply so the publisher's wait cannot hang on a lost reply.
static void CompleteReplyCallback(const OsAccountStateParcel &stateParcel, ErrCode errCode)
{
    auto callback = static_cast<OsAccountStateReplyCallbackService *>(stateParcel.callback.GetRefPtr());
    if (callback == nullptr) {
        return;
    }
    callback->SetStartTime(std::chrono::system_clock::now());
    if (errCode != ERR_OK) {
        callback->ForceComplete();
    }
}

static void HandleSwitchReplyCallback(const OsAccountStateParcel &stateParcel, ErrCode errCode)
{
    if (errCode != ERR_OK) {
        auto callback = static_cast<OsAccountStateReplyCallbackService *>(stateParcel.callback.GetRefPtr());
        if (callback != nullptr) {
            ACCOUNT_LOGE("Dynamic publish %{public}s ipc failed, %{public}d->%{public}d, to %{public}d "
                "with handshake, result=%{public}d", StateName(stateParcel.state), stateParcel.fromId,
                stateParcel.toId, callback->GetSubscriberUid(), errCode);
            REPORT_OS_ACCOUNT_FAIL(stateParcel.toId, Constants::OPERATION_EVENT_PUBLISH, errCode,
                std::string("Dynamic publish ") + StateName(stateParcel.state) + " ipc failed, to " +
                std::to_string(callback->GetSubscriberUid()) + " with handshake");
        }
    }
    CompleteReplyCallback(stateParcel, errCode);
}

static void ConsumerTask(std::weak_ptr<SwitchSubscribeInfo> weakInfo)
{
    bool exitFlag = false;
    while (!exitFlag) {
        std::shared_ptr<SwitchSubcribeWork> work;
        auto shareInfo = weakInfo.lock();
        if (shareInfo == nullptr) {
            return;
        }
        std::unique_lock<std::mutex> lock(shareInfo->mutex_);
        if (shareInfo->workDeque_.empty()) {
            shareInfo->workThread_.reset();
            return;
        }
        work = shareInfo->workDeque_.front();
        shareInfo->workDeque_.pop_front();
        lock.unlock();
#ifdef HICOLLIE_ENABLE
        int32_t timerId = HiviewDFX::XCollie::GetInstance().SetTimer(TIMER_NAME,
            TIMEOUT, nullptr, nullptr, HiviewDFX::XCOLLIE_FLAG_LOG);
#endif // HICOLLIE_ENABLE
        if (work != nullptr && work->eventProxy_ != nullptr) {
            ErrCode errCode = work->eventProxy_->OnStateChanged(work->stateParcel_);
            HandleSwitchReplyCallback(work->stateParcel_, errCode);
        }
#ifdef HICOLLIE_ENABLE
        HiviewDFX::XCollie::GetInstance().CancelTimer(timerId);
#endif // HICOLLIE_ENABLE
    }
}

bool SwitchSubscribeInfo::ProductTask(const sptr<IOsAccountEvent> &eventProxy, OsAccountStateParcel &stateParcel)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto work = std::make_shared<SwitchSubcribeWork>(eventProxy, stateParcel);
    workDeque_.push_back(work);
    if (workThread_ == nullptr) {
#ifdef FUZZ_TEST
        ConsumerTask(weak_from_this());
#else
        workThread_ = std::make_unique<std::thread>(&ConsumerTask, weak_from_this());
        pthread_setname_np(workThread_->native_handle(), THREAD_OS_ACCOUNT_EVENT);
        workThread_->detach();
#endif
    }
    return true;
}

OsAccountSubscribeManager::OsAccountSubscribeManager()
    : subscribeDeathRecipient_(sptr<IRemoteObject::DeathRecipient>(
        new (std::nothrow) OsAccountSubscribeDeathRecipient()))
{}

OsAccountSubscribeManager &OsAccountSubscribeManager::GetInstance()
{
    static OsAccountSubscribeManager *instance = new (std::nothrow) OsAccountSubscribeManager();
    return *instance;
}

ErrCode OsAccountSubscribeManager::SubscribeOsAccount(
    const std::shared_ptr<OsAccountSubscribeInfo> &subscribeInfoPtr, const sptr<IRemoteObject> &eventListener)
{
    if (subscribeInfoPtr == nullptr) {
        ACCOUNT_LOGE("SubscribeInfoPtr is nullptr");
        return ERR_ACCOUNT_COMMON_NULL_PTR_ERROR;
    }

    if (eventListener == nullptr) {
        ACCOUNT_LOGE("EventListener is nullptr");
        return ERR_ACCOUNT_COMMON_NULL_PTR_ERROR;
    }
    std::lock_guard<std::mutex> lock(subscribeRecordMutex_);
    auto it = std::find_if(subscribeRecords_.begin(), subscribeRecords_.end(),
        [eventListener] (const OsSubscribeRecordPtr &record) {
            return eventListener == record->eventListener_;
        });
    int32_t callingUid = IPCSkeleton::GetCallingUid();
    std::set<OsAccountState> states;
    subscribeInfoPtr->GetStates(states);
    if (it != subscribeRecords_.end()) {
        std::set<OsAccountState> tmpStates;
        (*it)->subscribeInfoPtr_->GetStates(tmpStates);
        if (states == tmpStates) {
            int32_t callingPid = IPCSkeleton::GetCallingRealPid();
            ACCOUNT_LOGI("EventListener(%{public}d) already exists.", callingPid);
        } else {
            (*it)->subscribeInfoPtr_ = subscribeInfoPtr;
        }
    } else {
        auto subscribeRecordPtr = std::make_shared<OsSubscribeRecord>(subscribeInfoPtr, eventListener, callingUid);
        if (subscribeRecordPtr == nullptr) {
            ACCOUNT_LOGE("SubscribeRecordPtr is nullptr");
            return ERR_ACCOUNT_COMMON_NULL_PTR_ERROR;
        }
        if (subscribeDeathRecipient_ != nullptr) {
            eventListener->AddDeathRecipient(subscribeDeathRecipient_);
        }
        subscribeRecords_.insert(subscribeRecordPtr);
    }
    ACCOUNT_LOGI("SubscribeOsAccount status size=%{public}zu.", states.size());
    if (states.find(SWITCHING) != states.end() || states.find(SWITCHED) != states.end()) {
        if (switchRecordMap_.count(callingUid) != 0) {
            switchRecordMap_[callingUid]->AddSubscribeInfo();
            return ERR_OK;
        }
        switchRecordMap_.emplace(callingUid, std::make_shared<SwitchSubscribeInfo>());
    }
    return ERR_OK;
}

ErrCode OsAccountSubscribeManager::UnsubscribeOsAccount(const sptr<IRemoteObject> &eventListener)
{
    if (eventListener == nullptr) {
        ACCOUNT_LOGE("EventListener is nullptr");
        return ERR_ACCOUNT_COMMON_NULL_PTR_ERROR;
    }
    std::lock_guard<std::mutex> lock(subscribeRecordMutex_);
    if (subscribeDeathRecipient_ != nullptr) {
        eventListener->RemoveDeathRecipient(subscribeDeathRecipient_);
    }

    return RemoveSubscribeRecord(eventListener);
}

ErrCode OsAccountSubscribeManager::RemoveSubscribeRecord(const sptr<IRemoteObject> &eventListener)
{
    for (auto it = subscribeRecords_.begin(); it != subscribeRecords_.end();) {
        if (eventListener == (*it)->eventListener_) {
            ACCOUNT_LOGI("UnsubscribeOsAccount eventListener.");
            (*it)->eventListener_ = nullptr;
            int32_t callingUid = (*it)->callingUid_;
            std::set<OsAccountState> states;
            (*it)->subscribeInfoPtr_->GetStates(states);
            it = subscribeRecords_.erase(it);
            if (states.find(SWITCHING) == states.end() && states.find(SWITCHED) == states.end()) {
                break;
            }
            if (switchRecordMap_.count(callingUid) == 0) {
                break;
            }
            switchRecordMap_[callingUid]->SubSubscribeInfo();
            if (switchRecordMap_[callingUid]->IsEmpty()) {
                switchRecordMap_.erase(callingUid);
            }
            break;
        } else {
            ++it;
        }
    }
    return ERR_OK;
}

const std::shared_ptr<OsAccountSubscribeInfo> OsAccountSubscribeManager::GetSubscribeRecordInfo(
    const sptr<IRemoteObject> &eventListener)
{
    if (eventListener == nullptr) {
        ACCOUNT_LOGE("EventListener is nullptr");
        return nullptr;
    }

    for (auto it = subscribeRecords_.begin(); it != subscribeRecords_.end(); ++it) {
        if (eventListener == (*it)->eventListener_) {
            return (*it)->subscribeInfoPtr_;
        }
    }

    return nullptr;
}

std::string OsAccountSubscribeManager::FormatStateInfo(const OsAccountStateParcel& stateParcel,
    int32_t targetUid, const std::string& phase, ErrCode errCode) const
{
    std::string info = std::string("Dynamic publish ") + StateName(stateParcel.state) + " " + phase +
        ", " + std::to_string(stateParcel.fromId) + "->" + std::to_string(stateParcel.toId);
    if ((stateParcel.state == SWITCHING || stateParcel.state == SWITCHED) &&
        stateParcel.displayId.has_value()) {
        info += " on " + std::to_string(stateParcel.displayId.value());
    }
    info += ", to " + std::to_string(targetUid);
    if (stateParcel.callback != nullptr) {
        info += " with handshake";
    }
    if (phase == "end") {
        info += ", result=" + std::to_string(errCode);
    }
    return info;
}

void OsAccountSubscribeManager::LogPublishEvent(const OsAccountStateParcel& stateParcel,
    int32_t targetUid, const std::string& phase, ErrCode errCode) const
{
    std::string logInfo = FormatStateInfo(stateParcel, targetUid, phase, errCode);
    ACCOUNT_LOGI("%{public}s", logInfo.c_str());
}

bool OsAccountSubscribeManager::OnStateChanged(
    const sptr<IOsAccountEvent> &eventProxy, OsAccountStateParcel &stateParcel, int32_t targetUid)
{
    if (stateParcel.state == SWITCHING || stateParcel.state == SWITCHED) {
        if (switchRecordMap_.count(targetUid) == 0) {
            return false;
        }
        return switchRecordMap_[targetUid]->ProductTask(eventProxy, stateParcel);
    }
    auto task = [this, eventProxy, stateParcel, targetUid]() {
        LogPublishEvent(stateParcel, targetUid, "start");
        ErrCode errCode = eventProxy->OnStateChanged(stateParcel);
        LogPublishEvent(stateParcel, targetUid, "end", errCode);
        if (errCode != ERR_OK) {
            std::string errorMsg = FormatStateInfo(stateParcel, targetUid, "failed", errCode);
            REPORT_OS_ACCOUNT_FAIL(stateParcel.toId, Constants::OPERATION_EVENT_PUBLISH, errCode, errorMsg);
        }
        CompleteReplyCallback(stateParcel, errCode);
    };
    if (!StartDetachedThread(task, THREAD_OS_ACCOUNT_EVENT)) {
        ACCOUNT_LOGE("Dynamic publish %{public}s dispatch failed, %{public}d->%{public}d, to %{public}d",
            StateName(stateParcel.state), stateParcel.fromId, stateParcel.toId, targetUid);
        REPORT_OS_ACCOUNT_FAIL(stateParcel.toId, Constants::OPERATION_EVENT_PUBLISH, ERR_ACCOUNT_COMMON_BUSY,
            std::string("Dynamic publish ") + StateName(stateParcel.state) + " dispatch failed, to " +
            std::to_string(targetUid));
        return false;
    }
    return true;
}

bool OsAccountSubscribeManager::OnStateChangedV0(const sptr<IOsAccountEvent> &eventProxy,
    OsAccountState state, int32_t fromId, int32_t toId, int32_t targetUid, const std::optional<uint64_t> &displayId)
{
    if (state == SWITCHING || state == SWITCHED) {
        if (switchRecordMap_.count(targetUid) == 0) {
            return false;
        }
        OsAccountStateParcel stateParcel;
        stateParcel.fromId = fromId;
        stateParcel.toId = toId;
        stateParcel.state = state;
        stateParcel.displayId = displayId;
        return switchRecordMap_[targetUid]->ProductTask(eventProxy, stateParcel);
    }
    return OnAccountsChanged(eventProxy, state, fromId, targetUid);
}

bool OsAccountSubscribeManager::OnAccountsChanged(
    const sptr<IOsAccountEvent> &eventProxy, OsAccountState state, int32_t id, int32_t targetUid)
{
    auto task = [eventProxy, state, id, targetUid] {
        ACCOUNT_LOGI("Dynamic publish %{public}s start, %{public}d, to %{public}d",
            StateName(state), id, targetUid);
        eventProxy->OnAccountsChanged(id);
        ACCOUNT_LOGI("Dynamic publish %{public}s end, %{public}d, to %{public}d",
            StateName(state), id, targetUid);
    };
    if (!StartDetachedThread(task, THREAD_OS_ACCOUNT_EVENT)) {
        ACCOUNT_LOGE("Dynamic publish %{public}s dispatch failed, %{public}d, to %{public}d",
            StateName(state), id, targetUid);
        REPORT_OS_ACCOUNT_FAIL(id, Constants::OPERATION_EVENT_PUBLISH, ERR_ACCOUNT_COMMON_BUSY,
            std::string("Dynamic publish ") + StateName(state) + " dispatch failed, to " +
            std::to_string(targetUid));
        return false;
    }
    return true;
}

OsAccountSubscribeManager::StatePublishToken OsAccountSubscribeManager::EnqueueStateChange(
    int32_t fromId, OsAccountState state, int32_t toId, std::optional<uint64_t> displayId)
{
    StatePublishToken token;
    token.mutexPtr = std::make_shared<std::mutex>();
    token.cvPtr = std::make_shared<std::condition_variable>();
    token.safeQueue = std::make_shared<SafeQueue<uint8_t>>();
    const char *handshakeInfo = IsStateNeedHandshake(state) ? " with handshake" : "";
    if ((state == SWITCHING || state == SWITCHED) && displayId.has_value()) {
        ACCOUNT_LOGI("Dynamic publish %{public}s dispatch start, %{public}d->%{public}d on %{public}d%{public}s",
            StateName(state), fromId, toId, static_cast<int>(displayId.value()), handshakeInfo);
    } else {
        ACCOUNT_LOGI("Dynamic publish %{public}s dispatch start, %{public}d->%{public}d%{public}s",
            StateName(state), fromId, toId, handshakeInfo);
    }
    {
        std::lock_guard<std::mutex> lock(subscribeRecordMutex_);
        PublishToAllSubscribers(fromId, state, toId, displayId, token);
    }
    if ((state == SWITCHING || state == SWITCHED) && displayId.has_value()) {
        ACCOUNT_LOGI("Dynamic publish %{public}s dispatch end, %{public}d->%{public}d on %{public}d%{public}s",
            StateName(state), fromId, toId, static_cast<int>(displayId.value()), handshakeInfo);
    } else {
        ACCOUNT_LOGI("Dynamic publish %{public}s dispatch end, %{public}d->%{public}d%{public}s",
            StateName(state), fromId, toId, handshakeInfo);
    }
    return token;
}

ErrCode OsAccountSubscribeManager::WaitForStateReplies(const StatePublishToken &token, OsAccountState state)
{
    std::chrono::milliseconds waitTime = std::chrono::seconds(DEACTIVATION_WAIT_SECONDS);
#ifdef OS_ACCOUNT_SUPPORT_STATIC_SUBSCRIPTION
    // Feature-enabled SWITCHING fork (declared here so the semantic difference is visible):
    // with a configured timeout the handshake replies are awaited for that long; without one
    // the dispatch is advisory — pending replies are ignored and released on late OnComplete,
    // so this returns immediately. With the feature disabled SWITCHING carries no handshake
    // at all, and every state keeps the legacy fixed wait below.
    if (state == OsAccountState::SWITCHING) {
        int32_t waitMs = OsAccountStaticSubscriberManager::GetInstance().GetStateWaitTimeMs(state);
        if (waitMs <= 0) {
            return ERR_OK; // no timeout configured for this state: dispatch without waiting
        }
        waitTime = std::chrono::milliseconds(waitMs);
    }
#endif
    // The token mutex is shared with the reply callbacks, so a reply landing between the
    // predicate check and the park still wakes the waiter instead of being lost.
    std::unique_lock<std::mutex> waitLock(*token.mutexPtr);
    auto result = token.cvPtr->wait_for(waitLock, waitTime,
        [safeQueue = token.safeQueue]() { return safeQueue->Size() == 0; });
    if (!result) {
        return ERR_ACCOUNT_COMMON_OPERATION_TIMEOUT;
    }
    return ERR_OK;
}

/*
 * Bounded join before the caller's subsequent event fan-out so the published notification
 * (static publish and dynamic reply collection) always finishes first. The publish settles
 * within one configured window (both handshakes run concurrently, each waiting at most its
 * configured budget from the dispatch instant), so a full configured wait started here —
 * even after the caller's own work — always covers the future: a timeout is a safety net,
 * not an expected outcome. Unconfigured means no wait; a timeout is only reported, never
 * blocking the caller's flow.
 */
void OsAccountSubscribeManager::AwaitPublish(std::future<ErrCode> &notifyFuture, OsAccountState state,
    int32_t fromId, int32_t toId, uint64_t displayId)
{
#ifdef OS_ACCOUNT_SUPPORT_STATIC_SUBSCRIPTION
    int32_t waitMs = OsAccountStaticSubscriberManager::GetInstance().GetStateWaitTimeMs(state);
    if (waitMs <= 0 || !notifyFuture.valid() ||
        notifyFuture.wait_for(std::chrono::milliseconds(waitMs)) != std::future_status::timeout) {
        return;
    }
    ACCOUNT_LOGE("Publish %{public}s notify timeout, %{public}d->%{public}d on %{public}d",
        StateName(state), fromId, toId, static_cast<int>(displayId));
    REPORT_OS_ACCOUNT_FAIL(toId, OsAccountStateReplyCallbackService::ConvertStateToSceneFlag(state),
        ERR_ACCOUNT_COMMON_OPERATION_TIMEOUT, std::string("Publish ") + StateName(state) + " notify timeout, " +
        std::to_string(fromId) + "->" + std::to_string(toId) + " on " + std::to_string(displayId));
#else
    // Without the handshake feature Publish never defers the reply wait: the future is
    // already ready and there is nothing to await.
    (void)notifyFuture;
    (void)state;
    (void)fromId;
    (void)toId;
    (void)displayId;
#endif
}

/*
 * When the static subscriber config defines a reply-wait timeout for a config-supported
 * state (STATE_NAME_MAP, currently SWITCHING), the wait (static publish plus dynamic
 * reply collection) runs on a detached thread and the returned future stays pending
 * until it finishes; the switch flow overlaps other work and joins via AwaitPublish.
 * Any other state — configured or not — runs the bounded legacy wait inline, so its
 * returned future is already ready (PublishSync relies on this). Without a configured
 * timeout the dispatch is advisory and the future is ready too. The static handshake
 * and the dynamic reply wait run concurrently, each waiting at most its configured
 * budget from the dispatch instant, so the future settles within one configured window
 * — a serial arrangement could take the sum of both budgets, which the caller's
 * single-window join (AwaitPublish) would then misreport as a notify timeout.
 */
std::future<ErrCode> OsAccountSubscribeManager::Publish(int32_t id, OsAccountState state)
{
    // A single-account event has no counterpart account; the account id fills the to slot so
    // dispatch logs, static handshake publishes and subscriber parcels carry one id pair.
    return Publish(id, state, id);
}

ErrCode OsAccountSubscribeManager::PublishSync(int32_t id, OsAccountState state)
{
    // Only a config-supported state (STATE_NAME_MAP) with a configured timeout defers its
    // reply wait to a detached task; every other state (LOCKING/LOCKED included) runs the
    // bounded legacy wait inline inside Publish and returns an already-ready future, so
    // this get() never blocks. Do not call this with a config-supported state: that path
    // is joined via the future-returning Publish + AwaitPublish.
    return Publish(id, state).get();
}

std::future<ErrCode> OsAccountSubscribeManager::Publish(int32_t fromId, OsAccountState state,
    int32_t toId, std::optional<uint64_t> displayId)
{
    auto token = EnqueueStateChange(fromId, state, toId, displayId);
#ifdef OS_ACCOUNT_SUPPORT_STATIC_SUBSCRIPTION
    /*
     * Static subscribers are always published when the feature is on; the configured state
     * timeout only decides below whether the replies are awaited. Without one the dispatch
     * is advisory: the callbacks run on detached threads and nobody joins their replies.
     */
    auto staticDone = std::make_shared<std::promise<void>>();
    auto staticTask = [fromId, state, toId, displayId, staticDone]() {
        OsAccountStaticSubscriberManager::GetInstance().SyncPublish(
            fromId, state, toId, displayId.value_or(0));
        staticDone->set_value();
    };
    if (!StartDetachedThread(staticTask, "StaticPublish")) {
        ACCOUNT_LOGE("Static publish %{public}s dispatch failed, %{public}d->%{public}d",
            StateName(state), fromId, toId);
        staticTask();
    }
    int32_t waitMs = OsAccountStaticSubscriberManager::GetInstance().GetStateWaitTimeMs(state);
    // Only a state the static subscriber config supports (STATE_NAME_MAP) defers its reply
    // wait when configured: its callers overlap other work and join via AwaitPublish. Every
    // other state — even if someone sets a timeout for it — runs the bounded legacy wait
    // inline, so its future is always returned ready and a joining caller can never block
    // on a detached promise-setter (P14).
    if (waitMs > 0 && OsAccountStaticSubscriberConfig::IsSupportedState(state)) {
        auto promisePtr = std::make_shared<std::promise<ErrCode>>();
        std::future<ErrCode> publishFuture = promisePtr->get_future();
        auto dynamicTask = [this, token, fromId, state, toId, displayId, promisePtr, staticDone]() {
            ErrCode result = WaitForStateReplies(token, state);
            staticDone->get_future().wait(); // join the static leg, bounded by its own budget
            if (result != ERR_OK) {
                ReportDynamicReplyTimeout(state, fromId, toId, displayId);
            }
            promisePtr->set_value(result);
        };
        if (!StartDetachedThread(dynamicTask, "DynamicReply")) {
            ACCOUNT_LOGE("Publish %{public}s dispatch failed, %{public}d->%{public}d on %{public}d",
                StateName(state), fromId, toId, static_cast<int>(displayId.value_or(0)));
            REPORT_OS_ACCOUNT_FAIL(toId, OsAccountStateReplyCallbackService::ConvertStateToSceneFlag(state),
                ERR_ACCOUNT_COMMON_BUSY, std::string("Publish ") + StateName(state) + " dispatch failed, " +
                std::to_string(fromId) + "->" + std::to_string(toId) + " on " +
                std::to_string(displayId.value_or(0)));
            promisePtr->set_value(ERR_ACCOUNT_COMMON_BUSY);
        }
        return publishFuture;
    }
#endif
    ErrCode result = WaitForStateReplies(token, state);
    if (result != ERR_OK) {
        ReportDynamicReplyTimeout(state, fromId, toId, displayId);
    }
    std::promise<ErrCode> readyPromise;
    readyPromise.set_value(result);
    return readyPromise.get_future();
}

void OsAccountSubscribeManager::ReportDynamicReplyTimeout(OsAccountState state, int32_t fromId, int32_t toId,
    const std::optional<uint64_t> &displayId)
{
    if (displayId.has_value()) {
        ACCOUNT_LOGE("Dynamic publish %{public}s reply timeout, %{public}d->%{public}d on %{public}d "
            "with handshake", StateName(state), fromId, toId, static_cast<int>(displayId.value()));
    } else {
        ACCOUNT_LOGE("Dynamic publish %{public}s reply timeout, %{public}d->%{public}d with handshake",
            StateName(state), fromId, toId);
    }
    REPORT_OS_ACCOUNT_FAIL(toId, OsAccountStateReplyCallbackService::ConvertStateToSceneFlag(state),
        ERR_ACCOUNT_COMMON_OPERATION_TIMEOUT, std::string("Dynamic publish ") + StateName(state) +
        " reply timeout with handshake");
}

void OsAccountSubscribeManager::PublishToAllSubscribers(int32_t fromId, OsAccountState state, int32_t toId,
    std::optional<uint64_t> displayId, const StatePublishToken &token)
{
    for (const auto &subscribeRecord : subscribeRecords_) {
        if (subscribeRecord->subscribeInfoPtr_ == nullptr) {
            ACCOUNT_LOGE("Subscribe info is null, %{public}d->%{public}d", fromId, toId);
            continue;
        }
        auto eventProxy = iface_cast<IOsAccountEvent>(subscribeRecord->eventListener_);
        if (eventProxy == nullptr) {
            ACCOUNT_LOGE("Event proxy is nullptr");
            continue;
        }
        int32_t subscriberUid = subscribeRecord->callingUid_;
        OS_ACCOUNT_SUBSCRIBE_TYPE subscribeType;
        subscribeRecord->subscribeInfoPtr_->GetOsAccountSubscribeType(subscribeType);
        if (subscribeType == state) { // For old version
            OnStateChangedV0(eventProxy, state, fromId, toId, subscriberUid, displayId);
            continue;
        }
        std::set<OsAccountState> states;
        subscribeRecord->subscribeInfoPtr_->GetStates(states);
        if (states.find(state) == states.end()) {
            continue;
        }
        OsAccountStateParcel stateParcel;
        stateParcel.fromId = fromId;
        stateParcel.toId = toId;
        stateParcel.state = state;
        if (state == SWITCHING || state == SWITCHED) {
            stateParcel.displayId = displayId;
        }
        if (IsStateNeedHandshake(state) && (subscribeRecord->subscribeInfoPtr_->IsWithHandshake())) {
            stateParcel.callback = new (std::nothrow) OsAccountStateReplyCallbackService(
                fromId, state, token.mutexPtr, token.cvPtr, token.safeQueue, subscriberUid);
            if (stateParcel.callback != nullptr) {
                token.safeQueue->Push(1);
            }
        }
        if (!OnStateChanged(eventProxy, stateParcel, subscriberUid)) {
            // The event was not dispatched (e.g. no switch record for this uid); release the
            // pending reply item so the publisher will not wait for a reply that never comes.
            auto callback = static_cast<OsAccountStateReplyCallbackService *>(stateParcel.callback.GetRefPtr());
            if (callback != nullptr) {
                callback->ForceComplete();
            }
        }
    }
}
}  // namespace AccountSA
}  // namespace OHOS
