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

#ifndef OS_ACCOUNT_SERVICES_ACCOUNTMGR_INCLUDE_OSACCOUNT_DISPLAY_USER_ZONE_CONFIG_DISPLAY_USER_ZONE_CONFIG_MANAGER_H
#define OS_ACCOUNT_SERVICES_ACCOUNTMGR_INCLUDE_OSACCOUNT_DISPLAY_USER_ZONE_CONFIG_DISPLAY_USER_ZONE_CONFIG_MANAGER_H

#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <vector>
#include "account_error_no.h"
#include "nocopyable.h"

// Forward declaration of the libxml2 opaque node type so the private parse helpers
// can take a node pointer without leaking the libxml2 headers into every translation unit.
struct _xmlNode;

namespace OHOS {
namespace AccountSA {

struct DisplayConfigInfo {
    uint64_t physicalId = 0;
    uint64_t logicalId = 0;
    std::string name;
    uint64_t userZone = 0;
    bool hasPhysicalId = false;
    bool hasLogicalId = false;
};

class DisplayUserZoneConfigManager {
public:
    static DisplayUserZoneConfigManager &GetInstance();

    /**
     * Load the display user zone config XML file once. Repeated calls are no-ops
     * after a successful load.
     * @return ERR_OK on success; otherwise a read or format error. A failed load
     *         leaves the manager uninitialized so the next user-zone query retries
     *         the load before consuming any result.
     */
    ErrCode Init();

    /**
     * A failed load (read or format error) is retried on every query.
     * Whether the given logical display is the primary display of its user zone.
     * A display is primary when its logicalId equals its user zone id (i.e. it is
     * a standalone display or the primary that other secondary displays point to).
     * @param logicalDisplayId Logical display id.
     * @param isPrimary Set to true if the display is primary, false if it is explicitly
     *        not primary. Untouched when an error is returned.
     * @return ERR_OK on success; ERR_ACCOUNT_COMMON_FILE_READ_FAILED when the initial
     *         configuration read fails again on this query; ERR_ACCOUNT_COMMON_BAD_JSON_FORMAT_ERROR
     *         when the configuration format is invalid; ERR_ACCOUNT_COMMON_DISPLAY_NOT_IN_CONFIG_ERROR
     *         when the logical display is absent from the configuration.
     */
    ErrCode IsDisplayPrimary(uint64_t logicalDisplayId, bool &isPrimary);

    /**
     * Resolve a logical display to the primary display of its user zone.
     * @param logicalDisplayId Logical display id to resolve.
     * @param primaryDisplayId Set to the corresponding primary display id. Untouched
     *        when an error is returned.
     * @return ERR_OK on success; ERR_ACCOUNT_COMMON_FILE_READ_FAILED after the
     *         read retry for this query fails; ERR_ACCOUNT_COMMON_BAD_JSON_FORMAT_ERROR
     *         if the loaded configuration has an invalid format;
     *         ERR_ACCOUNT_COMMON_DISPLAY_NOT_IN_CONFIG_ERROR when the logical display is
     *         absent from the configuration.
     */
    ErrCode GetPrimaryDisplayId(uint64_t logicalDisplayId, uint64_t &primaryDisplayId);

    /**
     * Get all displays in the user zone containing the given logical display.
     * @param logicalDisplayId Logical display id to resolve.
     * @param displayIds Set to the logical display ids in its user zone. Cleared on
     *        entry; stays empty when an error is returned.
     * @return ERR_OK on success; ERR_ACCOUNT_COMMON_FILE_READ_FAILED after the
     *         read retry for this query fails; ERR_ACCOUNT_COMMON_BAD_JSON_FORMAT_ERROR
     *         if the loaded configuration has an invalid format;
     *         ERR_ACCOUNT_COMMON_DISPLAY_NOT_IN_CONFIG_ERROR when the logical display is
     *         absent from the configuration.
     */
    ErrCode GetDisplayIdsByLogicalId(uint64_t logicalDisplayId, std::vector<uint64_t> &displayIds);

private:
    DisplayUserZoneConfigManager() = default;
    ~DisplayUserZoneConfigManager() = default;
    DISALLOW_COPY_AND_MOVE(DisplayUserZoneConfigManager);

    ErrCode ParseDisplayConfig(const std::string &content);
    ErrCode ParseDisplayNodes(_xmlNode *root);
    ErrCode CollectDisplayNodes(_xmlNode *root, std::vector<DisplayConfigInfo> &displays,
        std::map<uint64_t, uint64_t> &physicalToLogical,
        std::map<uint64_t, uint64_t> &userZoneIdByLogicalId);
    ErrCode ResolveDisplayUserZones(const std::vector<DisplayConfigInfo> &displays,
        const std::map<uint64_t, uint64_t> &physicalToLogical,
        const std::map<uint64_t, uint64_t> &userZoneIdByLogicalId);
    ErrCode ParseDisplayNodeAttributes(_xmlNode *node, DisplayConfigInfo &info,
        uint64_t &userZoneId, bool &hasUserZoneId);
    ErrCode FinalizeParsedConfig();
    ErrCode ValidateParsedConfig();
    ErrCode ReadConfigContent(std::string &content);
    ErrCode EnsureConfigReady();

    void ClearParsedData();
    bool ParseUInt64(const std::string &value, uint64_t &result);

private:
    mutable std::mutex mutex_;
    std::map<uint64_t, DisplayConfigInfo> logicalIdMap_;
    std::map<uint64_t, std::vector<uint64_t>> userZoneMap_;
    bool initOk_ = false;
};
}  // namespace AccountSA
}  // namespace OHOS

#endif // OS_ACCOUNT_SERVICES_ACCOUNTMGR_INCLUDE_OSACCOUNT_DISPLAY_USER_ZONE_CONFIG_DISPLAY_USER_ZONE_CONFIG_MANAGER_H
