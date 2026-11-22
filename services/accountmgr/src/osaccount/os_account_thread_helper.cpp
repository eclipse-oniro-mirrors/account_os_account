/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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

#include "os_account_thread_helper.h"

#include <memory>
#include <pthread.h>

namespace OHOS {
namespace AccountSA {
namespace {
#ifndef FUZZ_TEST
void *RunDetachedTask(void *arg)
{
    // Take over ownership at the thread boundary so cleanup is guaranteed even if the task returns early.
    std::unique_ptr<std::function<void()>> entry(static_cast<std::function<void()> *>(arg));
    (*entry)();
    return nullptr;
}
#endif // FUZZ_TEST
} // namespace

bool StartDetachedThread(const std::function<void()> &task, const char *threadName)
{
#ifdef FUZZ_TEST
    (void)threadName;
    task();
    return true;
#else
    std::unique_ptr<std::function<void()>> heapTask = std::make_unique<std::function<void()>>(task);
    pthread_t threadId;
    if (pthread_create(&threadId, nullptr, RunDetachedTask, heapTask.get()) != 0) {
        return false; // heapTask is released automatically, no manual delete needed
    }
    (void)heapTask.release(); // ownership transferred to the new thread; pthread boundary takes raw pointers
    if (threadName != nullptr) {
        pthread_setname_np(threadId, threadName);
    }
    (void)pthread_detach(threadId);
    return true;
#endif
}
} // namespace AccountSA
} // namespace OHOS
