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

#include "perf_stat.h"

#include "account_log_wrapper.h"

namespace OHOS {
namespace AccountSA {
PerfStat::PerfStat()
{}

PerfStat::~PerfStat()
{
    Reset();
}

/* Account Bind process statistic */
int64_t PerfStat::GetAccountBindStartTime() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return accountBindBegin_;
}

void PerfStat::SetAccountBindStartTime(int64_t time)
{
    std::lock_guard<std::mutex> lock(mutex_);
    accountBindBegin_ = (time > 0) ? time : 0;
}

int64_t PerfStat::GetAccountBindEndTime() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return accountBindEnd_;
}

void PerfStat::SetAccountBindEndTime(int64_t time)
{
    std::lock_guard<std::mutex> lock(mutex_);
    accountBindEnd_ = (time > 0 && time > accountBindBegin_) ? time : accountBindBegin_;
}

/* Account Add process statistic */
int64_t PerfStat::GetAccountAddStartTime() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return accountAddBegin_;
}

void PerfStat::SetAccountAddStartTime(int64_t time)
{
    std::lock_guard<std::mutex> lock(mutex_);
    accountAddBegin_ = (time > 0) ? time : 0;
}

int64_t PerfStat::GetAccountAddEndTime() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return accountAddEnd_;
}

void PerfStat::SetAccountAddEndTime(int64_t time)
{
    std::lock_guard<std::mutex> lock(mutex_);
    accountAddEnd_ = (time > 0 && time > accountAddBegin_) ? time : accountAddBegin_;
}

/* Account Delete process statistic */
int64_t PerfStat::GetAccountDelStartTime() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return accountDelBegin_;
}

void PerfStat::SetAccountDelStartTime(int64_t time)
{
    std::lock_guard<std::mutex> lock(mutex_);
    accountDelBegin_ = (time > 0) ? time : 0;
}

int64_t PerfStat::GetAccountDelEndTime() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return accountDelEnd_;
}

void PerfStat::SetAccountDelEndTime(int64_t time)
{
    std::lock_guard<std::mutex> lock(mutex_);
    accountDelEnd_ = (time > 0 && time > accountDelBegin_) ? time : accountDelBegin_;
}

/* Account Query process statistic */
int64_t PerfStat::GetAccountQueryStartTime() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return accountQueryBegin_;
}

void PerfStat::SetAccountQueryStartTime(int64_t time)
{
    std::lock_guard<std::mutex> lock(mutex_);
    accountQueryBegin_ = (time > 0) ? time : 0;
}

int64_t PerfStat::GetAccountQueryEndTime() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return accountQueryEnd_;
}

void PerfStat::SetAccountQueryEndTime(int64_t time)
{
    std::lock_guard<std::mutex> lock(mutex_);
    accountQueryEnd_ = (time > 0 && time > accountQueryBegin_) ? time : accountQueryBegin_;
}

/* Account Service process statistic */
void PerfStat::SetInstanceStartTime(int64_t time)
{
    std::lock_guard<std::mutex> lock(mutex_);
    serviceOnStart_ = time;
}

void PerfStat::SetInstanceStopTime(int64_t time)
{
    std::lock_guard<std::mutex> lock(mutex_);
    serviceOnStop_ = time;
}

void PerfStat::SetInstanceCreateTime(int64_t time)
{
    std::lock_guard<std::mutex> lock(mutex_);
    instanceCreate_ = time;
}

void PerfStat::SetInstanceInitTime(int64_t time)
{
    std::lock_guard<std::mutex> lock(mutex_);
    serviceInit_ = time;
}

/* Account state machine process statistic */
void PerfStat::SetAccountStateChangeTime(const std::string &stateStr, int64_t time)
{
    std::lock_guard<std::mutex> lock(mutex_);
    accountStateChangeRecords_[stateStr] = time;
}

/* Set/Get perf statistic enable state */
bool PerfStat::GetPerfStatEnabled() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return enableStat_;
}

void PerfStat::SetPerfStatEnabled(bool enable)
{
    std::lock_guard<std::mutex> lock(mutex_);
    enableStat_ = enable;
}

/* reset to default */
void PerfStat::Reset()
{
    // Whole body under one lock so Reset is atomic w.r.t. Set*/Dump: a concurrent
    // Dump snapshots a consistent view (all-old or all-zero), never a half-reset mix.
    std::lock_guard<std::mutex> lock(mutex_);
    accountBindBegin_ = 0;
    accountBindEnd_ = 0;

    accountAddBegin_ = 0;
    accountAddEnd_ = 0;

    accountDelBegin_ = 0;
    accountDelEnd_ = 0;

    accountQueryBegin_ = 0;
    accountQueryEnd_ = 0;

    instanceCreate_ = 0;
    serviceOnStart_ = 0;
    serviceOnStop_ = 0;
    serviceInit_ = 0;

    accountStateChangeRecords_.clear();

    enableStat_ = true;
}

void PerfStat::Dump(std::string& result) const
{
    // Whole body under mutex_ so concurrent Set*/Reset can't race the scalar reads
    // or mutate accountStateChangeRecords_ during iteration. Safe to hold across the
    // string formatting / ACCOUNT_LOGI below: no PerfStat accessor acquires another
    // lock under mutex_, so the lock order is one-directional (mutex_ -> HiLog lock),
    // never reversed -> no deadlock. mutex_ is non-recursive and this is the only
    // lock taken in Dump.
    std::lock_guard<std::mutex> lock(mutex_);
    if (!enableStat_) {
        ACCOUNT_LOGI("statistics disabled!");
        return;
    }

    if (instanceCreate_ > 0) {
        result.append("ServiceInstanceCreateTime: ").append(std::to_string(instanceCreate_)).append("\n");
    }

    if (serviceInit_ > 0) {
        result.append("ServiceInitTick: ").append(std::to_string(serviceInit_)).append("\n");
    }

    if (serviceOnStart_ > 0) {
        result.append("ServiceStartTick: ").append(std::to_string(serviceOnStart_)).append("\n");
    }

    if (serviceOnStop_ > 0) {
        result.append("ServiceStopTick: ").append(std::to_string(serviceOnStop_)).append("\n");
    }

    if (accountBindEnd_ > accountBindBegin_) {
        result.append("AccountBindTime: ")
            .append(std::to_string(accountBindEnd_ - accountBindBegin_))
            .append("\n");
    }

    if (accountAddEnd_ > accountAddBegin_) {
        result.append("AccountAddTime: ")
            .append(std::to_string(accountAddEnd_ - accountAddBegin_))
            .append("\n");
    }

    if (accountDelEnd_ > accountDelBegin_) {
        result.append("AccountDelTime: ")
            .append(std::to_string(accountDelEnd_ - accountDelBegin_))
            .append("\n");
    }

    if (accountQueryEnd_ > accountQueryBegin_) {
        result.append("AccountQueryTime: ")
            .append(std::to_string((accountQueryEnd_ - accountQueryBegin_)))
            .append("\n");
    }

    auto iter = accountStateChangeRecords_.begin();
    for (; iter != accountStateChangeRecords_.end(); iter++) {
        result.append(iter->first.c_str())
            .append(": ")
            .append(std::to_string(iter->second))
            .append(" Ticks\n");
    }
}
} // namespace AccountSA
} // namespace OHOS
