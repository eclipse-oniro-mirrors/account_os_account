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

#ifndef OS_ACCOUNT_SERVICES_ACCOUNTMGR_INCLUDE_OS_ACCOUNT_STATIC_SUBSCRIBER_CONFIG_H
#define OS_ACCOUNT_SERVICES_ACCOUNTMGR_INCLUDE_OS_ACCOUNT_STATIC_SUBSCRIBER_CONFIG_H

#include <map>
#include <set>
#include <string>
#include <vector>
#include "os_account_subscribe_info.h"

namespace OHOS {
namespace AccountSA {
#ifdef OS_ACCOUNT_SUPPORT_STATIC_SUBSCRIPTION
struct SubscriberConfig {
    std::string name;
    std::set<OsAccountState> states;
    std::string soPath;
    bool withHandshake = false;
};

/*
 * Result of loading the static subscriber config: parsed subscriber entries plus the
 * per-state wait timeouts. Timeouts are kept at state level so dynamic-subscriber waits
 * and rollback joins also work when no static subscriber is configured for the state.
 * An unconfigured state has no timeout entry, which means "do not wait".
 */
struct SubscriberConfigData {
    std::vector<SubscriberConfig> subscribers;
    std::map<OsAccountState, int32_t> stateTimeouts;
};

/**
 * Parses the static subscriber configuration (file location, JSON schema, field validation).
 * This class is stateless and does no dlopen/registration work, which is owned by
 * OsAccountStaticSubscriberManager.
 */
class OsAccountStaticSubscriberConfig {
public:
    /**
     * Resolves the config file path through the config policy, empty if unavailable.
     */
    static std::string GetConfigPath();

    /**
     * Reads and parses the config file. Both fields of the result are empty when the file
     * is missing, empty, or malformed.
     */
    static SubscriberConfigData Load(const std::string &configPath);

    /**
     * Resolves the default config path through the config policy and parses it.
     * Both fields of the result are empty when the path is unavailable or parsing fails.
     */
    static SubscriberConfigData LoadDefault();

    /**
     * Parses config content. Every rejected entry is logged and skipped, so the result
     * only contains valid subscriber configs and their timeouts.
     */
    static SubscriberConfigData Parse(const std::string &content);

    /**
     * Checks that the resolved so path stays under an allowed system library prefix and
     * does not contain a ".." component.
     */
    static bool IsPathAllowed(const std::string &path);

    /**
     * Whether the config schema recognizes the state as a waitable handshake state
     * (STATE_NAME_MAP). Only these states, when configured with a timeout, get the
     * deferred reply-wait treatment from the publish fork; every other state always
     * runs the bounded inline wait and returns an already-ready future.
     */
    static bool IsSupportedState(OsAccountState state);

private:
    static bool ContainsDotDotComponent(const std::string &path);
};
#endif // OS_ACCOUNT_SUPPORT_STATIC_SUBSCRIPTION
} // namespace AccountSA
} // namespace OHOS

#endif // OS_ACCOUNT_SERVICES_ACCOUNTMGR_INCLUDE_OS_ACCOUNT_STATIC_SUBSCRIBER_CONFIG_H