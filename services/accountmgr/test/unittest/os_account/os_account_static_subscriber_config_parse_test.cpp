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

#include <gtest/gtest.h>
#include <fstream>
#include "os_account_static_subscriber_config.h"

namespace OHOS {
namespace AccountSA {
using namespace testing::ext;
namespace {
const std::string VALID_CONFIG_SO_RELATIVE_PATH = "valid_with_target_symbol.z.so";
const std::string TEST_CONFIG_PATH = "/data/local/tmp/static_subscriber_config_parse_test.json";
const std::string TEST_EMPTY_CONFIG_PATH = "/data/local/tmp/static_subscriber_config_parse_test_empty.json";
const std::string TEST_NOT_EXIST_CONFIG_PATH = "/data/local/tmp/static_subscriber_config_parse_test_not_exist.json";
}

static void WriteConfigFile(const std::string &path, const std::string &content)
{
    std::ofstream outFile(path);
    outFile << content;
}

class OsAccountStaticSubscriberConfigParseTest : public testing::Test {
public:
    static void SetUpTestCase(void) {};
    static void TearDownTestCase(void) {};
    void SetUp() {};
    void TearDown() {};
};

/**
 * @tc.name: ParseStaticSubscriberConfig001
 * @tc.desc: Parse a full-format static subscriber config, all fields are resolved
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OsAccountStaticSubscriberConfigParseTest, ParseStaticSubscriberConfig001, TestSize.Level1)
{
    auto configs = OsAccountStaticSubscriberConfig::Parse(
        "{\"subscribers\": ["
        "  {\"name\": \"parse_sub\", \"events\": [\"SWITCHING\"],"
        "   \"soPath\": \"" + VALID_CONFIG_SO_RELATIVE_PATH + "\", \"withHandshake\": true}"
        "],"
        "\"timeout\": [{\"SWITCHING\": 800}]"
        "}");
    ASSERT_EQ(configs.subscribers.size(), 1);
    EXPECT_EQ(configs.subscribers[0].name, "parse_sub");
    EXPECT_EQ(configs.subscribers[0].states.count(OsAccountState::SWITCHING), 1);
    EXPECT_EQ(configs.subscribers[0].soPath, VALID_CONFIG_SO_RELATIVE_PATH);
    EXPECT_TRUE(configs.subscribers[0].withHandshake);
    EXPECT_EQ(configs.stateTimeouts.at(OsAccountState::SWITCHING), 800);
}

/**
 * @tc.name: ParseStaticSubscriberConfig002
 * @tc.desc: Parse an invalid json content, no config is returned
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OsAccountStaticSubscriberConfigParseTest, ParseStaticSubscriberConfig002, TestSize.Level1)
{
    EXPECT_TRUE(OsAccountStaticSubscriberConfig::Parse("not a json").subscribers.empty());
}

/**
 * @tc.name: ParseStaticSubscriberConfig003
 * @tc.desc: Parse a json without subscribers array, no subscriber is returned but the state timeout is kept
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OsAccountStaticSubscriberConfigParseTest, ParseStaticSubscriberConfig003, TestSize.Level1)
{
    SubscriberConfigData configs = OsAccountStaticSubscriberConfig::Parse(
        "{\"timeout\": [{\"SWITCHING\": 800}]}");
    EXPECT_TRUE(configs.subscribers.empty());
    EXPECT_EQ(configs.stateTimeouts.at(OsAccountState::SWITCHING), 800);
}

/**
 * @tc.name: ParseStaticSubscriberConfig004
 * @tc.desc: A waitTimeMs above the 10s cap is clamped in the state timeout table
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OsAccountStaticSubscriberConfigParseTest, ParseStaticSubscriberConfig004, TestSize.Level1)
{
    SubscriberConfigData configs = OsAccountStaticSubscriberConfig::Parse(
        "{\"subscribers\": ["
        "  {\"name\": \"parse_sub\", \"events\": [\"SWITCHING\"],"
        "   \"soPath\": \"" + VALID_CONFIG_SO_RELATIVE_PATH + "\", \"withHandshake\": true}"
        "],"
        "\"timeout\": [{\"SWITCHING\": 2147483647}]}");
    ASSERT_EQ(configs.subscribers.size(), 1);
    EXPECT_EQ(configs.stateTimeouts.at(OsAccountState::SWITCHING), 10000);
}

/**
 * @tc.name: IsPathAllowed001
 * @tc.desc: Only system library paths without a ".." component are allowed
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OsAccountStaticSubscriberConfigParseTest, IsPathAllowed001, TestSize.Level1)
{
    EXPECT_TRUE(OsAccountStaticSubscriberConfig::IsPathAllowed("/system/lib/valid.z.so"));
    EXPECT_TRUE(OsAccountStaticSubscriberConfig::IsPathAllowed("/system/lib64/valid.z.so"));
    EXPECT_FALSE(OsAccountStaticSubscriberConfig::IsPathAllowed("/system/lib/../etc/evil.z.so"));
    EXPECT_FALSE(OsAccountStaticSubscriberConfig::IsPathAllowed("/system/lib64/.."));
    EXPECT_FALSE(OsAccountStaticSubscriberConfig::IsPathAllowed("/data/local/tmp/evil.z.so"));
    EXPECT_FALSE(OsAccountStaticSubscriberConfig::IsPathAllowed("/system/lib/../lib64/valid.z.so"));
}

/**
 * @tc.name: IsSupportedState001
 * @tc.desc: Only states in STATE_NAME_MAP are config-supported waitable handshake states;
 *           the publish fork and PublishSync's ready-future contract key off this
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OsAccountStaticSubscriberConfigParseTest, IsSupportedState001, TestSize.Level1)
{
    EXPECT_TRUE(OsAccountStaticSubscriberConfig::IsSupportedState(OsAccountState::SWITCHING));
    EXPECT_FALSE(OsAccountStaticSubscriberConfig::IsSupportedState(OsAccountState::LOCKING));
    EXPECT_FALSE(OsAccountStaticSubscriberConfig::IsSupportedState(OsAccountState::LOCKED));
    EXPECT_FALSE(OsAccountStaticSubscriberConfig::IsSupportedState(OsAccountState::STOPPING));
    EXPECT_FALSE(OsAccountStaticSubscriberConfig::IsSupportedState(OsAccountState::ACTIVATED));
}

/**
 * @tc.name: LoadStaticSubscriberConfig001
 * @tc.desc: Load a valid config file, the parsed config is returned
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OsAccountStaticSubscriberConfigParseTest, LoadStaticSubscriberConfig001, TestSize.Level1)
{
    WriteConfigFile(TEST_CONFIG_PATH,
        "{\"subscribers\": ["
        "  {\"name\": \"parse_sub\", \"events\": [\"SWITCHING\"],"
        "   \"soPath\": \"" + VALID_CONFIG_SO_RELATIVE_PATH + "\", \"withHandshake\": true}"
        "]}");
    auto configs = OsAccountStaticSubscriberConfig::Load(TEST_CONFIG_PATH);
    ASSERT_EQ(configs.subscribers.size(), 1);
    EXPECT_EQ(configs.subscribers[0].name, "parse_sub");
    EXPECT_TRUE(configs.subscribers[0].withHandshake);
    EXPECT_TRUE(configs.stateTimeouts.empty()); // unconfigured timeout: no wait
}

/**
 * @tc.name: LoadStaticSubscriberConfig002
 * @tc.desc: Load a config file which does not exist, no config is returned
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OsAccountStaticSubscriberConfigParseTest, LoadStaticSubscriberConfig002, TestSize.Level1)
{
    EXPECT_TRUE(OsAccountStaticSubscriberConfig::Load(TEST_NOT_EXIST_CONFIG_PATH).subscribers.empty());
}

/**
 * @tc.name: LoadStaticSubscriberConfig003
 * @tc.desc: Load an empty config file, no config is returned
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(OsAccountStaticSubscriberConfigParseTest, LoadStaticSubscriberConfig003, TestSize.Level1)
{
    WriteConfigFile(TEST_EMPTY_CONFIG_PATH, "");
    EXPECT_TRUE(OsAccountStaticSubscriberConfig::Load(TEST_EMPTY_CONFIG_PATH).subscribers.empty());
}
} // AccountSA
} // OHOS