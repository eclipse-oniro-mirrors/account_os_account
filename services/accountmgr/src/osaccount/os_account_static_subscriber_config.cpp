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

#include "os_account_static_subscriber_config.h"

#include <fstream>
#include "account_constants.h"
#include "account_hisysevent_adapter.h"
#include "account_log_wrapper.h"
#include "json_utils.h"
#ifdef HAS_CONFIG_POLICY_PART
#include "config_policy_utils.h"
#endif

namespace OHOS {
namespace AccountSA {
#ifdef OS_ACCOUNT_SUPPORT_STATIC_SUBSCRIPTION
namespace {
#ifdef HAS_CONFIG_POLICY_PART
static const char STATIC_SUBSCRIBER_CONFIG_FILE[] = "etc/os_account/os_account_static_subscribers.json";
#endif
static const std::vector<std::string> ALLOWED_SO_PATH_PREFIXES = { "/system/lib/", "/system/lib64/" };
constexpr size_t DOT_DOT_COMPONENT_LENGTH = 2;
// Upper bound for a configured state wait: the wait runs while the caller may hold the
// per-id update lock (ActivateOsAccount), so an oversized OEM value must not stretch
// switch/rollback flows. Larger values are clamped, not rejected.
constexpr int32_t MAX_STATE_WAIT_MS = 10000;
static const std::map<std::string, OsAccountState> STATE_NAME_MAP = {
    {"SWITCHING", OsAccountState::SWITCHING},
};
} // namespace

std::string OsAccountStaticSubscriberConfig::GetConfigPath()
{
#ifdef HAS_CONFIG_POLICY_PART
    char buf[MAX_PATH_LEN] = {0};
    char *path = GetOneCfgFile(STATIC_SUBSCRIBER_CONFIG_FILE, buf, MAX_PATH_LEN);
    if (path != nullptr && *path != '\0') {
        return path;
    }
#endif
    return std::string();
}

static bool ReadConfigFile(const std::string &path, std::string &content)
{
    std::ifstream configFile(path);
    if (!configFile.is_open()) {
        ACCOUNT_LOGI("No static subscriber config file, path: %{public}s", path.c_str());
        return false;
    }
    content.assign(std::istreambuf_iterator<char>(configFile), std::istreambuf_iterator<char>());
    if (content.empty()) {
        ACCOUNT_LOGE("Static config file is empty, path: %{public}s", path.c_str());
        REPORT_OS_ACCOUNT_FAIL(0, Constants::OPERATION_EVENT_PUBLISH, ERR_ACCOUNT_COMMON_INVALID_PARAMETER,
            "Static config file is empty, path=" + path);
        return false;
    }
    return true;
}

static bool ParseSubscriberEvents(CJson *item, SubscriberConfig &config)
{
    std::vector<std::string> eventNames;
    if (!GetVectorStringFromJson(item, "events", eventNames) || eventNames.empty()) {
        ACCOUNT_LOGE("Failed to get events from subscriber config");
        return false;
    }
    for (const auto &eventName : eventNames) {
        auto it = STATE_NAME_MAP.find(eventName);
        if (it != STATE_NAME_MAP.end()) {
            config.states.insert(it->second);
        } else {
            ACCOUNT_LOGW("Unsupported event name: %{public}s", eventName.c_str());
        }
    }
    return !config.states.empty();
}

static bool ParseSubscriberItem(CJson *item, SubscriberConfig &config)
{
    config.name = GetStringFromJson(item, "name");
    if (config.name.empty()) {
        ACCOUNT_LOGE("Missing name field in subscriber config");
        return false;
    }
    if (!ParseSubscriberEvents(item, config)) {
        return false;
    }
    config.soPath = GetStringFromJson(item, "soPath");
    if (config.soPath.empty()) {
        ACCOUNT_LOGE("Missing soPath field in subscriber config");
        return false;
    }
    if (config.soPath[0] == '/') {
        ACCOUNT_LOGE("soPath must be a relative path, path=%{public}s", config.soPath.c_str());
        REPORT_OS_ACCOUNT_FAIL(0, Constants::OPERATION_EVENT_PUBLISH, ERR_ACCOUNT_COMMON_INVALID_PARAMETER,
            "soPath must be a relative path, path=" + config.soPath);
        return false;
    }
    bool withHandshake = false;
    GetBoolFromJson(item, "withHandshake", withHandshake);
    config.withHandshake = withHandshake;
    return true;
}

static void ParseStateTimeouts(CJson *root, std::map<OsAccountState, int32_t> &stateTimeouts)
{
    CJson *timeouts = GetObjFromJson(root, "timeout");
    if (timeouts == nullptr || !IsArray(timeouts)) {
        return;
    }
    int32_t timeoutNum = GetItemNum(timeouts);
    for (int32_t i = 0; i < timeoutNum; ++i) {
        CJson *timeoutItem = GetItemFromArray(timeouts, i);
        if (timeoutItem == nullptr) {
            continue;
        }
        std::string eventName;
        int32_t waitTimeMs = 0;
        if (!GetFirstEntryFromObj(timeoutItem, eventName, waitTimeMs)) {
            continue;
        }
        auto it = STATE_NAME_MAP.find(eventName);
        if (it == STATE_NAME_MAP.end()) {
            ACCOUNT_LOGW("Unsupported event name in timeout: %{public}s", eventName.c_str());
            continue;
        }
        if (waitTimeMs <= 0) {
            ACCOUNT_LOGW("Invalid waitTimeMs in timeout, eventName: %{public}s, waitTimeMs: %{public}d",
                eventName.c_str(), waitTimeMs);
            continue;
        }
        if (waitTimeMs > MAX_STATE_WAIT_MS) {
            ACCOUNT_LOGW("waitTimeMs %{public}d exceeds the %{public}d ms cap, clamped, eventName: %{public}s",
                waitTimeMs, MAX_STATE_WAIT_MS, eventName.c_str());
            waitTimeMs = MAX_STATE_WAIT_MS;
        }
        stateTimeouts[it->second] = waitTimeMs;
    }
}

SubscriberConfigData OsAccountStaticSubscriberConfig::Parse(const std::string &content)
{
    SubscriberConfigData result;
    CJsonUnique root = CreateJsonFromString(content);
    if (root == nullptr) {
        ACCOUNT_LOGE("Failed to parse static subscriber config as json");
        REPORT_OS_ACCOUNT_FAIL(0, Constants::OPERATION_EVENT_PUBLISH, ERR_ACCOUNT_COMMON_INVALID_PARAMETER,
            "Failed to parse static subscriber config as json");
        return result;
    }
    ParseStateTimeouts(root.get(), result.stateTimeouts);
    CJson *subscribers = GetObjFromJson(root.get(), "subscribers");
    if (subscribers == nullptr || !IsArray(subscribers)) {
        ACCOUNT_LOGI("No subscribers array in static subscriber config");
        return result;
    }
    int32_t size = GetItemNum(subscribers);
    for (int32_t i = 0; i < size; ++i) {
        CJson *item = GetItemFromArray(subscribers, i);
        if (item == nullptr) {
            continue;
        }
        SubscriberConfig config;
        if (ParseSubscriberItem(item, config)) {
            result.subscribers.push_back(config);
        }
    }
    return result;
}

SubscriberConfigData OsAccountStaticSubscriberConfig::Load(const std::string &configPath)
{
    std::string content;
    if (!ReadConfigFile(configPath, content)) {
        return {};
    }
    return Parse(content);
}

SubscriberConfigData OsAccountStaticSubscriberConfig::LoadDefault()
{
    std::string configPath = GetConfigPath();
    if (configPath.empty()) {
        return {};
    }
    return Load(configPath);
}

bool OsAccountStaticSubscriberConfig::IsSupportedState(OsAccountState state)
{
    for (const auto &entry : STATE_NAME_MAP) {
        if (entry.second == state) {
            return true;
        }
    }
    return false;
}

bool OsAccountStaticSubscriberConfig::ContainsDotDotComponent(const std::string &path)
{
    size_t pos = 0;
    while (pos < path.length()) {
        size_t next = path.find('/', pos);
        if (next == std::string::npos) {
            next = path.length();
        }
        if (next - pos == DOT_DOT_COMPONENT_LENGTH && path[pos] == '.' && path[pos + 1] == '.') {
            return true;
        }
        pos = next + 1;
    }
    return false;
}

bool OsAccountStaticSubscriberConfig::IsPathAllowed(const std::string &path)
{
    if (ContainsDotDotComponent(path)) {
        return false;
    }
    for (const auto &prefix : ALLOWED_SO_PATH_PREFIXES) {
        if (path.compare(0, prefix.length(), prefix) == 0) {
            return true;
        }
    }
    return false;
}
#endif // OS_ACCOUNT_SUPPORT_STATIC_SUBSCRIPTION
} // namespace AccountSA
} // namespace OHOS