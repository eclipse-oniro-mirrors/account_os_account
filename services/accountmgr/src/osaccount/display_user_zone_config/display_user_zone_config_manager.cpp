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

#include "osaccount/display_user_zone_config/display_user_zone_config_manager.h"

#include "account_log_wrapper.h"

#include <charconv>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include "account_file_operator.h"
#include "os_account_constants.h"
#ifdef HAS_CONFIG_POLICY_PART
#include "config_policy_utils.h"
#endif

namespace OHOS {
namespace AccountSA {

namespace {
std::string GetDisplayUserZoneConfigPath()
{
#ifdef ACCOUNT_TEST
    // Tests compile this file with ACCOUNT_TEST and place a real XML file under the
    // writable account test directory instead of the system configuration path.
    return "/data/service/el1/public/account/test/display_manager_config.xml";
#else
    std::string configPath = "/system/" + Constants::DISPLAY_MANAGER_CONFIG_RELATIVE_PATH;
#ifdef HAS_CONFIG_POLICY_PART
    char buf[MAX_PATH_LEN] = {0};
    char *path = GetOneCfgFile(Constants::DISPLAY_MANAGER_CONFIG_RELATIVE_PATH.c_str(), buf, MAX_PATH_LEN);
    if (path != nullptr && *path != '\0') {
        configPath = path;
    }
#endif // HAS_CONFIG_POLICY_PART
    return configPath;
#endif // ACCOUNT_TEST
}
} // namespace

DisplayUserZoneConfigManager &DisplayUserZoneConfigManager::GetInstance()
{
    static DisplayUserZoneConfigManager instance;
    return instance;
}

ErrCode DisplayUserZoneConfigManager::IsDisplayPrimary(uint64_t logicalDisplayId, bool &isPrimary)
{
    ErrCode errCode = EnsureConfigReady();
    if (errCode != ERR_OK) {
        return errCode;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    auto iter = logicalIdMap_.find(logicalDisplayId);
    if (iter == logicalIdMap_.end()) {
        return ERR_ACCOUNT_COMMON_DISPLAY_NOT_IN_CONFIG_ERROR;
    }
    isPrimary = iter->second.logicalId == iter->second.userZone;
    return ERR_OK;
}

ErrCode DisplayUserZoneConfigManager::GetPrimaryDisplayId(uint64_t logicalDisplayId, uint64_t &primaryDisplayId)
{
    ErrCode errCode = EnsureConfigReady();
    if (errCode != ERR_OK) {
        return errCode;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    auto displayIter = logicalIdMap_.find(logicalDisplayId);
    if (displayIter == logicalIdMap_.end()) {
        return ERR_ACCOUNT_COMMON_DISPLAY_NOT_IN_CONFIG_ERROR;
    }
    primaryDisplayId = displayIter->second.userZone;
    return ERR_OK;
}

ErrCode DisplayUserZoneConfigManager::GetDisplayIdsByLogicalId(uint64_t logicalDisplayId,
    std::vector<uint64_t> &displayIds)
{
    displayIds.clear();
    ErrCode errCode = EnsureConfigReady();
    if (errCode != ERR_OK) {
        return errCode;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    auto displayIter = logicalIdMap_.find(logicalDisplayId);
    if (displayIter == logicalIdMap_.end()) {
        return ERR_ACCOUNT_COMMON_DISPLAY_NOT_IN_CONFIG_ERROR;
    }
    auto userZoneIter = userZoneMap_.find(displayIter->second.userZone);
    if (userZoneIter == userZoneMap_.end()) {
        return ERR_OK;
    }
    displayIds = userZoneIter->second;
    return ERR_OK;
}

ErrCode DisplayUserZoneConfigManager::Init()
{
    return EnsureConfigReady();
}

ErrCode DisplayUserZoneConfigManager::ReadConfigContent(std::string &content)
{
    std::string configPath = GetDisplayUserZoneConfigPath();
    AccountFileOperator fileOperator;
    ErrCode errCode = fileOperator.GetFileContentByPath(configPath, content);
    if (errCode != ERR_OK) {
        ACCOUNT_LOGE("Failed to read display user zone config from %{public}s, err=%{public}d",
            configPath.c_str(), errCode);
        return ERR_ACCOUNT_COMMON_FILE_READ_FAILED;
    }
    return ERR_OK;
}

// The whole body runs under mutex_. The config read targets a small read-only /system XML,
// so the brief lock-held I/O is a deliberate trade for a single-segment critical section.
ErrCode DisplayUserZoneConfigManager::EnsureConfigReady()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (initOk_) {
        return ERR_OK;
    }
    ClearParsedData();
    initOk_ = false;
    std::string content;
    ErrCode errCode = ReadConfigContent(content);
    if (errCode != ERR_OK) {
        return errCode;
    }
    errCode = ParseDisplayConfig(content);
    if (errCode != ERR_OK) {
        return ERR_ACCOUNT_COMMON_BAD_JSON_FORMAT_ERROR;
    }
    initOk_ = true;
    return ERR_OK;
}

namespace {
class XmlDocGuard {
public:
    explicit XmlDocGuard(xmlDocPtr doc) : doc_(doc) {}
    ~XmlDocGuard()
    {
        if (doc_ != nullptr) {
            xmlFreeDoc(doc_);
        }
    }
    XmlDocGuard(const XmlDocGuard &) = delete;
    XmlDocGuard &operator=(const XmlDocGuard &) = delete;

private:
    xmlDocPtr doc_;
};

xmlNodePtr FindDisplaysNode(xmlDocPtr doc)
{
    xmlNodePtr root = xmlDocGetRootElement(doc);
    if (root == nullptr || xmlStrcmp(root->name, BAD_CAST "Configs") != 0) {
        return nullptr;
    }
    for (xmlNodePtr node = root->children; node != nullptr; node = node->next) {
        if (node->type != XML_ELEMENT_NODE || xmlStrcmp(node->name, BAD_CAST "displays") != 0) {
            continue;
        }
        return node;
    }
    return nullptr;
}

// XML leaf values may carry pretty-printing whitespace (spaces, tabs, newlines);
// trim it before the value is consumed.
std::string TrimConfigValue(const std::string &value)
{
    const std::string whiteSpaces = " \t\r\n\f\v";
    size_t start = value.find_first_not_of(whiteSpaces);
    if (start == std::string::npos) {
        return "";
    }
    size_t end = value.find_last_not_of(whiteSpaces);
    return value.substr(start, end - start + 1);
}
} // namespace

ErrCode DisplayUserZoneConfigManager::ParseDisplayConfig(const std::string &content)
{
    xmlDocPtr doc = xmlReadMemory(content.c_str(), static_cast<int>(content.size()),
        "display_manager_config.xml", nullptr, XML_PARSE_NOBLANKS);
    if (doc == nullptr) {
        ACCOUNT_LOGE("Failed to parse display user zone config XML");
        return ERR_ACCOUNT_COMMON_BAD_JSON_FORMAT_ERROR;
    }
    XmlDocGuard docGuard(doc);
    xmlNodePtr displaysNode = FindDisplaysNode(doc);
    if (displaysNode == nullptr) {
        ACCOUNT_LOGE("Invalid XML: missing Configs or displays element");
        return ERR_ACCOUNT_COMMON_BAD_JSON_FORMAT_ERROR;
    }
    ErrCode errCode = ParseDisplayNodes(displaysNode);
    if (errCode != ERR_OK) {
        return errCode;
    }
    return FinalizeParsedConfig();
}

ErrCode DisplayUserZoneConfigManager::ParseDisplayNodes(xmlNodePtr root)
{
    std::vector<DisplayConfigInfo> displays;
    std::map<uint64_t, uint64_t> physicalToLogical;
    std::map<uint64_t, uint64_t> userZoneIdByLogicalId;
    ErrCode errCode = CollectDisplayNodes(root, displays, physicalToLogical, userZoneIdByLogicalId);
    if (errCode != ERR_OK) {
        return errCode;
    }
    return ResolveDisplayUserZones(displays, physicalToLogical, userZoneIdByLogicalId);
}

ErrCode DisplayUserZoneConfigManager::CollectDisplayNodes(xmlNodePtr root,
    std::vector<DisplayConfigInfo> &displays, std::map<uint64_t, uint64_t> &physicalToLogical,
    std::map<uint64_t, uint64_t> &userZoneIdByLogicalId)
{
    for (xmlNodePtr node = root->children; node != nullptr; node = node->next) {
        if (node->type != XML_ELEMENT_NODE || xmlStrcmp(node->name, BAD_CAST "display") != 0) {
            continue;
        }
        DisplayConfigInfo info;
        uint64_t userZoneId = 0;
        bool hasUserZoneId = false;
        ErrCode errCode = ParseDisplayNodeAttributes(node, info, userZoneId, hasUserZoneId);
        if (errCode != ERR_OK) {
            return errCode;
        }
        if (physicalToLogical.find(info.physicalId) != physicalToLogical.end()) {
            ACCOUNT_LOGE("Duplicate physicalId=%{public}llu in config, config rejected",
                static_cast<unsigned long long>(info.physicalId));
            return ERR_ACCOUNT_COMMON_BAD_JSON_FORMAT_ERROR;
        }
        displays.push_back(info);
        physicalToLogical[info.physicalId] = info.logicalId;
        if (hasUserZoneId) {
            userZoneIdByLogicalId[info.logicalId] = userZoneId;
        }
    }
    return ERR_OK;
}

ErrCode DisplayUserZoneConfigManager::ResolveDisplayUserZones(const std::vector<DisplayConfigInfo> &displays,
    const std::map<uint64_t, uint64_t> &physicalToLogical,
    const std::map<uint64_t, uint64_t> &userZoneIdByLogicalId)
{
    for (const auto &info : displays) {
        if (logicalIdMap_.find(info.logicalId) != logicalIdMap_.end()) {
            ACCOUNT_LOGE("Duplicate logicalId=%{public}llu in config, config rejected",
                static_cast<unsigned long long>(info.logicalId));
            ClearParsedData();
            return ERR_ACCOUNT_COMMON_BAD_JSON_FORMAT_ERROR;
        }
        DisplayConfigInfo resolved = info;
        auto userZoneIt = userZoneIdByLogicalId.find(info.logicalId);
        if (userZoneIt != userZoneIdByLogicalId.end()) {
            auto it = physicalToLogical.find(userZoneIt->second);
            if (it == physicalToLogical.end()) {
                ACCOUNT_LOGE("userZoneId=%{public}llu not found for display logicalId=%{public}llu",
                    static_cast<unsigned long long>(userZoneIt->second),
                    static_cast<unsigned long long>(info.logicalId));
                ClearParsedData();
                return ERR_ACCOUNT_COMMON_BAD_JSON_FORMAT_ERROR;
            }
            resolved.userZone = it->second;
        } else {
            resolved.userZone = info.logicalId;
        }
        logicalIdMap_[resolved.logicalId] = resolved;
        userZoneMap_[resolved.userZone].push_back(resolved.logicalId);
        ACCOUNT_LOGI("Parsed display: logicalId=%{public}llu physicalId=%{public}llu group=%{public}llu "
            "name=%{public}s",
            static_cast<unsigned long long>(resolved.logicalId),
            static_cast<unsigned long long>(resolved.physicalId),
            static_cast<unsigned long long>(resolved.userZone), resolved.name.c_str());
    }
    return ERR_OK;
}

ErrCode DisplayUserZoneConfigManager::ParseDisplayNodeAttributes(xmlNodePtr node, DisplayConfigInfo &info,
    uint64_t &userZoneId, bool &hasUserZoneId)
{
    for (xmlNodePtr child = node->children; child != nullptr; child = child->next) {
        if (child->type != XML_ELEMENT_NODE) {
            continue;
        }
        std::string name = reinterpret_cast<const char *>(child->name);
        xmlChar *nodeContent = xmlNodeGetContent(child);
        std::string value = (nodeContent != nullptr) ? reinterpret_cast<const char *>(nodeContent) : "";
        xmlFree(nodeContent);
        if (name == "physicalId") {
            if (info.hasPhysicalId || !ParseUInt64(value, info.physicalId)) {
                ACCOUNT_LOGE("Failed to parse physicalId, value=%{public}s", value.c_str());
                return ERR_ACCOUNT_COMMON_BAD_JSON_FORMAT_ERROR;
            }
            info.hasPhysicalId = true;
        } else if (name == "logicalId") {
            if (info.hasLogicalId || !ParseUInt64(value, info.logicalId)) {
                ACCOUNT_LOGE("Failed to parse logicalId, value=%{public}s", value.c_str());
                return ERR_ACCOUNT_COMMON_BAD_JSON_FORMAT_ERROR;
            }
            info.hasLogicalId = true;
        } else if (name == "name") {
            info.name = value;
        } else if (name == "userZoneId") {
            if (hasUserZoneId || !ParseUInt64(value, userZoneId)) {
                ACCOUNT_LOGE("Failed to parse userZoneId, value=%{public}s", value.c_str());
                return ERR_ACCOUNT_COMMON_BAD_JSON_FORMAT_ERROR;
            }
            hasUserZoneId = true;
        }
    }
    if (!info.hasPhysicalId || !info.hasLogicalId) {
        ACCOUNT_LOGE("Display config is missing physicalId or logicalId");
        return ERR_ACCOUNT_COMMON_BAD_JSON_FORMAT_ERROR;
    }
    return ERR_OK;
}

ErrCode DisplayUserZoneConfigManager::FinalizeParsedConfig()
{
    ErrCode errCode = ValidateParsedConfig();
    if (errCode != ERR_OK) {
        ClearParsedData();
        return errCode;
    }
    ACCOUNT_LOGI("Display user zone config loaded, display count: %{public}zu", logicalIdMap_.size());
    return ERR_OK;
}

void DisplayUserZoneConfigManager::ClearParsedData()
{
    logicalIdMap_.clear();
    userZoneMap_.clear();
}

bool DisplayUserZoneConfigManager::ParseUInt64(const std::string &value, uint64_t &result)
{
    const std::string trimmed = TrimConfigValue(value);
    if (trimmed.empty()) {
        return false;
    }
    auto res = std::from_chars(trimmed.data(), trimmed.data() + trimmed.size(), result);
    return res.ec == std::errc{} && res.ptr == trimmed.data() + trimmed.size();
}

ErrCode DisplayUserZoneConfigManager::ValidateParsedConfig()
{
    if (logicalIdMap_.empty()) {
        ACCOUNT_LOGE("No display nodes found in config, config rejected");
        return ERR_ACCOUNT_COMMON_BAD_JSON_FORMAT_ERROR;
    }
    for (const auto &pair : logicalIdMap_) {
        const DisplayConfigInfo &info = pair.second;
        if (info.userZone == info.logicalId) {
            continue;
        }
        auto primaryIt = logicalIdMap_.find(info.userZone);
        if (primaryIt == logicalIdMap_.end()) {
            ACCOUNT_LOGE("Secondary display logicalId=%{public}llu references non-existent primary "
                "group=%{public}llu, config rejected",
                static_cast<unsigned long long>(info.logicalId),
                static_cast<unsigned long long>(info.userZone));
            return ERR_ACCOUNT_COMMON_BAD_JSON_FORMAT_ERROR;
        }
        if (primaryIt->second.userZone != primaryIt->second.logicalId) {
            ACCOUNT_LOGE("Secondary display logicalId=%{public}llu references display %{public}llu "
                "which is itself a secondary (group=%{public}llu), possible cycle",
                static_cast<unsigned long long>(info.logicalId),
                static_cast<unsigned long long>(primaryIt->second.logicalId),
                static_cast<unsigned long long>(primaryIt->second.userZone));
            return ERR_ACCOUNT_COMMON_BAD_JSON_FORMAT_ERROR;
        }
    }
    return ERR_OK;
}
}  // namespace AccountSA
}  // namespace OHOS
