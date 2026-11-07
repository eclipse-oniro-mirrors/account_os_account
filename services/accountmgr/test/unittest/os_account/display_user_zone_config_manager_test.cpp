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

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <thread>
#include <vector>

#include <gtest/gtest.h>
#define private public
#include "osaccount/display_user_zone_config/display_user_zone_config_manager.h"
#undef private

namespace OHOS {
namespace AccountSA {
using namespace testing::ext;

namespace {
// The manager is compiled with ACCOUNT_TEST, so the load path reads a real XML
// file from this writable test directory instead of the system configuration.
const std::string TEST_CONFIG_DIR = "/data/service/el1/public/account/test/";
const std::string TEST_CONFIG_PATH = TEST_CONFIG_DIR + "display_manager_config.xml";

void WriteTestConfig(const std::string &content)
{
    std::ofstream file(TEST_CONFIG_PATH, std::ios::out | std::ios::trunc);
    ASSERT_TRUE(file.is_open());
    file << content;
    file.close();
}

void RemoveTestConfig()
{
    std::remove(TEST_CONFIG_PATH.c_str());
}

constexpr uint64_t PHYS_A = 10;
constexpr uint64_t PHYS_B = 20;
constexpr uint64_t PHYS_C = 30;
constexpr uint64_t LOGICAL_A = 100;
constexpr uint64_t LOGICAL_B = 200;
constexpr uint64_t LOGICAL_C = 300;

// Build one <display> element. userZoneId is emitted only when requested so the
// standalone (own user zone) vs secondary (grouped) branches are both exercised.
std::string DisplayNode(uint64_t physicalId, uint64_t logicalId, const std::string &name,
    bool withUserZoneId = false, uint64_t userZoneId = 0)
{
    std::string node = "<display>"
        "<physicalId>" + std::to_string(physicalId) + "</physicalId>"
        "<logicalId>" + std::to_string(logicalId) + "</logicalId>"
        "<name>" + name + "</name>";
    if (withUserZoneId) {
        node += "<userZoneId>" + std::to_string(userZoneId) + "</userZoneId>";
    }
    node += "</display>";
    return node;
}

std::string WrapConfig(const std::string &displaysBody)
{
    return "<Configs><displays>" + displaysBody + "</displays></Configs>";
}
}  // namespace

class DisplayUserZoneConfigManagerTest : public testing::Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void);
    void SetUp(void);
    void TearDown(void);
};

void DisplayUserZoneConfigManagerTest::SetUpTestCase(void)
{
    mkdir(TEST_CONFIG_DIR.c_str(), S_IRWXU | S_IRWXG | S_IRWXO);
}

void DisplayUserZoneConfigManagerTest::TearDownTestCase(void)
{
    RemoveTestConfig();
}

void DisplayUserZoneConfigManagerTest::SetUp(void)
{
    auto &mgr = DisplayUserZoneConfigManager::GetInstance();
    mgr.logicalIdMap_.clear();
    mgr.userZoneMap_.clear();
    mgr.initOk_ = false;
    RemoveTestConfig();
}

void DisplayUserZoneConfigManagerTest::TearDown(void)
{
    auto &mgr = DisplayUserZoneConfigManager::GetInstance();
    mgr.logicalIdMap_.clear();
    mgr.userZoneMap_.clear();
    mgr.initOk_ = false;
    RemoveTestConfig();
}

/**
 * @tc.name: DisplayUserZoneConfigManagerParseStandalone001
 * @tc.desc: Every display without userZoneId is its own user zone; the lookup
 *           maps are populated consistently.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(DisplayUserZoneConfigManagerTest, DisplayUserZoneConfigManagerParseStandalone001, TestSize.Level1)
{
    auto &mgr = DisplayUserZoneConfigManager::GetInstance();
    const std::string xml = WrapConfig(
        DisplayNode(PHYS_A, LOGICAL_A, "display_a") +
        DisplayNode(PHYS_B, LOGICAL_B, "display_b"));

    EXPECT_EQ(mgr.ParseDisplayConfig(xml), ERR_OK);
    ASSERT_EQ(mgr.logicalIdMap_.size(), 2u);
    ASSERT_EQ(mgr.userZoneMap_.size(), 2u);

    EXPECT_EQ(mgr.logicalIdMap_[LOGICAL_A].physicalId, PHYS_A);
    EXPECT_EQ(mgr.logicalIdMap_[LOGICAL_A].name, "display_a");
    EXPECT_EQ(mgr.logicalIdMap_[LOGICAL_A].userZone, LOGICAL_A);
    EXPECT_EQ(mgr.logicalIdMap_[LOGICAL_B].userZone, LOGICAL_B);

    ASSERT_EQ(mgr.userZoneMap_[LOGICAL_A].size(), 1u);
    EXPECT_EQ(mgr.userZoneMap_[LOGICAL_A][0], LOGICAL_A);
    ASSERT_EQ(mgr.userZoneMap_[LOGICAL_B].size(), 1u);
    EXPECT_EQ(mgr.userZoneMap_[LOGICAL_B][0], LOGICAL_B);
}

/**
 * @tc.name: DisplayUserZoneConfigManagerParsePrimarySecondary001
 * @tc.desc: A secondary display pointing to the head display's physicalId via
 *           userZoneId is resolved into the head's user zone.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(DisplayUserZoneConfigManagerTest, DisplayUserZoneConfigManagerParsePrimarySecondary001, TestSize.Level1)
{
    auto &mgr = DisplayUserZoneConfigManager::GetInstance();
    const std::string xml = WrapConfig(
        DisplayNode(PHYS_A, LOGICAL_A, "head_unit") +
        DisplayNode(PHYS_B, LOGICAL_B, "passenger", true, PHYS_A));

    EXPECT_EQ(mgr.ParseDisplayConfig(xml), ERR_OK);
    ASSERT_EQ(mgr.logicalIdMap_.size(), 2u);
    ASSERT_EQ(mgr.userZoneMap_.size(), 1u);

    EXPECT_EQ(mgr.logicalIdMap_[LOGICAL_A].userZone, LOGICAL_A);
    EXPECT_EQ(mgr.logicalIdMap_[LOGICAL_B].userZone, LOGICAL_A);
    EXPECT_EQ(mgr.logicalIdMap_[LOGICAL_B].name, "passenger");

    ASSERT_EQ(mgr.userZoneMap_[LOGICAL_A].size(), 2u);
    EXPECT_EQ(mgr.userZoneMap_[LOGICAL_A][0], LOGICAL_A);
    EXPECT_EQ(mgr.userZoneMap_[LOGICAL_A][1], LOGICAL_B);
}

/**
 * @tc.name: DisplayUserZoneConfigManagerParseMultipleZones001
 * @tc.desc: A head zone with one secondary plus an independent standalone zone
 *           produce two distinct user zones with the correct primaries.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(DisplayUserZoneConfigManagerTest, DisplayUserZoneConfigManagerParseMultipleZones001, TestSize.Level1)
{
    auto &mgr = DisplayUserZoneConfigManager::GetInstance();
    const std::string xml = WrapConfig(
        DisplayNode(PHYS_A, LOGICAL_A, "head_unit") +
        DisplayNode(PHYS_B, LOGICAL_B, "passenger", true, PHYS_A) +
        DisplayNode(PHYS_C, LOGICAL_C, "standalone"));

    EXPECT_EQ(mgr.ParseDisplayConfig(xml), ERR_OK);
    ASSERT_EQ(mgr.logicalIdMap_.size(), 3u);
    ASSERT_EQ(mgr.userZoneMap_.size(), 2u);

    EXPECT_EQ(mgr.logicalIdMap_[LOGICAL_B].userZone, LOGICAL_A);
    EXPECT_EQ(mgr.logicalIdMap_[LOGICAL_C].userZone, LOGICAL_C);

    ASSERT_EQ(mgr.userZoneMap_[LOGICAL_A].size(), 2u);
    EXPECT_EQ(mgr.userZoneMap_[LOGICAL_A][0], LOGICAL_A);
    EXPECT_EQ(mgr.userZoneMap_[LOGICAL_A][1], LOGICAL_B);
    ASSERT_EQ(mgr.userZoneMap_[LOGICAL_C].size(), 1u);
    EXPECT_EQ(mgr.userZoneMap_[LOGICAL_C][0], LOGICAL_C);
}

/**
 * @tc.name: DisplayUserZoneConfigManagerParseWhitespace001
 * @tc.desc: Numeric leaf values carrying pretty-printing whitespace are trimmed
 *           before parsing, so hand-written configs still load.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(DisplayUserZoneConfigManagerTest, DisplayUserZoneConfigManagerParseWhitespace001, TestSize.Level1)
{
    auto &mgr = DisplayUserZoneConfigManager::GetInstance();
    const std::string xml =
        "<Configs><displays>"
        "<display><physicalId>\n\t10 \t</physicalId>"
        "<logicalId> 100\r\n</logicalId><name>head_unit</name></display>"
        "<display><physicalId> 20 </physicalId>"
        "<logicalId>200</logicalId><name>passenger</name>"
        "<userZoneId>\r\n 10 \n</userZoneId></display>"
        "</displays></Configs>";

    EXPECT_EQ(mgr.ParseDisplayConfig(xml), ERR_OK);
    ASSERT_EQ(mgr.logicalIdMap_.size(), 2u);
    EXPECT_EQ(mgr.logicalIdMap_[LOGICAL_B].userZone, LOGICAL_A);
    ASSERT_EQ(mgr.userZoneMap_[LOGICAL_A].size(), 2u);
}

/**
 * @tc.name: DisplayUserZoneConfigManagerParseXmlDeclComment001
 * @tc.desc: An XML declaration, comments, and pretty printing do not disturb
 *           parsing (XML_PARSE_NOBLANKS drops the whitespace text nodes).
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(DisplayUserZoneConfigManagerTest, DisplayUserZoneConfigManagerParseXmlDeclComment001, TestSize.Level1)
{
    auto &mgr = DisplayUserZoneConfigManager::GetInstance();
    const std::string xml =
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
        "<Configs>\n"
        "    <!-- built-in display -->\n"
        "    <displays>\n"
        "        <display>\n"
        "            <physicalId>10</physicalId>\n"
        "            <logicalId>100</logicalId>\n"
        "            <name>head_unit</name>\n"
        "        </display>\n"
        "        <display>\n"
        "            <physicalId>20</physicalId>\n"
        "            <logicalId>200</logicalId>\n"
        "            <name>passenger</name>\n"
        "            <userZoneId>10</userZoneId>\n"
        "        </display>\n"
        "    </displays>\n"
        "</Configs>\n";

    EXPECT_EQ(mgr.ParseDisplayConfig(xml), ERR_OK);
    ASSERT_EQ(mgr.logicalIdMap_.size(), 2u);
    EXPECT_EQ(mgr.logicalIdMap_[LOGICAL_B].userZone, LOGICAL_A);
}

/**
 * @tc.name: DisplayUserZoneConfigManagerParseSelfReference001
 * @tc.desc: A userZoneId equal to the display's own physicalId resolves to its
 *           own logicalId, making it a valid primary (standalone behavior).
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(DisplayUserZoneConfigManagerTest, DisplayUserZoneConfigManagerParseSelfReference001, TestSize.Level1)
{
    auto &mgr = DisplayUserZoneConfigManager::GetInstance();
    const std::string xml = WrapConfig(DisplayNode(PHYS_A, LOGICAL_A, "self", true, PHYS_A));

    EXPECT_EQ(mgr.ParseDisplayConfig(xml), ERR_OK);
    ASSERT_EQ(mgr.logicalIdMap_.size(), 1u);
    EXPECT_EQ(mgr.logicalIdMap_[LOGICAL_A].userZone, LOGICAL_A);
}

/**
 * @tc.name: DisplayUserZoneConfigManagerParseMinimalFields001
 * @tc.desc: physicalId and logicalId are the only required fields; name and
 *           userZoneId are optional and default to empty / standalone.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(DisplayUserZoneConfigManagerTest, DisplayUserZoneConfigManagerParseMinimalFields001, TestSize.Level1)
{
    auto &mgr = DisplayUserZoneConfigManager::GetInstance();
    const std::string xml = WrapConfig(
        "<display><physicalId>10</physicalId><logicalId>100</logicalId></display>");

    EXPECT_EQ(mgr.ParseDisplayConfig(xml), ERR_OK);
    ASSERT_EQ(mgr.logicalIdMap_.size(), 1u);
    EXPECT_EQ(mgr.logicalIdMap_[LOGICAL_A].userZone, LOGICAL_A);
    EXPECT_TRUE(mgr.logicalIdMap_[LOGICAL_A].name.empty());
}

/**
 * @tc.name: DisplayUserZoneConfigManagerParseNonXml001
 * @tc.desc: Non-XML content is rejected as a format error.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(DisplayUserZoneConfigManagerTest, DisplayUserZoneConfigManagerParseNonXml001, TestSize.Level1)
{
    auto &mgr = DisplayUserZoneConfigManager::GetInstance();
    EXPECT_EQ(mgr.ParseDisplayConfig("this is not xml"), ERR_ACCOUNT_COMMON_BAD_JSON_FORMAT_ERROR);
    EXPECT_TRUE(mgr.logicalIdMap_.empty());
}

/**
 * @tc.name: DisplayUserZoneConfigManagerParseWrongRoot001
 * @tc.desc: A well-formed document whose root is not <Configs> is rejected.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(DisplayUserZoneConfigManagerTest, DisplayUserZoneConfigManagerParseWrongRoot001, TestSize.Level1)
{
    auto &mgr = DisplayUserZoneConfigManager::GetInstance();
    EXPECT_EQ(mgr.ParseDisplayConfig("<Displays></Displays>"), ERR_ACCOUNT_COMMON_BAD_JSON_FORMAT_ERROR);
    EXPECT_TRUE(mgr.logicalIdMap_.empty());
}

/**
 * @tc.name: DisplayUserZoneConfigManagerParseMissingDisplays001
 * @tc.desc: A <Configs> root without a <displays> child is rejected.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(DisplayUserZoneConfigManagerTest, DisplayUserZoneConfigManagerParseMissingDisplays001, TestSize.Level1)
{
    auto &mgr = DisplayUserZoneConfigManager::GetInstance();
    EXPECT_EQ(mgr.ParseDisplayConfig("<Configs></Configs>"), ERR_ACCOUNT_COMMON_BAD_JSON_FORMAT_ERROR);
    EXPECT_TRUE(mgr.logicalIdMap_.empty());
}

/**
 * @tc.name: DisplayUserZoneConfigManagerParseEmptyDisplays001
 * @tc.desc: A config with zero display nodes has no primary display and is rejected.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(DisplayUserZoneConfigManagerTest, DisplayUserZoneConfigManagerParseEmptyDisplays001, TestSize.Level1)
{
    auto &mgr = DisplayUserZoneConfigManager::GetInstance();
    EXPECT_EQ(mgr.ParseDisplayConfig(WrapConfig("")), ERR_ACCOUNT_COMMON_BAD_JSON_FORMAT_ERROR);
    EXPECT_TRUE(mgr.logicalIdMap_.empty());
}

/**
 * @tc.name: DisplayUserZoneConfigManagerParseMissingPhysicalId001
 * @tc.desc: A display node missing physicalId is rejected.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(DisplayUserZoneConfigManagerTest, DisplayUserZoneConfigManagerParseMissingPhysicalId001, TestSize.Level1)
{
    auto &mgr = DisplayUserZoneConfigManager::GetInstance();
    const std::string xml = WrapConfig(
        "<display><logicalId>100</logicalId><name>a</name></display>");
    EXPECT_EQ(mgr.ParseDisplayConfig(xml), ERR_ACCOUNT_COMMON_BAD_JSON_FORMAT_ERROR);
    EXPECT_TRUE(mgr.logicalIdMap_.empty());
}

/**
 * @tc.name: DisplayUserZoneConfigManagerParseMissingLogicalId001
 * @tc.desc: A display node missing logicalId is rejected.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(DisplayUserZoneConfigManagerTest, DisplayUserZoneConfigManagerParseMissingLogicalId001, TestSize.Level1)
{
    auto &mgr = DisplayUserZoneConfigManager::GetInstance();
    const std::string xml = WrapConfig(
        "<display><physicalId>10</physicalId><name>a</name></display>");
    EXPECT_EQ(mgr.ParseDisplayConfig(xml), ERR_ACCOUNT_COMMON_BAD_JSON_FORMAT_ERROR);
    EXPECT_TRUE(mgr.logicalIdMap_.empty());
}

/**
 * @tc.name: DisplayUserZoneConfigManagerParseBadNumber001
 * @tc.desc: A non-numeric required attribute is rejected.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(DisplayUserZoneConfigManagerTest, DisplayUserZoneConfigManagerParseBadNumber001, TestSize.Level1)
{
    auto &mgr = DisplayUserZoneConfigManager::GetInstance();
    const std::string xml = WrapConfig(
        "<display><physicalId>abc</physicalId><logicalId>100</logicalId></display>");
    EXPECT_EQ(mgr.ParseDisplayConfig(xml), ERR_ACCOUNT_COMMON_BAD_JSON_FORMAT_ERROR);
}

/**
 * @tc.name: DisplayUserZoneConfigManagerParseNegativeNumber001
 * @tc.desc: A negative value is not a valid unsigned id and is rejected.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(DisplayUserZoneConfigManagerTest, DisplayUserZoneConfigManagerParseNegativeNumber001, TestSize.Level1)
{
    auto &mgr = DisplayUserZoneConfigManager::GetInstance();
    const std::string xml = WrapConfig(
        "<display><physicalId>10</physicalId><logicalId>-1</logicalId></display>");
    EXPECT_EQ(mgr.ParseDisplayConfig(xml), ERR_ACCOUNT_COMMON_BAD_JSON_FORMAT_ERROR);
}

/**
 * @tc.name: DisplayUserZoneConfigManagerParseOverflow001
 * @tc.desc: A value exceeding uint64_t range is rejected by from_chars.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(DisplayUserZoneConfigManagerTest, DisplayUserZoneConfigManagerParseOverflow001, TestSize.Level1)
{
    auto &mgr = DisplayUserZoneConfigManager::GetInstance();
    const std::string xml = WrapConfig(
        "<display><physicalId>18446744073709551616</physicalId><logicalId>100</logicalId></display>");
    EXPECT_EQ(mgr.ParseDisplayConfig(xml), ERR_ACCOUNT_COMMON_BAD_JSON_FORMAT_ERROR);
}

/**
 * @tc.name: DisplayUserZoneConfigManagerParseTrailingGarbage001
 * @tc.desc: A value with trailing non-digit characters is rejected (full-consumption rule).
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(DisplayUserZoneConfigManagerTest, DisplayUserZoneConfigManagerParseTrailingGarbage001, TestSize.Level1)
{
    auto &mgr = DisplayUserZoneConfigManager::GetInstance();
    const std::string xml = WrapConfig(
        "<display><physicalId>10abc</physicalId><logicalId>100</logicalId></display>");
    EXPECT_EQ(mgr.ParseDisplayConfig(xml), ERR_ACCOUNT_COMMON_BAD_JSON_FORMAT_ERROR);
}

/**
 * @tc.name: DisplayUserZoneConfigManagerParseDuplicatePhysicalId001
 * @tc.desc: Two display nodes sharing a physicalId are rejected.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(DisplayUserZoneConfigManagerTest, DisplayUserZoneConfigManagerParseDuplicatePhysicalId001, TestSize.Level1)
{
    auto &mgr = DisplayUserZoneConfigManager::GetInstance();
    const std::string xml = WrapConfig(
        DisplayNode(PHYS_A, LOGICAL_A, "a") +
        DisplayNode(PHYS_A, LOGICAL_B, "b"));
    EXPECT_EQ(mgr.ParseDisplayConfig(xml), ERR_ACCOUNT_COMMON_BAD_JSON_FORMAT_ERROR);
    EXPECT_TRUE(mgr.logicalIdMap_.empty());
}

/**
 * @tc.name: DisplayUserZoneConfigManagerParseDuplicateLogicalId001
 * @tc.desc: Two display nodes sharing a logicalId are rejected and any partial
 *           writes are rolled back.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(DisplayUserZoneConfigManagerTest, DisplayUserZoneConfigManagerParseDuplicateLogicalId001, TestSize.Level1)
{
    auto &mgr = DisplayUserZoneConfigManager::GetInstance();
    const std::string xml = WrapConfig(
        DisplayNode(PHYS_A, LOGICAL_A, "a") +
        DisplayNode(PHYS_B, LOGICAL_A, "b"));
    EXPECT_EQ(mgr.ParseDisplayConfig(xml), ERR_ACCOUNT_COMMON_BAD_JSON_FORMAT_ERROR);
    EXPECT_TRUE(mgr.logicalIdMap_.empty());
}

/**
 * @tc.name: DisplayUserZoneConfigManagerParseDuplicateField001
 * @tc.desc: A single display node repeating a required id field is rejected.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(DisplayUserZoneConfigManagerTest, DisplayUserZoneConfigManagerParseDuplicateField001, TestSize.Level1)
{
    auto &mgr = DisplayUserZoneConfigManager::GetInstance();
    const std::string dupPhysicalId =
        "<Configs><displays><display><physicalId>10</physicalId><physicalId>11</physicalId>"
        "<logicalId>100</logicalId></display></displays></Configs>";
    EXPECT_EQ(mgr.ParseDisplayConfig(dupPhysicalId), ERR_ACCOUNT_COMMON_BAD_JSON_FORMAT_ERROR);

    const std::string dupLogicalId =
        "<Configs><displays><display><physicalId>10</physicalId><logicalId>100</logicalId>"
        "<logicalId>101</logicalId></display></displays></Configs>";
    EXPECT_EQ(mgr.ParseDisplayConfig(dupLogicalId), ERR_ACCOUNT_COMMON_BAD_JSON_FORMAT_ERROR);

    const std::string dupUserZoneId =
        "<Configs><displays><display><physicalId>10</physicalId><logicalId>100</logicalId>"
        "<userZoneId>10</userZoneId><userZoneId>20</userZoneId></display></displays></Configs>";
    EXPECT_EQ(mgr.ParseDisplayConfig(dupUserZoneId), ERR_ACCOUNT_COMMON_BAD_JSON_FORMAT_ERROR);
    EXPECT_TRUE(mgr.logicalIdMap_.empty());
}

/**
 * @tc.name: DisplayUserZoneConfigManagerParseUserZoneIdNotFound001
 * @tc.desc: A secondary display referencing a physicalId that no display owns is rejected.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(DisplayUserZoneConfigManagerTest, DisplayUserZoneConfigManagerParseUserZoneIdNotFound001, TestSize.Level1)
{
    auto &mgr = DisplayUserZoneConfigManager::GetInstance();
    const std::string xml = WrapConfig(DisplayNode(PHYS_A, LOGICAL_A, "a", true, 999));
    EXPECT_EQ(mgr.ParseDisplayConfig(xml), ERR_ACCOUNT_COMMON_BAD_JSON_FORMAT_ERROR);
    EXPECT_TRUE(mgr.logicalIdMap_.empty());
}

/**
 * @tc.name: DisplayUserZoneConfigManagerParseCycle001
 * @tc.desc: Two displays whose userZoneId point at each other form a cycle with no
 *           primary and are rejected.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(DisplayUserZoneConfigManagerTest, DisplayUserZoneConfigManagerParseCycle001, TestSize.Level1)
{
    auto &mgr = DisplayUserZoneConfigManager::GetInstance();
    const std::string xml = WrapConfig(
        DisplayNode(PHYS_A, LOGICAL_A, "a", true, PHYS_B) +
        DisplayNode(PHYS_B, LOGICAL_B, "b", true, PHYS_A));
    EXPECT_EQ(mgr.ParseDisplayConfig(xml), ERR_ACCOUNT_COMMON_BAD_JSON_FORMAT_ERROR);
    EXPECT_TRUE(mgr.logicalIdMap_.empty());
}

/**
 * @tc.name: DisplayUserZoneConfigManagerParseSecondaryRefsSecondary001
 * @tc.desc: A display whose userZoneId resolves to another secondary (not a primary)
 *           is rejected as a possible cycle.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(DisplayUserZoneConfigManagerTest, DisplayUserZoneConfigManagerParseSecondaryRefsSecondary001, TestSize.Level1)
{
    auto &mgr = DisplayUserZoneConfigManager::GetInstance();
    const std::string xml = WrapConfig(
        DisplayNode(PHYS_A, LOGICAL_A, "head") +
        DisplayNode(PHYS_B, LOGICAL_B, "mid", true, PHYS_A) +
        DisplayNode(PHYS_C, LOGICAL_C, "leaf", true, PHYS_B));
    EXPECT_EQ(mgr.ParseDisplayConfig(xml), ERR_ACCOUNT_COMMON_BAD_JSON_FORMAT_ERROR);
    EXPECT_TRUE(mgr.logicalIdMap_.empty());
}

/**
 * @tc.name: DisplayUserZoneConfigManagerParseUInt64Boundary001
 * @tc.desc: ParseUInt64 handles empty, whitespace, invalid, valid, and out-of-range
 *           numeric strings correctly.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(DisplayUserZoneConfigManagerTest, DisplayUserZoneConfigManagerParseUInt64Boundary001, TestSize.Level1)
{
    auto &mgr = DisplayUserZoneConfigManager::GetInstance();
    uint64_t value = 0;

    EXPECT_FALSE(mgr.ParseUInt64("", value));
    EXPECT_FALSE(mgr.ParseUInt64(" \t\r\n", value));
    EXPECT_FALSE(mgr.ParseUInt64("abc", value));
    EXPECT_FALSE(mgr.ParseUInt64("-1", value));
    EXPECT_FALSE(mgr.ParseUInt64("10abc", value));
    EXPECT_FALSE(mgr.ParseUInt64("10 20", value));
    EXPECT_FALSE(mgr.ParseUInt64("18446744073709551616", value));

    EXPECT_TRUE(mgr.ParseUInt64("0", value));
    EXPECT_EQ(value, 0u);
    EXPECT_TRUE(mgr.ParseUInt64("18446744073709551615", value));
    EXPECT_EQ(value, UINT64_MAX);
    EXPECT_TRUE(mgr.ParseUInt64(" 12345 \n", value));
    EXPECT_EQ(value, 12345u);
}

/**
 * @tc.name: DisplayUserZoneConfigManagerRetryReadFailure001
 * @tc.desc: A failed read is retried on every query; a later successful read is cached.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(DisplayUserZoneConfigManagerTest, DisplayUserZoneConfigManagerRetryReadFailure001, TestSize.Level1)
{
    auto &mgr = DisplayUserZoneConfigManager::GetInstance();
    bool isPrimary = false;
    EXPECT_EQ(mgr.IsDisplayPrimary(LOGICAL_A, isPrimary), ERR_ACCOUNT_COMMON_FILE_READ_FAILED);

    // A later query retries the read; once a real XML file appears the load succeeds.
    WriteTestConfig(WrapConfig(DisplayNode(PHYS_A, LOGICAL_A, "display_a")));
    EXPECT_EQ(mgr.IsDisplayPrimary(LOGICAL_A, isPrimary), ERR_OK);
    EXPECT_TRUE(isPrimary);

    // A successful load is cached: replacing the file must not change the result.
    WriteTestConfig(WrapConfig(
        DisplayNode(PHYS_A, LOGICAL_A, "display_a", true, PHYS_C) +
        DisplayNode(PHYS_C, LOGICAL_C, "display_c")));
    EXPECT_EQ(mgr.IsDisplayPrimary(LOGICAL_A, isPrimary), ERR_OK);
    EXPECT_TRUE(isPrimary);
}

/**
 * @tc.name: DisplayUserZoneConfigManagerRetryFormatError001
 * @tc.desc: A successful read with invalid XML is a format error and is retried on every query.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(DisplayUserZoneConfigManagerTest, DisplayUserZoneConfigManagerRetryFormatError001, TestSize.Level1)
{
    auto &mgr = DisplayUserZoneConfigManager::GetInstance();
    WriteTestConfig("<Invalid/>");

    bool isPrimary = false;
    EXPECT_EQ(mgr.IsDisplayPrimary(LOGICAL_A, isPrimary), ERR_ACCOUNT_COMMON_BAD_JSON_FORMAT_ERROR);
    EXPECT_EQ(mgr.IsDisplayPrimary(LOGICAL_A, isPrimary), ERR_ACCOUNT_COMMON_BAD_JSON_FORMAT_ERROR);

    // Fixing the real XML file recovers the next query.
    WriteTestConfig(WrapConfig(DisplayNode(PHYS_A, LOGICAL_A, "display_a")));
    EXPECT_EQ(mgr.IsDisplayPrimary(LOGICAL_A, isPrimary), ERR_OK);
    EXPECT_TRUE(isPrimary);
}

/**
 * @tc.name: DisplayUserZoneConfigManagerQueryPrimary001
 * @tc.desc: GetPrimaryDisplayId and IsDisplayPrimary resolve a loaded secondary display
 *           to its user zone primary, and unknown displays are reported as an error.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(DisplayUserZoneConfigManagerTest, DisplayUserZoneConfigManagerQueryPrimary001, TestSize.Level1)
{
    auto &mgr = DisplayUserZoneConfigManager::GetInstance();
    WriteTestConfig(WrapConfig(
        DisplayNode(PHYS_A, LOGICAL_A, "head_unit") +
        DisplayNode(PHYS_B, LOGICAL_B, "passenger", true, PHYS_A)));

    uint64_t primaryDisplayId = 0;
    EXPECT_EQ(mgr.GetPrimaryDisplayId(LOGICAL_B, primaryDisplayId), ERR_OK);
    EXPECT_EQ(primaryDisplayId, LOGICAL_A);
    EXPECT_EQ(mgr.GetPrimaryDisplayId(LOGICAL_A, primaryDisplayId), ERR_OK);
    EXPECT_EQ(primaryDisplayId, LOGICAL_A);
    constexpr uint64_t UNKNOWN_DISPLAY = 999;
    // An unknown display is reported as an error and the out-params are left untouched.
    EXPECT_EQ(mgr.GetPrimaryDisplayId(UNKNOWN_DISPLAY, primaryDisplayId),
        ERR_ACCOUNT_COMMON_DISPLAY_NOT_IN_CONFIG_ERROR);
    EXPECT_EQ(primaryDisplayId, LOGICAL_A);

    bool isPrimary = false;
    EXPECT_EQ(mgr.IsDisplayPrimary(LOGICAL_A, isPrimary), ERR_OK);
    EXPECT_TRUE(isPrimary);
    EXPECT_EQ(mgr.IsDisplayPrimary(LOGICAL_B, isPrimary), ERR_OK);
    EXPECT_FALSE(isPrimary);
    EXPECT_EQ(mgr.IsDisplayPrimary(UNKNOWN_DISPLAY, isPrimary),
        ERR_ACCOUNT_COMMON_DISPLAY_NOT_IN_CONFIG_ERROR);
    EXPECT_FALSE(isPrimary);
}

/**
 * @tc.name: DisplayUserZoneConfigManagerQueryZoneDisplays001
 * @tc.desc: GetDisplayIdsByLogicalId returns the whole user zone of a loaded secondary
 *           display and reports unknown displays as an error with an empty result.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(DisplayUserZoneConfigManagerTest, DisplayUserZoneConfigManagerQueryZoneDisplays001, TestSize.Level1)
{
    auto &mgr = DisplayUserZoneConfigManager::GetInstance();
    WriteTestConfig(WrapConfig(
        DisplayNode(PHYS_A, LOGICAL_A, "head_unit") +
        DisplayNode(PHYS_B, LOGICAL_B, "passenger", true, PHYS_A) +
        DisplayNode(PHYS_C, LOGICAL_C, "standalone")));

    std::vector<uint64_t> displayIds;
    EXPECT_EQ(mgr.GetDisplayIdsByLogicalId(LOGICAL_B, displayIds), ERR_OK);
    ASSERT_EQ(displayIds.size(), 2u);
    EXPECT_EQ(displayIds[0], LOGICAL_A);
    EXPECT_EQ(displayIds[1], LOGICAL_B);
    EXPECT_EQ(mgr.GetDisplayIdsByLogicalId(LOGICAL_C, displayIds), ERR_OK);
    ASSERT_EQ(displayIds.size(), 1u);
    EXPECT_EQ(displayIds[0], LOGICAL_C);
    constexpr uint64_t UNKNOWN_DISPLAY = 999;
    // An unknown display is reported as an error; the vector stays empty (cleared on entry).
    EXPECT_EQ(mgr.GetDisplayIdsByLogicalId(UNKNOWN_DISPLAY, displayIds),
        ERR_ACCOUNT_COMMON_DISPLAY_NOT_IN_CONFIG_ERROR);
    EXPECT_TRUE(displayIds.empty());
}

/**
 * @tc.name: DisplayUserZoneConfigManagerConcurrentLoad001
 * @tc.desc: Concurrent first load, Init and queries complete without deadlock and
 *           every thread observes the same user zone layout.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(DisplayUserZoneConfigManagerTest, DisplayUserZoneConfigManagerConcurrentLoad001, TestSize.Level1)
{
    auto &mgr = DisplayUserZoneConfigManager::GetInstance();
    WriteTestConfig(WrapConfig(
        DisplayNode(PHYS_A, LOGICAL_A, "head_unit") +
        DisplayNode(PHYS_B, LOGICAL_B, "passenger", true, PHYS_A)));

    constexpr int THREAD_COUNT = 8;
    constexpr int ITERATIONS = 50;
    std::atomic<int> failures = 0;
    std::atomic<bool> start = false;
    std::vector<std::thread> threads;
    threads.reserve(THREAD_COUNT);
    for (int i = 0; i < THREAD_COUNT; ++i) {
        threads.emplace_back([&mgr, &failures, &start, i]() {
            while (!start.load()) {
                std::this_thread::yield();
            }
            for (int j = 0; j < ITERATIONS; ++j) {
                if (i % 2 == 0) {
                    bool isPrimary = false;
                    if (mgr.IsDisplayPrimary(LOGICAL_A, isPrimary) != ERR_OK || !isPrimary) {
                        ++failures;
                    }
                    uint64_t primaryDisplayId = 0;
                    if (mgr.GetPrimaryDisplayId(LOGICAL_B, primaryDisplayId) != ERR_OK ||
                        primaryDisplayId != LOGICAL_A) {
                        ++failures;
                    }
                } else {
                    std::vector<uint64_t> displayIds;
                    if (mgr.Init() != ERR_OK) {
                        ++failures;
                    }
                    if (mgr.GetDisplayIdsByLogicalId(LOGICAL_B, displayIds) != ERR_OK ||
                        displayIds.size() != 2u) {
                        ++failures;
                    }
                }
            }
        });
    }
    start.store(true);
    // A deadlock here would block join() and fail the suite by timeout.
    for (auto &thread : threads) {
        thread.join();
    }
    EXPECT_EQ(failures.load(), 0);
}

/**
 * @tc.name: DisplayUserZoneConfigManagerInitFailure001
 * @tc.desc: Init returns the normalized read/format error and leaves the manager
 *           unready so a subsequent query retries the load.
 * @tc.type: FUNC
 * @tc.require:
 */
HWTEST_F(DisplayUserZoneConfigManagerTest, DisplayUserZoneConfigManagerInitFailure001, TestSize.Level1)
{
    auto &mgr = DisplayUserZoneConfigManager::GetInstance();

    // No config file: Init reports a read failure and leaves the manager unready.
    EXPECT_EQ(mgr.Init(), ERR_ACCOUNT_COMMON_FILE_READ_FAILED);
    EXPECT_FALSE(mgr.initOk_);

    WriteTestConfig("not xml");
    EXPECT_EQ(mgr.Init(), ERR_ACCOUNT_COMMON_BAD_JSON_FORMAT_ERROR);
    EXPECT_FALSE(mgr.initOk_);
}
}  // namespace AccountSA
}  // namespace OHOS
