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

#ifndef OS_ACCOUNT_SERVICES_ACCOUNTMGR_INCLUDE_OSACCOUNT_OS_ACCOUNT_THREAD_HELPER_H
#define OS_ACCOUNT_SERVICES_ACCOUNTMGR_INCLUDE_OSACCOUNT_OS_ACCOUNT_THREAD_HELPER_H

#include <functional>

namespace OHOS {
namespace AccountSA {
/*
 * Start a detached thread to run task. Returns false when memory allocation or thread
 * creation fails, so callers can degrade gracefully.
 * Note: std::thread aborts the process on construction failure because accountmgr is built
 * with -fno-exceptions; paths that must not crash the service should use this helper instead.
 * In FUZZ_TEST builds the task runs synchronously in the caller's thread and true is returned.
 */
bool StartDetachedThread(const std::function<void()> &task, const char *threadName);
} // namespace AccountSA
} // namespace OHOS

#endif // OS_ACCOUNT_SERVICES_ACCOUNTMGR_INCLUDE_OSACCOUNT_OS_ACCOUNT_THREAD_HELPER_H
