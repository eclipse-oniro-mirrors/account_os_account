/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#ifndef OS_ACCOUNT_SERVICE_BUNDLE_MANAGER_ADAPTER_PROXY_H
#define OS_ACCOUNT_SERVICE_BUNDLE_MANAGER_ADAPTER_PROXY_H

#include <string>
#include <vector>

#include "application_info.h"
#include "bundle_constants.h"
#include "bundle_framework_core_ipc_interface_code.h"
#include "bundle_installer_interface.h"
#include "bundle_mgr_interface.h"
#include "bundle_pack_info.h"
#include "bundle_status_callback_interface.h"
#include "bundle_user_mgr_interface.h"
#include "clean_cache_callback_interface.h"
#include "common_event_info.h"
#include "distributed_bundle_info.h"
#include "extension_ability_info.h"
#include "iremote_proxy.h"
#include "json_utils.h"
#include "permission_define.h"
#include "shortcut_info.h"

namespace OHOS {
namespace AccountSA {
using namespace AppExecFwk;
using namespace AAFwk;
class BundleManagerAdapterProxy : public IRemoteProxy<IBundleMgr> {
public:
    explicit BundleManagerAdapterProxy(const sptr<IRemoteObject> &impl);
    ~BundleManagerAdapterProxy() override;

    /**
     * @brief Obtains the BundleInfo based on a given bundle name through the proxy object.
     * @param bundleName Indicates the application bundle name to be queried.
     * @param flags Indicates the information contained in the BundleInfo object to be returned.
     * @param bundleInfo Indicates the obtained BundleInfo object.
     * @param userId Indicates the user ID.
     * @return Returns true if the BundleInfo is successfully obtained; returns false otherwise.
     */
    bool GetBundleInfo(const std::string &bundleName, BundleFlag flags,
        BundleInfo &bundleInfo, int32_t userId = Constants::UNSPECIFIED_USERID) override;

    /**
     * @brief Obtains the application UID based on the given bundle name and user ID through the proxy object.
     * @param bundleName Indicates the bundle name of the application.
     * @param userId Indicates the user ID.
     * @return Returns the uid if successfully obtained; returns -1 otherwise.
     */
    int GetUidByBundleName(const std::string &bundleName, const int userId) override;

    /** @brief Obtains the bundle name of a specified application based on the given UID through the proxy object.
     * @param uid Indicates the uid.
     * @param bundleName Indicates the obtained bundle name.
     * @return Returns true if the bundle name is successfully obtained; returns false otherwise.
     */
    ErrCode GetNameForUid(const int uid, std::string &bundleName) override;

    /**
     * @brief Obtains the interface used to create or delete user.
     * @return Returns a pointer to IBundleUserMgr class if exist; returns nullptr otherwise.
     */
    sptr<IBundleUserMgr> GetBundleUserMgr() override;

    /**
     * @brief Query extension info.
     * @param Want Indicates the information of extension info.
     * @param flag Indicates the query flag which will filter any specified stuff in the extension info.
     * @param userId Indicates the userId in the system.
     * @param extensionInfos Indicates the obtained extensions.
     * @return Returns true if this function is successfully called; returns false otherwise.
     */
    bool QueryExtensionAbilityInfos(const Want &want, const int32_t &flag, const int32_t &userId,
        std::vector<ExtensionAbilityInfo> &extensionInfos) override;

    bool QueryExtensionAbilityInfos(const Want &want, const ExtensionAbilityType &extensionType,
        const int32_t &flag, const int32_t &userId, std::vector<ExtensionAbilityInfo> &extensionInfos) override;

    template<typename T>
    ErrCode GetParcelableInfosWithErrCode(BundleMgrInterfaceCode code, MessageParcel &data,
        std::vector<T> &parcelableInfos);

    bool QueryAbilityInfos(
        const Want &want, int32_t flags, int32_t userId, std::vector<AbilityInfo> &abilityInfos) override;

private:
    bool ParseStr(const char *buf, const int itemLen, int index, std::string &result);
    bool ParseExtensionInfo(std::string infoStr, ExtensionAbilityInfo &extensionInfo);
    bool ParseExtensionAbilityInfos(
        CJsonUnique &jsonObject, std::vector<ExtensionAbilityInfo> &extensionInfos);
    template<typename T>
    bool ParseInfo(std::string &infoStr, T &info);

    /**
     * @brief Send a command message from the proxy object.
     * @param code Indicates the message code to be sent.
     * @param data Indicates the objects to be sent.
     * @param reply Indicates the reply to be sent;
     * @return Returns true if message send successfully; returns false otherwise.
     */
    bool SendTransactCmd(BundleMgrInterfaceCode code, MessageParcel &data, MessageParcel &reply);
    /**
     * @brief Send a command message and then get a parcelable information object from the reply.
     * @param code Indicates the message code to be sent.
     * @param data Indicates the objects to be sent.
     * @param parcelableInfo Indicates the object to be got;
     * @return Returns true if objects get successfully; returns false otherwise.
     */
    template <typename T>
    bool GetParcelableInfo(BundleMgrInterfaceCode code, MessageParcel &data, T &parcelableInfo);

    template <typename T>
    bool GetParcelableFromAshmem(MessageParcel &reply, T &parcelableInfo);

    template<typename T>
    bool GetBigParcelableInfo(
        BundleMgrInterfaceCode code, MessageParcel &data, T &parcelableInfo);

    template <typename T>
    ErrCode GetParcelableInfoWithErrCode(BundleMgrInterfaceCode code, MessageParcel &data, T &parcelableInfo);
    /**
     * @brief Send a command message and then get a vector of parcelable information objects from the reply.
     * @param code Indicates the message code to be sent.
     * @param data Indicates the objects to be sent.
     * @param parcelableInfos Indicates the vector objects to be got;
     * @return Returns true if the vector get successfully; returns false otherwise.
     */
    template <typename T>
    bool GetParcelableInfos(BundleMgrInterfaceCode code, MessageParcel &data, std::vector<T> &parcelableInfos);
    template <typename T>
    bool ParseAshmem(int32_t infoSize, const char* dataStr, int32_t offset, std::vector<T> &parcelableInfos);
    /**
     * @brief Send a command message and then get a vector of parcelable information objects from the reply Ashmem.
     * @param code Indicates the message code to be sent.
     * @param data Indicates the objects to be sent.
     * @param parcelableInfos Indicates the vector objects to be got;
     * @return Returns true if the vector get successfully; returns false otherwise.
     */
    template <typename T>
    bool GetParcelableInfosFromAshmem(
        BundleMgrInterfaceCode code, MessageParcel &data, std::vector<T> &parcelableInfos);

    template<typename T>
    bool GetVectorFromParcelIntelligent(
        BundleMgrInterfaceCode code, MessageParcel &data, std::vector<T> &parcelableInfos);

    template<typename T>
    ErrCode InnerGetVectorFromParcelIntelligent(
        MessageParcel &reply, std::vector<T> &parcelableInfos);

    bool SendData(void *&buffer, size_t size, const void *data);
    bool GetData(void *&buffer, size_t size, const void *data);

    template <typename T>
    bool GetParcelInfo(BundleMgrInterfaceCode code, MessageParcel &data, T &parcelInfo);

    template <typename T>
    bool InnerGetParcelInfo(MessageParcel &reply, T &parcelInfo);

    static inline BrokerDelegator<BundleManagerAdapterProxy> delegator_;
};
}  // namespace AccountSA
}  // namespace OHOS
#endif  // OS_ACCOUNT_SERVICE_BUNDLE_MANAGER_ADAPTER_PROXY_H
