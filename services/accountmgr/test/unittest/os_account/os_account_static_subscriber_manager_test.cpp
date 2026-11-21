/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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

#include <gtest/gtest.h>
#include <fstream>
#include <unistd.h>
#include "dlfcn.h"
#include "account_log_wrapper.h"
#define private public
#include "os_account_static_subscriber_manager.h"
#undef private

namespace OHOS {
namespace AccountSA {
using namespace testing::ext;
using namespace OHOS::AccountSA;
using namespace OHOS;
using namespace AccountSA;
namespace {
constexpr int32_t TEST_ACCOUNT_ID = 101;
const std::string INVALID_SO_PATH = "invalid.z.so";
const std::string VALID_WITH_TARGET_SYMBOL_SO_PATH = "valid_with_target_symbol.z.so";
const std::string VALID_WITHOUT_TARGET_SYMBOL_SO_PATH = "valid_without_target_symbol.z.so";
#ifdef OS_ACCOUNT_SUPPORT_STATIC_SUBSCRIPTION
constexpr int32_t SYNC_BLOCK_SECONDS = 1;
const std::string VALID_CONFIG_SO_RELATIVE_PATH = "valid_with_target_symbol.z.so";
const std::string VALID_CONFIG_SO_PATH_LIB = "/system/lib/valid_with_target_symbol.z.so";
const std::string VALID_CONFIG_SO_PATH_LIB64 = "/system/lib64/valid_with_target_symbol.z.so";
const std::string EXISTING_SUBSCRIBER_SO_PATH = "/system/lib/existing_subscriber.z.so";
const std::string TEST_CONFIG_PATH = "/data/local/tmp/static_subscriber_config_test.json";
const std::string NOT_EXIST_CONFIG_PATH = "/data/local/tmp/static_subscriber_config_not_exist.json";
#endif
}
#ifdef __cplusplus
extern "C" {
#endif
static bool g_withSymbol = true;
static bool g_withoutSymbol = false;
static void* g_ptrWithSymbol = &g_withSymbol;
static void* g_ptrWithoutSymbol = &g_withoutSymbol;

int dlclose(void *handle)
{
    if (handle == nullptr) {
        ACCOUNT_LOGI("Handle is null.");
        errno = 1;
        return -1;
    }
    errno = 0;
    return 0;
}

char *dlerror(void)
{
    if (errno == 0) {
        return nullptr;
    }
    errno = 0;
    return const_cast<char *>("Failed to dlopen or dlsym");
}

int32_t OnOsAccountStateChanged(const COsAccountStateData *stateData)
{
    if (stateData->state == OsAccountState::CREATING) {
        return 0;
    }
    return -1;
}

#ifdef OS_ACCOUNT_SUPPORT_STATIC_SUBSCRIPTION
int32_t BlockOnOsAccountStateChanged(const COsAccountStateData *stateData)
{
    sleep(SYNC_BLOCK_SECONDS);
    return 0;
}
#endif

void *dlsym(void *__restrict handle, const char *__restrict symbol)
{
    if (handle != nullptr && *(reinterpret_cast<bool *>(handle))) {
        ACCOUNT_LOGI("Dlsym successfully, symbol:  %{public}s", symbol);
        return reinterpret_cast<void *>(OnOsAccountStateChanged);
    }
    ACCOUNT_LOGE("Invalid symbol: %{public}s", symbol);
    return nullptr;
}

void *dlopen(const char* path, int flag)
{
    if (strcmp(path, "/system/lib//system/lib64/absolute.z.so") == 0 ||
        strcmp(path, "/system/lib64/../../data/local/tmp/evil.z.so") == 0) {
        return g_ptrWithSymbol;
    }
    if (strcmp(path, VALID_WITH_TARGET_SYMBOL_SO_PATH.c_str()) == 0) {
        ACCOUNT_LOGI("Dlopen successfully, path: %{public}s", VALID_WITH_TARGET_SYMBOL_SO_PATH.c_str());
        return g_ptrWithSymbol;
    }
    if (strcmp(path, VALID_CONFIG_SO_PATH_LIB.c_str()) == 0) {
        ACCOUNT_LOGI("Dlopen successfully, path: %{public}s", VALID_CONFIG_SO_PATH_LIB.c_str());
        return g_ptrWithSymbol;
    }
    if (strcmp(path, VALID_CONFIG_SO_PATH_LIB64.c_str()) == 0) {
        ACCOUNT_LOGI("Dlopen successfully, path: %{public}s", VALID_CONFIG_SO_PATH_LIB64.c_str());
        return g_ptrWithSymbol;
    }
    if (strcmp(path, VALID_WITHOUT_TARGET_SYMBOL_SO_PATH.c_str()) == 0) {
        ACCOUNT_LOGI("Dlopen successfully, path: %{public}s", VALID_WITHOUT_TARGET_SYMBOL_SO_PATH.c_str());
        return g_ptrWithoutSymbol;
    }
    ACCOUNT_LOGE("Dlopen failed");
    return nullptr;
}
#ifdef __cplusplus
}
#endif

class OsAccountStaticSubscriberManagerTest : public testing::Test {
public:
    static void SetUpTestCase(void) {};
    static void TearDownTestCase(void) {};
    void SetUp();
    void TearDown() {};
};

void OsAccountStaticSubscriberManagerTest::SetUp()
{
    OsAccountStaticSubscriberManager::GetInstance().staticSubscribers_.clear();
    OsAccountStaticSubscriberManager::GetInstance().state2Subscribers_.clear();
}

/**
 * @tc.name: Publish001
 * @tc.desc: Test when target subscriber set is empty
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OsAccountStaticSubscriberManagerTest, Publish001, TestSize.Level1)
{
    auto& manager = OsAccountStaticSubscriberManager::GetInstance();
    EXPECT_EQ(manager.state2Subscribers_.find(OsAccountState::CREATED), manager.state2Subscribers_.end());
    EXPECT_EQ(ERR_OK, manager.Publish(TEST_ACCOUNT_ID, OsAccountState::CREATED, TEST_ACCOUNT_ID));
}

/**
 * @tc.name: Publish002
 * @tc.desc: Test when target subscriber set not empty
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OsAccountStaticSubscriberManagerTest, Publish002, TestSize.Level1)
{
    auto& manager = OsAccountStaticSubscriberManager::GetInstance();
    auto subscriber = std::make_shared<StaticSubscriber>();
    subscriber->handle = g_ptrWithSymbol;
    subscriber->callback = reinterpret_cast<void *>(OnOsAccountStateChanged);
    manager.state2Subscribers_[OsAccountState::CREATING] = { subscriber, nullptr };
    EXPECT_NE(manager.state2Subscribers_.find(OsAccountState::CREATING), manager.state2Subscribers_.end());
    EXPECT_EQ(ERR_OK, manager.Publish(TEST_ACCOUNT_ID, OsAccountState::CREATING, TEST_ACCOUNT_ID));
    manager.state2Subscribers_[OsAccountState::CREATED] = {};
    EXPECT_NE(manager.state2Subscribers_.find(OsAccountState::CREATED), manager.state2Subscribers_.end());
    EXPECT_EQ(ERR_OK, manager.Publish(TEST_ACCOUNT_ID, OsAccountState::CREATED, TEST_ACCOUNT_ID));
}

/**
 * @tc.name: PublishToSubscriber001
 * @tc.desc: Fail to publish with invalid parameter
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OsAccountStaticSubscriberManagerTest, PublishToSubscriber001, TestSize.Level1)
{
    COsAccountStateData data;
    EXPECT_EQ(OsAccountStaticSubscriberManager::GetInstance().PublishToSubscriber(nullptr, data),
        ERR_ACCOUNT_COMMON_INVALID_PARAMETER);
    auto subscriber = std::make_shared<StaticSubscriber>();
    EXPECT_EQ(OsAccountStaticSubscriberManager::GetInstance().PublishToSubscriber(subscriber, data),
        ERR_ACCOUNT_COMMON_INVALID_PARAMETER);
}

/**
 * @tc.name: PublishToSubscriber002
 * @tc.desc: Publish successfully, but callback failed
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OsAccountStaticSubscriberManagerTest, PublishToSubscriber002, TestSize.Level1)
{
    COsAccountStateData data;
    data.state = OsAccountState::CREATED;
    auto subscriber = std::make_shared<StaticSubscriber>();
    subscriber->callback = reinterpret_cast<void *>(OnOsAccountStateChanged);
    EXPECT_EQ(OsAccountStaticSubscriberManager::GetInstance().PublishToSubscriber(subscriber, data), -1);
}

/**
 * @tc.name: PublishToSubscriber003
 * @tc.desc: Publish and callback successfully
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OsAccountStaticSubscriberManagerTest, PublishToSubscriber003, TestSize.Level1)
{
    COsAccountStateData data;
    data.state = OsAccountState::CREATING;
    auto subscriber = std::make_shared<StaticSubscriber>();
    subscriber->callback = reinterpret_cast<void *>(OnOsAccountStateChanged);
    EXPECT_EQ(OsAccountStaticSubscriberManager::GetInstance().PublishToSubscriber(subscriber, data), ERR_OK);
}

/**
 * @tc.name: ParseStaticSubscriber001
 * @tc.desc: Parse invalid subscriber
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OsAccountStaticSubscriberManagerTest, ParseStaticSubscriber001, TestSize.Level1)
{
    auto &manager = OsAccountStaticSubscriberManager::GetInstance();
    EXPECT_EQ(manager.ParseStaticSubscriber(INVALID_SO_PATH), nullptr);
    EXPECT_EQ(manager.ParseStaticSubscriber(VALID_WITHOUT_TARGET_SYMBOL_SO_PATH), nullptr);
}

/**
 * @tc.name: ParseStaticSubscriber002
 * @tc.desc: Parse duplicated subscriber
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OsAccountStaticSubscriberManagerTest, ParseStaticSubscriber002, TestSize.Level1)
{
    auto &manager = OsAccountStaticSubscriberManager::GetInstance();
    auto subscriber = std::make_shared<StaticSubscriber>();
    manager.staticSubscribers_[VALID_WITH_TARGET_SYMBOL_SO_PATH] = subscriber;
    EXPECT_EQ(manager.ParseStaticSubscriber(VALID_WITH_TARGET_SYMBOL_SO_PATH), subscriber);
}

/**
 * @tc.name: ParseStaticSubscriber003
 * @tc.desc: Parse valid subscriber
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OsAccountStaticSubscriberManagerTest, ParseStaticSubscriber003, TestSize.Level1)
{
    auto &manager = OsAccountStaticSubscriberManager::GetInstance();
    auto subscriber = manager.ParseStaticSubscriber(VALID_WITH_TARGET_SYMBOL_SO_PATH);
    EXPECT_NE(subscriber, nullptr);
    EXPECT_EQ(subscriber->callback, reinterpret_cast<void *>(OnOsAccountStateChanged));
}

/**
 * @tc.name: Init001
 * @tc.desc: Init subscriber with config
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OsAccountStaticSubscriberManagerTest, Init001, TestSize.Level1)
{
    auto &manager = OsAccountStaticSubscriberManager::GetInstance();
    EXPECT_EQ(manager.staticSubscribers_.size(), 0);
    std::map<OsAccountState, std::set<std::string>> config;
    manager.Init(config);
    EXPECT_EQ(manager.staticSubscribers_.size(), 0);
    config[OsAccountState::CREATING] = { VALID_WITH_TARGET_SYMBOL_SO_PATH, VALID_WITHOUT_TARGET_SYMBOL_SO_PATH};
    manager.Init(config);
    EXPECT_EQ(manager.staticSubscribers_.size(), 1);
    EXPECT_EQ(manager.state2Subscribers_.size(), 1);
    auto it = manager.state2Subscribers_.find(OsAccountState::CREATING);
    ASSERT_NE(it, manager.state2Subscribers_.end());
    EXPECT_EQ(it->second.size(), 1);
}

#ifdef OS_ACCOUNT_SUPPORT_STATIC_SUBSCRIPTION
static void WriteConfigFile(const std::string &path, const std::string &content)
{
    std::ofstream outFile(path);
    outFile << content;
}

/**
 * @tc.name: LoadConfig001
 * @tc.desc: Load config when file does not exist
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OsAccountStaticSubscriberManagerTest, LoadConfig001, TestSize.Level1)
{
    auto &manager = OsAccountStaticSubscriberManager::GetInstance();
    manager.LoadSubscribers(OsAccountStaticSubscriberConfig::Load(NOT_EXIST_CONFIG_PATH).subscribers);
    EXPECT_EQ(manager.staticSubscribers_.size(), 0);
}

/**
 * @tc.name: LoadConfig002
 * @tc.desc: Load config with valid subscriber, unknown state, and missing fields
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OsAccountStaticSubscriberManagerTest, LoadConfig002, TestSize.Level1)
{
    WriteConfigFile(TEST_CONFIG_PATH,
        "{\"subscribers\": ["
        "  {\"name\": \"test_sub\", \"events\": [\"SWITCHING\"],"
        "   \"soPath\": \"" + VALID_CONFIG_SO_RELATIVE_PATH + "\", \"withHandshake\": true},"
        "  {\"name\": \"unknown_state_sub\", \"events\": [\"UNKNOWN_STATE\"],"
        "   \"soPath\": \"" + VALID_CONFIG_SO_RELATIVE_PATH + "\", \"withHandshake\": false},"
        "  {\"name\": \"no_soPath_sub\", \"events\": [\"SWITCHING\"],"
        "   \"soPath\": \"\", \"withHandshake\": false}"
        "]}");
    auto &manager = OsAccountStaticSubscriberManager::GetInstance();
    manager.LoadSubscribers(OsAccountStaticSubscriberConfig::Load(TEST_CONFIG_PATH).subscribers);
    EXPECT_EQ(manager.state2Subscribers_.count(OsAccountState::SWITCHING), 1);
    EXPECT_EQ(manager.staticSubscribers_.size(), 1);
    auto it = manager.state2Subscribers_.find(OsAccountState::SWITCHING);
    ASSERT_NE(it, manager.state2Subscribers_.end());
    EXPECT_EQ(it->second.size(), 1);
    auto subscriber = *it->second.begin();
    EXPECT_TRUE(subscriber->withHandshake);
    EXPECT_EQ(subscriber->name, "test_sub");
}

/**
 * @tc.name: LoadConfig003
 * @tc.desc: Load config with invalid json content
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OsAccountStaticSubscriberManagerTest, LoadConfig003, TestSize.Level1)
{
    WriteConfigFile(TEST_CONFIG_PATH, "not a json");
    auto &manager = OsAccountStaticSubscriberManager::GetInstance();
    manager.LoadSubscribers(OsAccountStaticSubscriberConfig::Load(TEST_CONFIG_PATH).subscribers);
    EXPECT_EQ(manager.staticSubscribers_.size(), 0);
}

/**
 * @tc.name: LoadConfig004
 * @tc.desc: Load config with path traversal in soPath
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OsAccountStaticSubscriberManagerTest, LoadConfig004, TestSize.Level1)
{
    WriteConfigFile(TEST_CONFIG_PATH,
        "{\"subscribers\": ["
        "  {\"name\": \"traversal_sub\", \"events\": [\"SWITCHING\"],"
        "   \"soPath\": \"../../data/local/tmp/evil.z.so\", \"withHandshake\": true}"
        "]}");
    auto &manager = OsAccountStaticSubscriberManager::GetInstance();
    manager.LoadSubscribers(OsAccountStaticSubscriberConfig::Load(TEST_CONFIG_PATH).subscribers);
    EXPECT_EQ(manager.state2Subscribers_.count(OsAccountState::SWITCHING), 0);
    EXPECT_EQ(manager.staticSubscribers_.size(), 0);
}

/**
 * @tc.name: LoadConfig005
 * @tc.desc: Load config merges with existing subscribers instead of overwriting
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OsAccountStaticSubscriberManagerTest, LoadConfig005, TestSize.Level1)
{
    auto &manager = OsAccountStaticSubscriberManager::GetInstance();
    auto existingSub = std::make_shared<StaticSubscriber>();
    existingSub->name = "existing";
    existingSub->withHandshake = true;
    existingSub->handle = g_ptrWithSymbol;
    existingSub->callback = reinterpret_cast<void *>(OnOsAccountStateChanged);
    manager.staticSubscribers_[EXISTING_SUBSCRIBER_SO_PATH] = existingSub;
    manager.state2Subscribers_[OsAccountState::SWITCHING] = { existingSub };
    WriteConfigFile(TEST_CONFIG_PATH,
        "{\"subscribers\": ["
        "  {\"name\": \"config_sub\", \"events\": [\"SWITCHING\"],"
        "   \"soPath\": \"" + VALID_CONFIG_SO_RELATIVE_PATH + "\", \"withHandshake\": true}"
        "]}");
    manager.LoadSubscribers(OsAccountStaticSubscriberConfig::Load(TEST_CONFIG_PATH).subscribers);
    auto it = manager.state2Subscribers_.find(OsAccountState::SWITCHING);
    ASSERT_NE(it, manager.state2Subscribers_.end());
    EXPECT_EQ(it->second.size(), 2); // existing + config subscriber merged
}

/**
 * @tc.name: LoadConfig006
 * @tc.desc: Load config with top-level timeout array, the state timeout table is filled
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OsAccountStaticSubscriberManagerTest, LoadConfig006, TestSize.Level1)
{
    WriteConfigFile(TEST_CONFIG_PATH,
        "{\"subscribers\": ["
        "  {\"name\": \"timeout_sub\", \"events\": [\"SWITCHING\"],"
        "   \"soPath\": \"" + VALID_CONFIG_SO_RELATIVE_PATH + "\", \"withHandshake\": true}"
        "],"
        "\"timeout\": [{\"SWITCHING\": 1200}]"
        "}");
    auto &manager = OsAccountStaticSubscriberManager::GetInstance();
    SubscriberConfigData configs = OsAccountStaticSubscriberConfig::Load(TEST_CONFIG_PATH);
    EXPECT_EQ(configs.stateTimeouts.at(OsAccountState::SWITCHING), 1200);
    manager.LoadSubscribers(configs.subscribers);
    auto it = manager.state2Subscribers_.find(OsAccountState::SWITCHING);
    ASSERT_NE(it, manager.state2Subscribers_.end());
    EXPECT_EQ(it->second.size(), 1); // state timeout stays in the table, not on the subscriber
}

/**
 * @tc.name: LoadConfig007
 * @tc.desc: Load config with duplicate subscriber name, the second one is rejected
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OsAccountStaticSubscriberManagerTest, LoadConfig007, TestSize.Level1)
{
    WriteConfigFile(TEST_CONFIG_PATH,
        "{\"subscribers\": ["
        "  {\"name\": \"dup_sub\", \"events\": [\"SWITCHING\"],"
        "   \"soPath\": \"" + VALID_CONFIG_SO_RELATIVE_PATH + "\", \"withHandshake\": true},"
        "  {\"name\": \"dup_sub\", \"events\": [\"SWITCHING\"],"
        "   \"soPath\": \"" + VALID_CONFIG_SO_RELATIVE_PATH + "\", \"withHandshake\": false}"
        "]}");
    auto &manager = OsAccountStaticSubscriberManager::GetInstance();
    manager.LoadSubscribers(OsAccountStaticSubscriberConfig::Load(TEST_CONFIG_PATH).subscribers);
    auto it = manager.state2Subscribers_.find(OsAccountState::SWITCHING);
    ASSERT_NE(it, manager.state2Subscribers_.end());
    ASSERT_EQ(it->second.size(), 1);
    EXPECT_TRUE((*it->second.begin())->withHandshake);
}

/**
 * @tc.name: LoadConfig008
 * @tc.desc: Load config with an absolute soPath, the entry is rejected
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OsAccountStaticSubscriberManagerTest, LoadConfig008, TestSize.Level1)
{
    WriteConfigFile(TEST_CONFIG_PATH,
        "{\"subscribers\": ["
        "  {\"name\": \"abs_path_sub\", \"events\": [\"SWITCHING\"],"
        "   \"soPath\": \"/system/lib64/absolute.z.so\", \"withHandshake\": true}"
        "]}");
    auto &manager = OsAccountStaticSubscriberManager::GetInstance();
    manager.LoadSubscribers(OsAccountStaticSubscriberConfig::Load(TEST_CONFIG_PATH).subscribers);
    EXPECT_EQ(manager.state2Subscribers_.count(OsAccountState::SWITCHING), 0);
    EXPECT_EQ(manager.staticSubscribers_.size(), 0);
}

/**
 * @tc.name: LoadConfig009
 * @tc.desc: Load config with a non-positive timeout, the entry is rejected and no wait is configured
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OsAccountStaticSubscriberManagerTest, LoadConfig009, TestSize.Level1)
{
    WriteConfigFile(TEST_CONFIG_PATH,
        "{\"subscribers\": ["
        "  {\"name\": \"timeout_sub\", \"events\": [\"SWITCHING\"],"
        "   \"soPath\": \"" + VALID_CONFIG_SO_RELATIVE_PATH + "\", \"withHandshake\": true}"
        "],"
        "\"timeout\": [{\"SWITCHING\": 0}]"
        "}");
    auto &manager = OsAccountStaticSubscriberManager::GetInstance();
    SubscriberConfigData configs = OsAccountStaticSubscriberConfig::Load(TEST_CONFIG_PATH);
    // Parse-level assertion: the non-positive entry is rejected here, before any manager state.
    // (No manager-level assertion: stateTimeouts_ is constructor-loaded from the device config
    // and SetUp does not reset it, so a device-deployed SWITCHING timeout would fail spuriously.)
    EXPECT_TRUE(configs.stateTimeouts.find(OsAccountState::SWITCHING) == configs.stateTimeouts.end());
    manager.LoadSubscribers(configs.subscribers);
    auto it = manager.state2Subscribers_.find(OsAccountState::SWITCHING);
    ASSERT_NE(it, manager.state2Subscribers_.end());
    EXPECT_EQ(it->second.size(), 1);
}

/**
 * @tc.name: LoadConfig010
 * @tc.desc: Load config with an invalid timeout entry, the entry is skipped and no wait is configured
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OsAccountStaticSubscriberManagerTest, LoadConfig010, TestSize.Level1)
{
    WriteConfigFile(TEST_CONFIG_PATH,
        "{\"subscribers\": ["
        "  {\"name\": \"bad_timeout_sub\", \"events\": [\"SWITCHING\"],"
        "   \"soPath\": \"" + VALID_CONFIG_SO_RELATIVE_PATH + "\", \"withHandshake\": true}"
        "],"
        "\"timeout\": [{}]"
        "}");
    auto &manager = OsAccountStaticSubscriberManager::GetInstance();
    SubscriberConfigData configs = OsAccountStaticSubscriberConfig::Load(TEST_CONFIG_PATH);
    EXPECT_TRUE(configs.stateTimeouts.empty()); // invalid timeout entry is skipped: no wait
    manager.LoadSubscribers(configs.subscribers);
    auto it = manager.state2Subscribers_.find(OsAccountState::SWITCHING);
    ASSERT_NE(it, manager.state2Subscribers_.end());
    EXPECT_EQ(it->second.size(), 1);
}

/**
 * @tc.name: LoadConfig011
 * @tc.desc: Load config with duplicate soPath but different name, both become independent notifications
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OsAccountStaticSubscriberManagerTest, LoadConfig011, TestSize.Level1)
{
    WriteConfigFile(TEST_CONFIG_PATH,
        "{\"subscribers\": ["
        "  {\"name\": \"path_dup_a\", \"events\": [\"SWITCHING\"],"
        "   \"soPath\": \"" + VALID_CONFIG_SO_RELATIVE_PATH + "\", \"withHandshake\": true},"
        "  {\"name\": \"path_dup_b\", \"events\": [\"SWITCHING\"],"
        "   \"soPath\": \"" + VALID_CONFIG_SO_RELATIVE_PATH + "\", \"withHandshake\": false}"
        "]}");
    auto &manager = OsAccountStaticSubscriberManager::GetInstance();
    manager.LoadSubscribers(OsAccountStaticSubscriberConfig::Load(TEST_CONFIG_PATH).subscribers);
    auto it = manager.state2Subscribers_.find(OsAccountState::SWITCHING);
    ASSERT_NE(it, manager.state2Subscribers_.end());
    ASSERT_EQ(it->second.size(), 2);
    EXPECT_EQ(manager.staticSubscribers_.size(), 1);
    std::map<std::string, bool> handshakeByName;
    for (const auto &sub : it->second) {
        handshakeByName[sub->name] = sub->withHandshake;
    }
    EXPECT_TRUE(handshakeByName["path_dup_a"]);
    EXPECT_FALSE(handshakeByName["path_dup_b"]);
}

/**
 * @tc.name: LoadConfig012
 * @tc.desc: Load config with a subscriber missing the name field, the entry is rejected
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OsAccountStaticSubscriberManagerTest, LoadConfig012, TestSize.Level1)
{
    WriteConfigFile(TEST_CONFIG_PATH,
        "{\"subscribers\": ["
        "  {\"events\": [\"SWITCHING\"],"
        "   \"soPath\": \"" + VALID_CONFIG_SO_RELATIVE_PATH + "\", \"withHandshake\": true}"
        "]}");
    auto &manager = OsAccountStaticSubscriberManager::GetInstance();
    manager.LoadSubscribers(OsAccountStaticSubscriberConfig::Load(TEST_CONFIG_PATH).subscribers);
    EXPECT_EQ(manager.state2Subscribers_.count(OsAccountState::SWITCHING), 0);
    EXPECT_EQ(manager.staticSubscribers_.size(), 0);
}
#endif

/**
 * @tc.name: SyncPublish001
 * @tc.desc: Test SyncPublish when target subscriber set is empty
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OsAccountStaticSubscriberManagerTest, SyncPublish001, TestSize.Level1)
{
    auto &manager = OsAccountStaticSubscriberManager::GetInstance();
    EXPECT_EQ(manager.state2Subscribers_.find(OsAccountState::SWITCHING), manager.state2Subscribers_.end());
    EXPECT_EQ(ERR_OK, manager.SyncPublish(TEST_ACCOUNT_ID, OsAccountState::SWITCHING, TEST_ACCOUNT_ID));
}

#ifdef OS_ACCOUNT_SUPPORT_STATIC_SUBSCRIPTION
/**
 * @tc.name: SyncPublish002
 * @tc.desc: Test SyncPublish when handshake subscriber replies immediately
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OsAccountStaticSubscriberManagerTest, SyncPublish002, TestSize.Level1)
{
    auto &manager = OsAccountStaticSubscriberManager::GetInstance();
    auto subscriber = std::make_shared<StaticSubscriber>();
    subscriber->withHandshake = true;
    subscriber->handle = g_ptrWithSymbol;
    subscriber->callback = reinterpret_cast<void *>(OnOsAccountStateChanged);
    manager.state2Subscribers_[OsAccountState::SWITCHING] = { subscriber };
    manager.stateTimeouts_[OsAccountState::SWITCHING] = 100; // wait budget comes from the state table
    EXPECT_EQ(ERR_OK, manager.SyncPublish(TEST_ACCOUNT_ID, OsAccountState::SWITCHING, TEST_ACCOUNT_ID));
    manager.stateTimeouts_.erase(OsAccountState::SWITCHING);
}

/**
 * @tc.name: SyncPublish003
 * @tc.desc: Test SyncPublish when handshake subscriber blocks longer than timeout
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OsAccountStaticSubscriberManagerTest, SyncPublish003, TestSize.Level1)
{
    auto &manager = OsAccountStaticSubscriberManager::GetInstance();
    auto subscriber = std::make_shared<StaticSubscriber>();
    subscriber->withHandshake = true;
    subscriber->handle = g_ptrWithSymbol;
    subscriber->callback = reinterpret_cast<void *>(BlockOnOsAccountStateChanged);
    manager.state2Subscribers_[OsAccountState::SWITCHING] = { subscriber };
    manager.stateTimeouts_[OsAccountState::SWITCHING] = 100; // wait budget comes from the state table
    EXPECT_EQ(ERR_ACCOUNT_COMMON_OPERATION_TIMEOUT,
        manager.SyncPublish(TEST_ACCOUNT_ID, OsAccountState::SWITCHING, TEST_ACCOUNT_ID));
    manager.stateTimeouts_.erase(OsAccountState::SWITCHING);
}

/**
 * @tc.name: SyncPublish004
 * @tc.desc: Test SyncPublish skips non-handshake subscribers
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OsAccountStaticSubscriberManagerTest, SyncPublish004, TestSize.Level1)
{
    auto &manager = OsAccountStaticSubscriberManager::GetInstance();
    auto subscriber = std::make_shared<StaticSubscriber>();
    subscriber->withHandshake = false; // non-handshake subscriber
    subscriber->handle = g_ptrWithSymbol;
    subscriber->callback = reinterpret_cast<void *>(BlockOnOsAccountStateChanged);
    manager.state2Subscribers_[OsAccountState::SWITCHING] = { subscriber };
    EXPECT_EQ(ERR_OK, manager.SyncPublish(TEST_ACCOUNT_ID, OsAccountState::SWITCHING, TEST_ACCOUNT_ID));
}

/**
 * @tc.name: GetStateWaitTimeMs001
 * @tc.desc: GetStateWaitTimeMs returns the configured timeout for the state and 0 for unconfigured states
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OsAccountStaticSubscriberManagerTest, GetStateWaitTimeMs001, TestSize.Level1)
{
    auto &manager = OsAccountStaticSubscriberManager::GetInstance();
    manager.stateTimeouts_[OsAccountState::SWITCHING] = 700;
    EXPECT_EQ(manager.GetStateWaitTimeMs(OsAccountState::SWITCHING), 700);
    EXPECT_EQ(manager.GetStateWaitTimeMs(OsAccountState::CREATED), 0);
    manager.stateTimeouts_.clear();
}
#endif
} // AccountSA
} // OHOS
