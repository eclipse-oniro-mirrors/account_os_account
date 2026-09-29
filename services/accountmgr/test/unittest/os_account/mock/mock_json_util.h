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

#ifndef OS_ACCOUNT_MOCK_JSON_UTILS_H
#define OS_ACCOUNT_MOCK_JSON_UTILS_H

#include <map>
#include <memory>
#include <vector>
#include <string>
#include <set>
#include <functional>
#include "cJSON.h"

namespace OHOS {
namespace AccountSA {
typedef cJSON CJson;
typedef std::unique_ptr<CJson, std::function<void(CJson *ptr)>> CJsonUnique;
void SetTimes(int32_t times, int32_t falseTime, std::string methodName);
void SetCreateJsonFail(bool fail);
CJsonUnique CreateJson(void);
bool AddVectorStringToJson(CJsonUnique &jsonObj, const std::string &key, const std::vector<std::string> &vec);
bool AddVectorStringToJson(CJson *jsonObj, const std::string &key, const std::vector<std::string> &vec);
bool AddObjToJson(CJsonUnique &jsonObj, const std::string &key, CJsonUnique &childObj);
bool AddObjToJson(CJson *jsonObj, const std::string &key, const CJson *childObj);
bool AddIntToJson(CJsonUnique &jsonObj, const std::string &key, const int value);
bool AddIntToJson(CJson *jsonObj, const std::string &key, const int value);
bool AddStringToJson(CJsonUnique &jsonObj, const std::string &key, const std::string &value);
bool AddStringToJson(CJson *jsonObj, const std::string &key, const std::string &value);
bool AddBoolToJson(CJsonUnique &jsonObj, const std::string &key, const bool value);
bool AddBoolToJson(CJson *jsonObj, const std::string &key, const bool value);
bool AddInt64ToJson(CJsonUnique &jsonObj, const std::string &key, int64_t value);
bool AddInt64ToJson(CJson *jsonObj, const std::string &key, int64_t value);
bool AddUint64ToJson(CJsonUnique &jsonObj, const std::string &key, uint64_t value);
bool AddUint64ToJson(CJson *jsonObj, const std::string &key, uint64_t value);
bool AddDoubleToJson(CJsonUnique &jsonObj, const std::string &key, double value);
bool AddDoubleToJson(CJson *jsonObj, const std::string &key, double value);
bool AddSetStringToJson(CJsonUnique &jsonObj, const std::string &key, const std::set<std::string> &setData);
bool AddSetStringToJson(CJson *jsonObj, const std::string &key, const std::set<std::string> &setData);
bool AddVectorIntToJson(CJsonUnique &jsonObj, const std::string &key, const std::vector<int32_t> &vec);
bool AddVectorIntToJson(CJson *jsonObj, const std::string &key, const std::vector<int32_t> &vec);
bool AddVectorUint8ToJson(CJsonUnique &jsonObj, const std::string &key, std::vector<uint8_t> arr);
bool AddVectorUint8ToJson(CJson *jsonObj, const std::string &key, std::vector<uint8_t> arr);
bool AddObjToArray(CJsonUnique &jsonArr, CJsonUnique &item);
bool AddObjToArray(CJson *jsonArr, CJson *item);
} // namespace AccountSA
} // namespace OHOS
#endif // OS_ACCOUNT_MOCK_JSON_UTILS_H