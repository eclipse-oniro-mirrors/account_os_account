/*
 * Copyright (C) 2025 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "mock_json_util.h"

namespace OHOS {
namespace AccountSA {
static int32_t g_times = 0;
static int32_t g_falseTime = 0;
static std::string g_methodName = "";
static bool g_createJsonFail = false;
void SetTimes(int32_t times, int32_t falseTime, std::string methodName)
{
    g_times = times;
    g_falseTime = falseTime;
    g_methodName = methodName;
}

void SetCreateJsonFail(bool fail)
{
    g_createJsonFail = fail;
}

CJsonUnique CreateJson(void)
{
    if (g_createJsonFail) {
        return nullptr;
    }
    return CJsonUnique(cJSON_CreateObject(), cJSON_Delete);
}
bool MockMothd()
{
    g_times++;
    if (g_times == g_falseTime) {
        return false;
    }
    return true;
}

bool AddVectorStringToJson(CJsonUnique &jsonObj, const std::string &key, const std::vector<std::string> &vec)
{
    if (g_methodName != "AddVectorStringToJson") {
        return true;
    }
    return MockMothd();
}

bool AddVectorStringToJson(CJson *jsonObj, const std::string &key, const std::vector<std::string> &vec)
{
    if (g_methodName != "AddVectorStringToJson") {
        return true;
    }
    return MockMothd();
}

bool AddObjToJson(CJsonUnique &jsonObj, const std::string &key, CJsonUnique &childObj)
{
    if (g_methodName != "AddObjToJson") {
        return true;
    }
    return MockMothd();
}

bool AddObjToJson(CJson *jsonObj, const std::string &key, const CJson *childObj)
{
    if (g_methodName != "AddObjToJson") {
        return true;
    }
    return MockMothd();
}

bool AddIntToJson(CJsonUnique &jsonObj, const std::string &key, const int value)
{
    if (g_methodName != "AddIntToJson") {
        return true;
    }
    return MockMothd();
}

bool AddIntToJson(CJson *jsonObj, const std::string &key, const int value)
{
    if (g_methodName != "AddIntToJson") {
        return true;
    }
    return MockMothd();
}

bool AddStringToJson(CJsonUnique &jsonObj, const std::string &key, const std::string &value)
{
    if (g_methodName != "AddStringToJson") {
        return true;
    }
    return MockMothd();
}

bool AddStringToJson(CJson *jsonObj, const std::string &key, const std::string &value)
{
    if (g_methodName != "AddStringToJson") {
        return true;
    }
    return MockMothd();
}

bool AddBoolToJson(CJsonUnique &jsonObj, const std::string &key, const bool value)
{
    if (g_methodName != "AddBoolToJson") {
        return true;
    }
    return MockMothd();
}

bool AddBoolToJson(CJson *jsonObj, const std::string &key, const bool value)
{
    if (g_methodName != "AddBoolToJson") {
        return true;
    }
    return MockMothd();
}

bool AddInt64ToJson(CJsonUnique &jsonObj, const std::string &key, int64_t value)
{
    if (g_methodName != "AddInt64ToJson") {
        return true;
    }
    return MockMothd();
}

bool AddInt64ToJson(CJson *jsonObj, const std::string &key, int64_t value)
{
    if (g_methodName != "AddInt64ToJson") {
        return true;
    }
    return MockMothd();
}

bool AddUint64ToJson(CJsonUnique &jsonObj, const std::string &key, uint64_t value)
{
    if (g_methodName != "AddUint64ToJson") {
        return true;
    }
    return MockMothd();
}

bool AddUint64ToJson(CJson *jsonObj, const std::string &key, uint64_t value)
{
    if (g_methodName != "AddUint64ToJson") {
        return true;
    }
    return MockMothd();
}

bool AddDoubleToJson(CJsonUnique &jsonObj, const std::string &key, double value)
{
    if (g_methodName != "AddDoubleToJson") {
        return true;
    }
    return MockMothd();
}

bool AddDoubleToJson(CJson *jsonObj, const std::string &key, double value)
{
    if (g_methodName != "AddDoubleToJson") {
        return true;
    }
    return MockMothd();
}

bool AddSetStringToJson(CJsonUnique &jsonObj, const std::string &key, const std::set<std::string> &setData)
{
    if (g_methodName != "AddSetStringToJson") {
        return true;
    }
    return MockMothd();
}

bool AddSetStringToJson(CJson *jsonObj, const std::string &key, const std::set<std::string> &setData)
{
    if (g_methodName != "AddSetStringToJson") {
        return true;
    }
    return MockMothd();
}

bool AddVectorIntToJson(CJsonUnique &jsonObj, const std::string &key, const std::vector<int32_t> &vec)
{
    if (g_methodName != "AddVectorIntToJson") {
        return true;
    }
    return MockMothd();
}

bool AddVectorIntToJson(CJson *jsonObj, const std::string &key, const std::vector<int32_t> &vec)
{
    if (g_methodName != "AddVectorIntToJson") {
        return true;
    }
    return MockMothd();
}

bool AddVectorUint8ToJson(CJsonUnique &jsonObj, const std::string &key, std::vector<uint8_t> arr)
{
    if (g_methodName != "AddVectorUint8ToJson") {
        return true;
    }
    return MockMothd();
}

bool AddVectorUint8ToJson(CJson *jsonObj, const std::string &key, std::vector<uint8_t> arr)
{
    if (g_methodName != "AddVectorUint8ToJson") {
        return true;
    }
    return MockMothd();
}

bool AddObjToArray(CJsonUnique &jsonArr, CJsonUnique &item)
{
    if (g_methodName != "AddObjToArray") {
        return true;
    }
    return MockMothd();
}

bool AddObjToArray(CJson *jsonArr, CJson *item)
{
    if (g_methodName != "AddObjToArray") {
        return true;
    }
    return MockMothd();
}
} // namespace AccountSA
} // namespace OHOS