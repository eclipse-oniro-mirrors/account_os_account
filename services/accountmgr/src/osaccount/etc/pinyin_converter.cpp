/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "pinyin_converter.h"
#include <memory>
#include "account_log_wrapper.h"
#include "unicode/putil.h"
#include "unicode/translit.h"

namespace OHOS {
namespace AccountSA {
namespace {
constexpr const char *TRANSLIT_RULE_ID = "Han-Latin; Latin-ASCII";
constexpr const char *ICU_DATA_DIR = "/system/usr/icu";

icu::Transliterator *CreateTransliterator()
{
    u_setDataDirectory(ICU_DATA_DIR);
    UErrorCode status = U_ZERO_ERROR;
    icu::UnicodeString ruleId = icu::UnicodeString::fromUTF8(TRANSLIT_RULE_ID);
    icu::Transliterator *trans = icu::Transliterator::createInstance(
        ruleId, UTransDirection::UTRANS_FORWARD, status);
    if (U_FAILURE(status) || trans == nullptr) {
        ACCOUNT_LOGE("Create transliterator failed, status = %{public}d", static_cast<int>(status));
        return nullptr;
    }
    return trans;
}

icu::Transliterator *GetTransliterator()
{
    static const std::unique_ptr<icu::Transliterator> instance(CreateTransliterator());
    return instance.get();
}
} // namespace

bool PinyinConverter::ConvertToPinyin(const std::string &input, std::string &output)
{
    icu::Transliterator *trans = GetTransliterator();
    if (trans == nullptr) {
        return false;
    }
    icu::UnicodeString unicodeStr = icu::UnicodeString::fromUTF8(input);
    trans->transliterate(unicodeStr);
    output.clear();
    unicodeStr.toUTF8String(output);
    return true;
}
} // namespace AccountSA
} // namespace OHOS
