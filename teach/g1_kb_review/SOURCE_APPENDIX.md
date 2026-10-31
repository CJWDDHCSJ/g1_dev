# 附录 A：G1 当前部署源码全文

> 本附录逐字复制自 G1 当前部署目录的本地只读快照，未做格式化、未省略、未改写。

> 快照根目录：`/home/mick/Mick_Grey/teach/g1_kb_review/remote_snapshot/g1_voice_router/`

> 远端根目录：`/home/unitree/unitree_ros2/example/g1_voice_router/`

> 快照采集时间：2026-10-31；源码哈希与远端 `sha256sum` 结果一致（见 `G1_KB_REVIEW_BUNDLE.md` 第 4 节）。


## A.0 文件清单与状态

| 序号 | 文件 | 远端状态 | SHA256 | 行数 |
|---|---|---|---|---|
| 1 | `include/g1_voice_router/knowledge_base.hpp` | 新增 | `1dc19db4db18d5f3503488fe7486d5e794b5c645fc6259ee878bebb53d57a2af` | 48 |
| 2 | `src/knowledge_base.cpp` | 新增 | `295df377a43e0eb9675bb6f6cc8cdad6f26a3fed4b74ecfbeb4e5bb62f043477` | 772 |
| 3 | `test/knowledge_base_test.cpp` | 新增 | `dab2aef7156ff2cf3762f98a9d1681f38200f140ebb865482d4749c3c9c742fa` | 362 |
| 4 | `include/g1_voice_router/intent_recognizer.hpp` | 未修改 | `a208e31fdf4970177438b98fb2def583b6ae459f22d9cec22387fc706ae709c5` | 47 |
| 5 | `src/intent_recognizer.cpp` | 修改 | `80d50eaaa8a75733f55bda629b5be19cf8fd9b0a921d8974e2441fda80640569` | 310 |
| 6 | `test/intent_recognizer_test.cpp` | 修改 | `cd80b15f4542412051133e30a9a7faefc55eca7dbb20fe130778efd9c3abb3d7` | 86 |
| 7 | `src/voice_router.cpp` | 修改 | `fd6d29c02757eb82dba517d53d2f3f72c82d324cc56741ae917e8e42988e0148` | 688 |
| 8 | `CMakeLists.txt` | 修改 | `a7166376f6335ae346250d5b4af23e9fe9a6d624bf117420906ec46cee2feb8c` | 94 |
| 9 | `package.xml` | 修改 | `22d0f4a6341c455bc83122d7983c15490a0bd96730136dab247ae5d2a42dcb39` | 19 |

状态判定依据：`备份目录 /home/unitree/g1_backups/g1_voice_router_20261031_132307_before_kb/` 中是否存在同名文件，以及同名文件 SHA256 是否一致。


## A.1 `include/g1_voice_router/knowledge_base.hpp`

- **远端实际路径**：`/home/unitree/unitree_ros2/example/g1_voice_router/include/g1_voice_router/knowledge_base.hpp`

- **本地快照路径**：`remote_snapshot/g1_voice_router/include/g1_voice_router/knowledge_base.hpp`

- **变更状态**：新增

- **SHA256**：`1dc19db4db18d5f3503488fe7486d5e794b5c645fc6259ee878bebb53d57a2af`

- **行数**：48


```cpp
#ifndef G1_VOICE_ROUTER_KNOWLEDGE_BASE_HPP_
#define G1_VOICE_ROUTER_KNOWLEDGE_BASE_HPP_

#include <cstddef>
#include <string>

#include <nlohmann/json.hpp>

namespace g1_voice_router
{

struct KnowledgeResult
{
    bool found{false};
    std::string category;
    std::string matched_id;
    std::string answer;
};

class KnowledgeBase
{
public:
    // 加载并校验知识库；成功后常驻内存。
    bool load(
        const std::string &file_path,
        std::string &error);

    // 依次匹配 FAQ 精确问题、输入中包含的 FAQ 问题、
    // FAQ 关键词、条目关键词、类别关键词；全部失败时返回兜底。
    KnowledgeResult query(const std::string &user_text) const;

    bool loaded() const noexcept;

    std::size_t faq_count() const noexcept;
    std::size_t direction_count() const noexcept;
    std::size_t project_count() const noexcept;
    std::size_t equipment_count() const noexcept;

private:
    KnowledgeResult fallback_result() const;

    nlohmann::json data_;
    bool loaded_{false};
};

}  // namespace g1_voice_router

#endif  // G1_VOICE_ROUTER_KNOWLEDGE_BASE_HPP_
```


## A.2 `src/knowledge_base.cpp`

- **远端实际路径**：`/home/unitree/unitree_ros2/example/g1_voice_router/src/knowledge_base.cpp`

- **本地快照路径**：`remote_snapshot/g1_voice_router/src/knowledge_base.cpp`

- **变更状态**：新增

- **SHA256**：`295df377a43e0eb9675bb6f6cc8cdad6f26a3fed4b74ecfbeb4e5bb62f043477`

- **行数**：772


```cpp
#include "g1_voice_router/knowledge_base.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <fstream>
#include <iterator>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace g1_voice_router
{
namespace
{

using Json = nlohmann::json;

constexpr int kSupportedSchemaVersion = 1;

const char * kDefaultFallback =
    "这个信息目前没有公开资料，我不能确认。";

// 去掉中英文标点、空白，并只对 ASCII 字母做小写转换。
std::string normalize_text(const std::string & text)
{
    std::string result = text;

    static const std::vector<std::string> punctuation = {
        "。", "！", "？", "，", "、", "；", "：",
        "“", "”", "‘", "’",
        "（", "）", "【", "】", "《", "》", "〈", "〉",
        "…", "—", "–", "－", "·", "　",
        ".", ",", "!", "?", ";", ":", "\"", "'",
        "(", ")", "[", "]", "{", "}", "<", ">",
        "~", "`", " ", "\t", "\r", "\n"
    };

    for (const auto & mark : punctuation) {
        std::size_t pos = 0;
        while ((pos = result.find(mark, pos)) != std::string::npos) {
            result.erase(pos, mark.size());
        }
    }

    std::transform(
        result.begin(), result.end(), result.begin(),
        [](unsigned char c) {
            if (c < 0x80) {
                return static_cast<char>(std::tolower(c));
            }
            return static_cast<char>(c);
        });

    return result;
}

std::size_t utf8_length(const std::string & text) noexcept
{
    std::size_t length = 0;

    for (const unsigned char c : text) {
        if ((c & 0xC0U) != 0x80U) {
            ++length;
        }
    }

    return length;
}

// 至少命中两个关键词，或命中一个不少于四个汉字的关键词。
int match_score(
    const std::string & text,
    const Json & keywords)
{
    int hits = 0;
    int score = 0;
    int longest = 0;

    for (const auto & keyword : keywords) {
        if (!keyword.is_string()) {
            continue;
        }

        const std::string normalized =
            normalize_text(keyword.get<std::string>());

        if (normalized.empty()) {
            continue;
        }

        if (text.find(normalized) != std::string::npos) {
            const int length =
                static_cast<int>(utf8_length(normalized));

            ++hits;
            score += length;
            longest = std::max(longest, length);
        }
    }

    if (hits >= 2 || longest >= 4) {
        return score + hits;
    }

    return 0;
}

bool contains_url(const std::string & text)
{
    return text.find("http://") != std::string::npos ||
           text.find("https://") != std::string::npos ||
           text.find("www.") != std::string::npos;
}

bool read_string(
    const Json & object,
    const char * key,
    std::string & value,
    std::string & error,
    const std::string & path,
    bool allow_empty)
{
    if (!object.contains(key) || !object[key].is_string()) {
        error = path + "." + key + " must be a string";
        return false;
    }

    value = object[key].get<std::string>();

    if (!allow_empty && value.empty()) {
        error = path + "." + key + " must not be empty";
        return false;
    }

    return true;
}

bool read_string_array(
    const Json & object,
    const char * key,
    std::vector<std::string> & values,
    std::string & error,
    const std::string & path,
    bool allow_empty)
{
    if (!object.contains(key) || !object[key].is_array()) {
        error = path + "." + key + " must be an array";
        return false;
    }

    values.clear();

    for (const auto & item : object[key]) {
        if (!item.is_string()) {
            error = path + "." + key + " must contain only strings";
            return false;
        }

        values.push_back(item.get<std::string>());
    }

    if (!allow_empty && values.empty()) {
        error = path + "." + key + " must not be empty";
        return false;
    }

    return true;
}

bool validate_entry(
    const Json & item,
    const std::string & path,
    bool require_questions,
    const std::set<std::string> & source_ids,
    std::string & error)
{
    if (!item.is_object()) {
        error = path + " must be an object";
        return false;
    }

    std::string id;
    std::string answer;

    if (!read_string(item, "id", id, error, path, false) ||
        !read_string(item, "answer", answer, error, path, false))
    {
        return false;
    }

    if (contains_url(answer)) {
        error = path + ".answer must not contain a URL";
        return false;
    }

    if (item.contains("category") && !item["category"].is_string()) {
        error = path + ".category must be a string";
        return false;
    }

    std::vector<std::string> keywords;

    if (!read_string_array(
            item, "keywords", keywords, error, path, false))
    {
        return false;
    }

    for (const auto & keyword : keywords) {
        if (normalize_text(keyword).empty()) {
            error = path + ".keywords contains an empty keyword";
            return false;
        }
    }

    if (require_questions) {
        std::vector<std::string> questions;

        if (!read_string_array(
                item, "questions", questions, error, path, false))
        {
            return false;
        }

        for (const auto & question : questions) {
            if (normalize_text(question).empty()) {
                error = path + ".questions contains an empty question";
                return false;
            }
        }
    }

    std::vector<std::string> entry_source_ids;

    if (!read_string_array(
            item, "source_ids", entry_source_ids, error, path, false))
    {
        return false;
    }

    for (const auto & source_id : entry_source_ids) {
        if (source_ids.find(source_id) == source_ids.end()) {
            error = path + ".source_ids references unknown source: " +
                    source_id;
            return false;
        }
    }

    return true;
}

KnowledgeResult make_result(
    const Json & item,
    const std::string & category)
{
    KnowledgeResult result;
    result.found = true;
    result.category = category;
    result.matched_id = item.value("id", std::string{});
    result.answer = item.value("answer", std::string{});
    return result;
}

}  // namespace

bool KnowledgeBase::load(
    const std::string & file_path,
    std::string & error)
{
    data_.clear();
    loaded_ = false;

    auto fail = [&](const std::string & message) {
        error = message;
        data_.clear();
        loaded_ = false;
        return false;
    };

    if (file_path.empty()) {
        return fail("knowledge file path is empty");
    }

    std::ifstream file(file_path, std::ios::binary);

    if (!file) {
        return fail("cannot open knowledge file: " + file_path);
    }

    std::string content(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>());

    if (file.bad()) {
        return fail("failed to read knowledge file: " + file_path);
    }

    // 容忍 UTF-8 BOM。
    if (content.size() >= 3 &&
        static_cast<unsigned char>(content[0]) == 0xEF &&
        static_cast<unsigned char>(content[1]) == 0xBB &&
        static_cast<unsigned char>(content[2]) == 0xBF)
    {
        content.erase(0, 3);
    }

    Json parsed;

    try {
        parsed = Json::parse(content);
    } catch (const std::exception & exception) {
        return fail(
            "JSON parse error in " + file_path + ": " +
            exception.what());
    }

    if (!parsed.is_object()) {
        return fail("knowledge root must be a JSON object");
    }

    if (!parsed.contains("schema_version") ||
        !parsed["schema_version"].is_number_integer())
    {
        return fail("schema_version must be an integer");
    }

    const int schema_version =
        parsed["schema_version"].get<int>();

    if (schema_version != kSupportedSchemaVersion) {
        return fail(
            "unsupported schema_version: " +
            std::to_string(schema_version) +
            " (expected " +
            std::to_string(kSupportedSchemaVersion) + ")");
    }

    std::string school_name;
    std::string college_name;
    std::string lab_name;
    std::string location;
    std::string introduction;
    std::string fallback_answer;

    if (!read_string(
            parsed, "school_name", school_name, error,
            "$", true) ||
        !read_string(
            parsed, "college_name", college_name, error,
            "$", true) ||
        !read_string(
            parsed, "lab_name", lab_name, error,
            "$", true) ||
        !read_string(
            parsed, "location", location, error,
            "$", true) ||
        !read_string(
            parsed, "introduction", introduction, error,
            "$", true) ||
        !read_string(
            parsed, "fallback_answer", fallback_answer, error,
            "$", false))
    {
        return fail(error);
    }

    if (contains_url(fallback_answer) ||
        (!introduction.empty() && contains_url(introduction)) ||
        (!location.empty() && contains_url(location)))
    {
        return fail("top-level answers must not contain a URL");
    }

    if (parsed.contains("disclaimer")) {
        std::string disclaimer;

        if (!read_string(
                parsed, "disclaimer", disclaimer, error,
                "$", true))
        {
            return fail(error);
        }

        if (contains_url(disclaimer)) {
            return fail("disclaimer must not contain a URL");
        }
    }

    const std::array<const char *, 6> required_arrays = {
        "research_directions",
        "projects",
        "equipment",
        "categories",
        "faq",
        "sources"
    };

    for (const char * key : required_arrays) {
        if (!parsed.contains(key) || !parsed[key].is_array()) {
            return fail(std::string("$.") + key + " must be an array");
        }
    }

    if (parsed["sources"].empty()) {
        return fail("$.sources must not be empty");
    }

    std::set<std::string> source_ids;

    for (std::size_t index = 0; index < parsed["sources"].size(); ++index) {
        const Json & source = parsed["sources"][index];
        const std::string path =
            "$.sources[" + std::to_string(index) + "]";

        if (!source.is_object()) {
            return fail(path + " must be an object");
        }

        std::string id;
        std::string title;
        std::string publisher;
        std::string url;
        std::string level;
        std::string access_date;

        if (!read_string(source, "id", id, error, path, false) ||
            !read_string(source, "title", title, error, path, false) ||
            !read_string(
                source, "publisher", publisher, error, path, false) ||
            !read_string(source, "url", url, error, path, false) ||
            !read_string(source, "level", level, error, path, false) ||
            !read_string(
                source, "access_date", access_date, error, path, false))
        {
            return fail(error);
        }

        if (source.contains("date") && !source["date"].is_string()) {
            return fail(path + ".date must be a string");
        }

        if (source.contains("verification") &&
            !source["verification"].is_string())
        {
            return fail(path + ".verification must be a string");
        }

        if (!source_ids.insert(id).second) {
            return fail(path + ".id is duplicated: " + id);
        }
    }

    if (parsed.contains("labs")) {
        if (!parsed["labs"].is_array()) {
            return fail("$.labs must be an array");
        }

        for (std::size_t index = 0; index < parsed["labs"].size(); ++index) {
            const Json & lab = parsed["labs"][index];
            const std::string path =
                "$.labs[" + std::to_string(index) + "]";

            if (!lab.is_object()) {
                return fail(path + " must be an object");
            }

            std::string id;
            std::string name;
            std::vector<std::string> lab_source_ids;

            if (!read_string(lab, "id", id, error, path, false) ||
                !read_string(lab, "name", name, error, path, false) ||
                !read_string_array(
                    lab, "source_ids", lab_source_ids,
                    error, path, false))
            {
                return fail(error);
            }

            for (const auto & source_id : lab_source_ids) {
                if (source_ids.find(source_id) == source_ids.end()) {
                    return fail(
                        path + ".source_ids references unknown source: " +
                        source_id);
                }
            }
        }
    }

    const std::array<std::pair<const char *, bool>, 3> entry_arrays = {{
        {"research_directions", false},
        {"projects", false},
        {"equipment", false}
    }};

    for (const auto & spec : entry_arrays) {
        const Json & entries = parsed[spec.first];

        for (std::size_t index = 0; index < entries.size(); ++index) {
            const std::string path =
                std::string("$.") + spec.first +
                "[" + std::to_string(index) + "]";

            if (!validate_entry(
                    entries[index], path, spec.second,
                    source_ids, error))
            {
                return fail(error);
            }
        }
    }

    for (std::size_t index = 0; index < parsed["categories"].size(); ++index) {
        const std::string path =
            "$.categories[" + std::to_string(index) + "]";

        if (!validate_entry(
                parsed["categories"][index], path, false,
                source_ids, error))
        {
            return fail(error);
        }
    }

    for (std::size_t index = 0; index < parsed["faq"].size(); ++index) {
        const std::string path =
            "$.faq[" + std::to_string(index) + "]";

        if (!validate_entry(
                parsed["faq"][index], path, true,
                source_ids, error))
        {
            return fail(error);
        }
    }

    std::set<std::string> entry_ids;

    const std::array<const char *, 5> id_arrays = {
        "research_directions",
        "projects",
        "equipment",
        "categories",
        "faq"
    };

    for (const char * key : id_arrays) {
        for (const auto & entry : parsed[key]) {
            const std::string id = entry.value("id", std::string{});

            if (!entry_ids.insert(id).second) {
                return fail(
                    std::string("duplicated entry id: ") + id);
            }
        }
    }

    data_ = std::move(parsed);
    loaded_ = true;

    return true;
}

KnowledgeResult KnowledgeBase::query(
    const std::string & user_text) const
{
    if (!loaded_) {
        return fallback_result();
    }

    const std::string text = normalize_text(user_text);

    if (text.empty()) {
        return fallback_result();
    }

    // 1. FAQ 问题精确匹配。
    for (const auto & faq : data_["faq"]) {
        for (const auto & question : faq["questions"]) {
            if (text == normalize_text(question.get<std::string>())) {
                const KnowledgeResult result = make_result(
                    faq, faq.value("category", std::string("faq")));

                if (!result.answer.empty()) {
                    return result;
                }
            }
        }
    }

    // 1b. FAQ 问题被包含在输入中。
    // 容忍“你好，介绍一下你们实验室”这类带前缀的说法。
    std::size_t best_question_length = 0;
    const Json * best_question_faq = nullptr;

    for (const auto & faq : data_["faq"]) {
        for (const auto & question : faq["questions"]) {
            const std::string normalized_question =
                normalize_text(question.get<std::string>());
            const std::size_t question_length =
                utf8_length(normalized_question);

            if (question_length >= 5 &&
                question_length > best_question_length &&
                text.find(normalized_question) != std::string::npos)
            {
                best_question_length = question_length;
                best_question_faq = &faq;
            }
        }
    }

    if (best_question_faq != nullptr) {
        const KnowledgeResult result = make_result(
            *best_question_faq,
            best_question_faq->value(
                "category", std::string("faq")));

        if (!result.answer.empty()) {
            return result;
        }
    }


    // 2. FAQ 关键词匹配。
    int best_score = 0;
    const Json * best_item = nullptr;

    for (const auto & faq : data_["faq"]) {
        const int score = match_score(text, faq["keywords"]);

        if (score > best_score) {
            best_score = score;
            best_item = &faq;
        }
    }

    if (best_item != nullptr) {
        const KnowledgeResult result = make_result(
            *best_item,
            best_item->value("category", std::string("faq")));

        if (!result.answer.empty()) {
            return result;
        }
    }

    // 3. 研究方向、项目、设备条目匹配。
    const std::array<std::pair<const char *, const char *>, 3> entry_arrays = {{
        {"research_directions", "research_directions"},
        {"projects", "projects"},
        {"equipment", "equipment"}
    }};

    best_score = 0;
    best_item = nullptr;
    std::string best_category;

    for (const auto & spec : entry_arrays) {
        for (const auto & entry : data_[spec.first]) {
            const int score = match_score(text, entry["keywords"]);

            if (score > best_score) {
                best_score = score;
                best_item = &entry;
                best_category = spec.second;
            }
        }
    }

    if (best_item != nullptr) {
        const KnowledgeResult result =
            make_result(*best_item, best_category);

        if (!result.answer.empty()) {
            return result;
        }
    }

    // 4. 类别关键词匹配。
    best_score = 0;
    best_item = nullptr;

    for (const auto & category : data_["categories"]) {
        const int score = match_score(text, category["keywords"]);

        if (score > best_score) {
            best_score = score;
            best_item = &category;
        }
    }

    if (best_item != nullptr) {
        const KnowledgeResult result = make_result(
            *best_item,
            best_item->value("id", std::string("category")));

        if (!result.answer.empty()) {
            return result;
        }
    }

    return fallback_result();
}

bool KnowledgeBase::loaded() const noexcept
{
    return loaded_;
}

std::size_t KnowledgeBase::faq_count() const noexcept
{
    if (!loaded_) {
        return 0;
    }

    return data_["faq"].size();
}

std::size_t KnowledgeBase::direction_count() const noexcept
{
    if (!loaded_) {
        return 0;
    }

    return data_["research_directions"].size();
}

std::size_t KnowledgeBase::project_count() const noexcept
{
    if (!loaded_) {
        return 0;
    }

    return data_["projects"].size();
}

std::size_t KnowledgeBase::equipment_count() const noexcept
{
    if (!loaded_) {
        return 0;
    }

    return data_["equipment"].size();
}

KnowledgeResult KnowledgeBase::fallback_result() const
{
    KnowledgeResult result;
    result.found = false;
    result.category = "fallback";

    if (data_.is_object() &&
        data_.contains("fallback_answer") &&
        data_["fallback_answer"].is_string())
    {
        result.answer =
            data_["fallback_answer"].get<std::string>();
    }

    if (result.answer.empty()) {
        result.answer = kDefaultFallback;
    }

    return result;
}

}  // namespace g1_voice_router
```


## A.3 `test/knowledge_base_test.cpp`

- **远端实际路径**：`/home/unitree/unitree_ros2/example/g1_voice_router/test/knowledge_base_test.cpp`

- **本地快照路径**：`remote_snapshot/g1_voice_router/test/knowledge_base_test.cpp`

- **变更状态**：新增

- **SHA256**：`dab2aef7156ff2cf3762f98a9d1681f38200f140ebb865482d4749c3c9c742fa`

- **行数**：362


```cpp
#include "g1_voice_router/knowledge_base.hpp"

#include <cctype>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <unistd.h>

using g1_voice_router::KnowledgeBase;

namespace
{

int passed = 0;
int failed = 0;

void check(
    const std::string & name,
    bool condition,
    const std::string & detail = "")
{
    if (condition) {
        ++passed;
        std::cout << "[PASS] " << name << '\n';
    } else {
        ++failed;
        std::cout << "[FAIL] " << name;

        if (!detail.empty()) {
            std::cout << " | " << detail;
        }

        std::cout << '\n';
    }
}

std::string temp_path(const std::string & suffix)
{
    return "/tmp/g1_kb_test_" +
           std::to_string(static_cast<long long>(getpid())) +
           suffix;
}

bool write_file(
    const std::string & path,
    const std::string & content)
{
    std::ofstream output(path, std::ios::binary);
    output << content;
    return static_cast<bool>(output);
}

bool has_text(
    const g1_voice_router::KnowledgeResult & result,
    const std::string & text)
{
    return result.answer.find(text) != std::string::npos;
}

// 检测“阿拉伯数字 + 台”形式的数量断言，例如“3台”。
bool has_equipment_count(
    const std::string & text)
{
    for (std::size_t index = 0; index + 4 <= text.size(); ++index) {
        if (std::isdigit(
                static_cast<unsigned char>(text[index])) &&
            text.compare(index + 1, 3, "台") == 0)
        {
            return true;
        }
    }

    return false;
}

}  // namespace

int main(int argc, char ** argv)
{
    const std::string knowledge_path =
        argc > 1 ? argv[1] : "config/lab_knowledge.json";

    std::cout << "========== KnowledgeBase Test ==========\n";
    std::cout << "Knowledge file: " << knowledge_path << '\n';

    KnowledgeBase knowledge;
    std::string error;

    check(
        "normal JSON load",
        knowledge.load(knowledge_path, error),
        error);

    check("loaded flag is true", knowledge.loaded());
    check("FAQ count is 20", knowledge.faq_count() == 20);
    check(
        "research direction count is 3",
        knowledge.direction_count() == 3);
    check("project count is 1", knowledge.project_count() == 1);
    check("equipment count is 1", knowledge.equipment_count() == 1);

    auto result = knowledge.query("介绍一下实验室");
    check(
        "introduction query",
        result.found &&
        result.category == "introduction" &&
        has_text(result, "脑机交互智能研究中心"),
        result.answer);

    result = knowledge.query("你好，介绍一下你们实验室");
    check(
        "introduction query with greeting prefix",
        result.found &&
        result.category == "introduction" &&
        has_text(result, "脑机交互智能研究中心"),
        result.answer);

    result = knowledge.query("请问重点实验室研究什么");
    check(
        "FAQ question embedded in a longer sentence",
        result.found &&
        has_text(result, "多模态融合学习"),
        result.answer);

    result = knowledge.query("你们实验室主要研究什么");
    check(
        "research directions query",
        result.found &&
        result.category == "research_directions" &&
        has_text(result, "神经信号感知") &&
        has_text(result, "多模态融合学习"),
        result.answer);

    result = knowledge.query("脑机中心有哪些研究方向");
    check(
        "BCI directions FAQ",
        result.found && has_text(result, "类脑智能系统"),
        result.answer);

    result = knowledge.query("重点实验室研究什么");
    check(
        "embodied lab research FAQ",
        result.found &&
        has_text(result, "多模态融合学习") &&
        has_text(result, "域泛化"),
        result.answer);

    result = knowledge.query("学院有什么在研项目");
    check(
        "project query",
        result.found &&
        result.category == "projects" &&
        has_text(result, "500万"),
        result.answer);

    result = knowledge.query("实验室有哪些设备");
    check(
        "equipment query",
        result.found &&
        result.category == "equipment" &&
        has_text(result, "智能机器狗"),
        result.answer);

    result = knowledge.query("脑机中心负责人是谁");
    check(
        "FAQ exact leader hit",
        result.found && has_text(result, "蒋田仔"),
        result.answer);

    result = knowledge.query("脑机中心是干什么的");
    check(
        "Chinese synonym expression",
        result.found &&
        has_text(result, "脑机交互智能研究中心"),
        result.answer);

    result = knowledge.query("重点实验室和赛力斯是什么关系");
    check(
        "SERES co-building FAQ",
        result.found && has_text(result, "共建单位"),
        result.answer);

    result = knowledge.query("脑机中心和重点实验室是一回事吗");
    check(
        "two labs are distinguished",
        result.found && has_text(result, "不是同一个名称"),
        result.answer);

    result = knowledge.query("实验室在哪里");
    check(
        "location query",
        result.found && has_text(result, "普福大道"),
        result.answer);

    result = knowledge.query("学院有哪些科研团队");
    check(
        "college teams FAQ",
        result.found && has_text(result, "机器博弈"),
        result.answer);

    result = knowledge.query("学院学生拿过什么奖");
    check(
        "college awards FAQ",
        result.found && has_text(result, "15项"),
        result.answer);

    result = knowledge.query("实验室有几台机器人");
    check(
        "unverified equipment count is not invented",
        result.found &&
        !has_equipment_count(result.answer) &&
        result.answer.find("G1") == std::string::npos,
        result.answer);

    result = knowledge.query("实验室有多少人");
    check(
        "unknown team size falls back",
        !result.found &&
        result.category == "fallback" &&
        !result.answer.empty(),
        result.answer);

    result = knowledge.query("今天天气怎么样");
    check(
        "no-match question falls back",
        !result.found &&
        result.category == "fallback" &&
        !result.answer.empty(),
        result.answer);

    result = knowledge.query("");
    check(
        "empty input falls back",
        !result.found &&
        result.category == "fallback" &&
        !result.answer.empty(),
        result.answer);

    const std::vector<std::string> answer_checks = {
        "介绍一下实验室",
        "你们实验室主要研究什么",
        "脑机中心负责人是谁",
        "学院有什么在研项目",
        "实验室有哪些设备",
        "实验室在哪里"
    };

    bool answers_have_no_url = true;

    for (const auto & query : answer_checks) {
        const auto answer = knowledge.query(query);

        if (answer.answer.find("http://") != std::string::npos ||
            answer.answer.find("https://") != std::string::npos ||
            answer.answer.find("www.") != std::string::npos)
        {
            answers_have_no_url = false;
            break;
        }
    }

    check("answers contain no source URL", answers_have_no_url);

    const std::string missing_path = temp_path("_missing.json");
    KnowledgeBase missing;
    check(
        "missing file is rejected",
        !missing.load(missing_path, error) &&
        !missing.loaded(),
        error);
    check(
        "query after failed load uses fallback",
        !missing.query("介绍一下实验室").found);

    const std::string malformed_path = temp_path("_malformed.json");
    check(
        "malformed fixture written",
        write_file(malformed_path, "{ this is not json"));
    check(
        "malformed JSON is rejected",
        !missing.load(malformed_path, error),
        error);

    const std::string unsupported_path = temp_path("_unsupported.json");
    check(
        "unsupported fixture written",
        write_file(unsupported_path, R"({"schema_version": 2})"));
    check(
        "unsupported schema is rejected",
        !missing.load(unsupported_path, error),
        error);

    const std::string empty_path = temp_path("_empty_fields.json");
    const std::string minimal_json = R"JSON({
      "schema_version": 1,
      "school_name": "测试学校",
      "college_name": "测试学院",
      "lab_name": "测试实验室",
      "location": "",
      "introduction": "",
      "research_directions": [],
      "projects": [],
      "equipment": [],
      "categories": [],
      "faq": [],
      "fallback_answer": "测试兜底回答。",
      "sources": [
        {
          "id": "S99",
          "title": "测试来源",
          "publisher": "测试",
          "url": "https://example.com",
          "date": "",
          "level": "A",
          "access_date": "2026-10-31",
          "verification": "test"
        }
      ]
    })JSON";

    check(
        "empty-fields fixture written",
        write_file(empty_path, minimal_json));

    KnowledgeBase empty_fields;
    check(
        "empty fields load",
        empty_fields.load(empty_path, error),
        error);

    result = empty_fields.query("介绍一下实验室");
    check(
        "empty introduction is not spoken",
        !result.found &&
        result.answer == "测试兜底回答。",
        result.answer);

    result = empty_fields.query("实验室在哪里");
    check(
        "empty location is not spoken",
        !result.found &&
        result.answer == "测试兜底回答。",
        result.answer);

    check(
        "empty path is rejected",
        !KnowledgeBase().load("", error),
        error);

    std::remove(malformed_path.c_str());
    std::remove(unsupported_path.c_str());
    std::remove(empty_path.c_str());

    std::cout << "=========================================\n";
    std::cout << "Passed: " << passed << '\n';
    std::cout << "Failed: " << failed << '\n';

    return failed == 0 ? 0 : 1;
}
```


## A.4 `include/g1_voice_router/intent_recognizer.hpp`

- **远端实际路径**：`/home/unitree/unitree_ros2/example/g1_voice_router/include/g1_voice_router/intent_recognizer.hpp`

- **本地快照路径**：`remote_snapshot/g1_voice_router/include/g1_voice_router/intent_recognizer.hpp`

- **变更状态**：未修改

- **SHA256**：`a208e31fdf4970177438b98fb2def583b6ae459f22d9cec22387fc706ae709c5`

- **行数**：47


```cpp
#ifndef G1_VOICE_ROUTER_INTENT_RECOGNIZER_HPP_
#define G1_VOICE_ROUTER_INTENT_RECOGNIZER_HPP_

#include <string>

namespace g1_voice_router
{

// 用户意图类型
enum class Intent
{
    GREETING,       // 问候
    LAB_INTRO,      // 实验室相关问题
    ROBOT_INTRO,    // 机器人介绍
    DEMO_ACTION,    // 动作演示
    ROBOT_STATUS,   // 机器人状态查询
    GOODBYE,        // 告别
    GENERAL_QA      // 普通问题
};

// 一次意图识别的结果
struct IntentResult
{
    Intent intent{Intent::GENERAL_QA};

    // 去除标点、空格并进行英文小写转换后的文本
    std::string normalized_text;
};

// 意图识别器
class IntentRecognizer
{
public:
    // 输入 ASR 识别文本，返回对应意图
    IntentResult recognize(const std::string &text) const;

    // 将枚举类型转换为可读字符串，方便日志输出
    static const char *to_string(Intent intent) noexcept;

private:
    // 文本归一化
    static std::string normalize(const std::string &text);
};

}  // namespace g1_voice_router

#endif  // G1_VOICE_ROUTER_INTENT_RECOGNIZER_HPP_
```


## A.5 `src/intent_recognizer.cpp`

- **远端实际路径**：`/home/unitree/unitree_ros2/example/g1_voice_router/src/intent_recognizer.cpp`

- **本地快照路径**：`remote_snapshot/g1_voice_router/src/intent_recognizer.cpp`

- **变更状态**：修改

- **SHA256**：`80d50eaaa8a75733f55bda629b5be19cf8fd9b0a921d8974e2441fda80640569`

- **行数**：310


```cpp
#include "g1_voice_router/intent_recognizer.hpp"

#include <algorithm>
#include <cctype>
#include <initializer_list>
#include <string>
#include <vector>

namespace g1_voice_router
{
namespace
{

// 检查文本是否包含指定词语中的任意一个
bool contains_any(
    const std::string &text,
    std::initializer_list<std::string> keywords)
{
    for (const auto &keyword : keywords)
    {
        if (text.find(keyword) != std::string::npos)
        {
            return true;
        }
    }

    return false;
}

}  // namespace


// ============================================================
// 文本归一化
// ============================================================

std::string IntentRecognizer::normalize(const std::string &text)
{
    std::string result = text;

    // 去除中英文标点、空格和换行
    static const std::vector<std::string> punctuation = {
        "。", "！", "？", "，", "、", "；", "：",
        "“", "”", "‘", "’",
        ".", ",", "!", "?", ";", ":",
        "\"", "'",
        " ", "\t", "\r", "\n"
    };

    for (const auto &mark : punctuation)
    {
        std::size_t pos = 0;

        while ((pos = result.find(mark, pos)) != std::string::npos)
        {
            result.erase(pos, mark.size());
        }
    }

    // 只转换 ASCII 英文字母，不改变 UTF-8 中文字节
    std::transform(
        result.begin(),
        result.end(),
        result.begin(),
        [](unsigned char c)
        {
            if (c < 0x80)
            {
                return static_cast<char>(std::tolower(c));
            }

            return static_cast<char>(c);
        });

    return result;
}


// ============================================================
// 意图识别
// ============================================================

IntentResult IntentRecognizer::recognize(
    const std::string &text) const
{
    IntentResult result;

    result.normalized_text = normalize(text);

    const std::string &input = result.normalized_text;

    // 空文本按普通问题处理，后续由上层决定是否忽略
    if (input.empty())
    {
        result.intent = Intent::GENERAL_QA;
        return result;
    }


    // --------------------------------------------------------
    // 1. GREETING：问候
    // 只匹配独立问候语，避免误判复合问题
    // --------------------------------------------------------

    if (input == "你好" ||
        input == "您好" ||
        input == "你好啊" ||
        input == "您好啊" ||
        input == "嗨" ||
        input == "嗨嗨" ||
        input == "哈喽" ||
        input == "hello" ||
        input == "hi" ||
        input == "早上好" ||
        input == "早安" ||
        input == "上午好" ||
        input == "下午好" ||
        input == "晚上好")
    {
        result.intent = Intent::GREETING;
        return result;
    }


    // --------------------------------------------------------
    // 2. GOODBYE：告别
    // 同样先匹配明确的独立告别语
    // --------------------------------------------------------

    if (input == "再见" ||
        input == "拜拜" ||
        input == "谢谢" ||
        input == "谢谢你" ||
        input == "辛苦了" ||
        input == "结束讲解" ||
        input == "结束接待" ||
        input == "讲解结束" ||
        input == "到此为止" ||
        input == "就到这里")
    {
        result.intent = Intent::GOODBYE;
        return result;
    }


    // --------------------------------------------------------
    // 3. LAB_INTRO：实验室相关问题
    // --------------------------------------------------------

    if (contains_any(input, {
        "实验室",
        "课题组",
        "研究方向",
        "科研项目",
        "实验设备",
        "实验成果",
        "脑机",
        "具身智能",
        "重点实验室",
        "开放平台",
        "赛力斯",
        "在研项目",
        "揭榜挂帅",
        "科研团队",
        "学院有哪些团队",
        "学院学生拿过什么奖",
        "学院竞赛",
        "学院什么时候成立",
        "学院成立",
        "学院地址",
        "学院在哪里",
        "学院主要研究",
        "肖汉光",
        "刘宏",
        "有机器狗吗"
    }))
    {
        result.intent = Intent::LAB_INTRO;
        return result;
    }


    // --------------------------------------------------------
    // 4. ROBOT_STATUS：机器人状态查询
    // --------------------------------------------------------

    if (contains_any(input, {
        "状态",
        "运行情况",
        "工作情况",
        "当前模式",
        "运行模式",
        "运动模式",
        "姿态",
        "关节状态",
        "动作状态",
        "有没有故障",
        "有故障吗",
        "运行正常吗",
        "正常吗",
        "当前任务"
    }))
    {
        result.intent = Intent::ROBOT_STATUS;
        return result;
    }


    // --------------------------------------------------------
    // 5. DEMO_ACTION：动作演示
    // --------------------------------------------------------

    if (contains_any(input, {
        "给我们展示一下",
        "展示一下",
        "演示一下",
        "表演一下",
        "给我们表演",
        "动作演示",
        "做个动作",
        "来个动作",
        "挥挥手",
        "挥手",
        "比心",
        "比个心",
        "举起双手",
        "跳个舞"
    }))
    {
        result.intent = Intent::DEMO_ACTION;
        return result;
    }


    // --------------------------------------------------------
    // 6. ROBOT_INTRO：机器人介绍
    // --------------------------------------------------------

    if (contains_any(input, {
        "你是谁",
        "你叫什么",
        "介绍你自己",
        "介绍一下你",
        "介绍一下自己",
        "你能做什么",
        "你会做什么",
        "你有什么功能",
        "你有什么能力",
        "你的功能",
        "你的能力",
        "你是什么机器人",
        "这是什么机器人",
        "介绍一下机器人",
        "机器人能做什么",
        "机器人有什么功能",
        "介绍一下g1",
        "g1能做什么",
        "宇树g1是什么"
    }))
    {
        result.intent = Intent::ROBOT_INTRO;
        return result;
    }


    // --------------------------------------------------------
    // 7. GENERAL_QA：其他问题交给大模型
    // --------------------------------------------------------

    result.intent = Intent::GENERAL_QA;

    return result;
}


// ============================================================
// 意图名称转换
// ============================================================

const char *IntentRecognizer::to_string(Intent intent) noexcept
{
    switch (intent)
    {
        case Intent::GREETING:
            return "GREETING";

        case Intent::LAB_INTRO:
            return "LAB_INTRO";

        case Intent::ROBOT_INTRO:
            return "ROBOT_INTRO";

        case Intent::DEMO_ACTION:
            return "DEMO_ACTION";

        case Intent::ROBOT_STATUS:
            return "ROBOT_STATUS";

        case Intent::GOODBYE:
            return "GOODBYE";

        case Intent::GENERAL_QA:
            return "GENERAL_QA";

        default:
            return "UNKNOWN";
    }
}

}  // namespace g1_voice_router
```


## A.6 `test/intent_recognizer_test.cpp`

- **远端实际路径**：`/home/unitree/unitree_ros2/example/g1_voice_router/test/intent_recognizer_test.cpp`

- **本地快照路径**：`remote_snapshot/g1_voice_router/test/intent_recognizer_test.cpp`

- **变更状态**：修改

- **SHA256**：`cd80b15f4542412051133e30a9a7faefc55eca7dbb20fe130778efd9c3abb3d7`

- **行数**：86


```cpp
#include "g1_voice_router/intent_recognizer.hpp"

#include <iostream>
#include <string>

using g1_voice_router::Intent;
using g1_voice_router::IntentRecognizer;

struct TestCase
{
    std::string input;
    Intent expected;
};

int main()
{
    IntentRecognizer recognizer;

    const TestCase cases[] = {
        {"你好", Intent::GREETING},
        {"您好！", Intent::GREETING},
        {"GOOD MORNING", Intent::GENERAL_QA},
        {"你好，介绍一下你们实验室", Intent::LAB_INTRO},
        {"你们实验室研究什么？", Intent::LAB_INTRO},
        {"介绍一下你自己", Intent::ROBOT_INTRO},
        {"你能做什么", Intent::ROBOT_INTRO},
        {"给我们展示一下", Intent::DEMO_ACTION},
        {"挥挥手", Intent::DEMO_ACTION},
        {"你现在状态怎么样", Intent::ROBOT_STATUS},
        {"当前运行模式是什么？", Intent::ROBOT_STATUS},
        {"谢谢", Intent::GOODBYE},
        {"再见！", Intent::GOODBYE},
        {"学院有什么在研项目", Intent::LAB_INTRO},
        {"脑机中心负责人是谁", Intent::LAB_INTRO},
        {"学院有哪些科研团队", Intent::LAB_INTRO},
        {"学院学生拿过什么奖", Intent::LAB_INTRO},
        {"学院什么时候成立", Intent::LAB_INTRO},
        {"学院地址在哪里", Intent::LAB_INTRO},
        {"具身智能开放平台是什么", Intent::LAB_INTRO},
        {"肖汉光教授是谁", Intent::LAB_INTRO},
        {"有机器狗吗", Intent::LAB_INTRO},
        {"学院有哪些专业", Intent::GENERAL_QA},
        {"机器狗多少钱", Intent::GENERAL_QA},
        {"宇树机器人多少钱？", Intent::GENERAL_QA}
    };

    int passed = 0;
    int failed = 0;

    std::cout << "========== Intent Recognizer Test =========="
              << std::endl;

    for (const auto &test : cases)
    {
        const auto result = recognizer.recognize(test.input);

        const bool success = result.intent == test.expected;

        std::cout
            << (success ? "[PASS] " : "[FAIL] ")
            << test.input
            << " -> "
            << IntentRecognizer::to_string(result.intent)
            << std::endl;

        if (success)
        {
            ++passed;
        }
        else
        {
            ++failed;

            std::cout
                << "  Expected: "
                << IntentRecognizer::to_string(test.expected)
                << std::endl;
        }
    }

    std::cout << "============================================" << std::endl;
    std::cout << "Passed: " << passed << std::endl;
    std::cout << "Failed: " << failed << std::endl;

    return failed == 0 ? 0 : 1;
}
```


## A.7 `src/voice_router.cpp`

- **远端实际路径**：`/home/unitree/unitree_ros2/example/g1_voice_router/src/voice_router.cpp`

- **本地快照路径**：`remote_snapshot/g1_voice_router/src/voice_router.cpp`

- **变更状态**：修改

- **SHA256**：`fd6d29c02757eb82dba517d53d2f3f72c82d324cc56741ae917e8e42988e0148`

- **行数**：688


```cpp
#include <curl/curl.h>
#include <algorithm>
#include <ament_index_cpp/get_package_share_directory.hpp>
#include <nlohmann/json.hpp>

#include <cctype>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <iterator>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>

#include "g1_voice_router/intent_recognizer.hpp"
#include "g1_voice_router/knowledge_base.hpp"
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"

using Json = nlohmann::json;

class G1VoiceRouter : public rclcpp::Node
{
public:
  G1VoiceRouter()
  : Node("g1_voice_router")
  {
    ollama_url_ = declare_parameter<std::string>(
      "ollama_url",
      "http://192.168.123.99:11434/api/chat");

    model_name_ = declare_parameter<std::string>(
      "model",
      "qwen3-vl:4b-instruct");

    timeout_seconds_ = declare_parameter<int>(
      "request_timeout_seconds", 60);

    if (timeout_seconds_ < 5 || timeout_seconds_ > 300) {
      RCLCPP_WARN(
        get_logger(),
        "Invalid request timeout; using 60 seconds");
      timeout_seconds_ = 60;
    }

    std::string default_knowledge_file;

    try {
      default_knowledge_file =
        ament_index_cpp::get_package_share_directory(
          "g1_voice_router") +
        "/config/lab_knowledge.json";
    } catch (const std::exception & error) {
      RCLCPP_ERROR(
        get_logger(),
        "Failed to locate g1_voice_router share directory: %s",
        error.what());
    }

    knowledge_file_ = declare_parameter<std::string>(
      "knowledge_file",
      default_knowledge_file);

    std::string knowledge_error;

    if (!knowledge_base_.load(knowledge_file_, knowledge_error)) {
      RCLCPP_ERROR(
        get_logger(),
        "KnowledgeBase load failed: %s",
        knowledge_error.c_str());
    } else {
      RCLCPP_INFO(
        get_logger(),
        "KnowledgeBase loaded: %s "
        "(FAQ=%zu, directions=%zu, projects=%zu, equipment=%zu)",
        knowledge_file_.c_str(),
        knowledge_base_.faq_count(),
        knowledge_base_.direction_count(),
        knowledge_base_.project_count(),
        knowledge_base_.equipment_count());
    }

    system_prompt_ = declare_parameter<std::string>(
      "system_prompt",
      "你是运行在宇树G1人形机器人中的中文语音助手。"
      "用户输入通常来自语音识别，可能存在少量错字。"
      "请使用简体中文，回答自然、简洁，适合直接语音播报，"
      "通常回答一到三句话。不要使用Markdown、表格或代码块。"
      "不要编造机器人实时状态；没有传感器数据时要诚实说明。");

    // 保留最近的对话上下文，第一条始终是系统提示词。
    history_.push_back(Json{
      {"role", "system"},
      {"content", system_prompt_}
    });

    // 输出仍然使用已通过测试的 TTS 接口。
    publisher_ =
      create_publisher<std_msgs::msg::String>(
        "/g1/tts", 10);

    // 发布固定白名单动作请求给 g1_action_executor。
    action_publisher_ =
      create_publisher<std_msgs::msg::String>(
        "/g1/action_request", 10);


    // 输入仍然使用已通过测试的 ASR 桥接接口。
    subscription_ =
      create_subscription<std_msgs::msg::String>(
        "/g1/asr_text",
        10,
        [this](const std_msgs::msg::String::SharedPtr msg) {
          enqueue_user_text(msg->data);
        });

    // 先建立订阅和发布接口，再启动工作线程。
    worker_ = std::thread(
      &G1VoiceRouter::worker_loop, this);

    RCLCPP_INFO(
      get_logger(),
      "G1 voice router started");

    RCLCPP_INFO(
      get_logger(),
      "Ollama URL: %s",
      ollama_url_.c_str());

    RCLCPP_INFO(
      get_logger(),
      "Model: %s",
      model_name_.c_str());

    RCLCPP_INFO(
      get_logger(),
      "Listening on /g1/asr_text, publishing to /g1/tts");
  }

  ~G1VoiceRouter() override
  {
    stop_worker();
  }

  // 退出时停止接收新任务，并等待正在进行的请求结束。
  void stop_worker()
  {
    {
      std::lock_guard<std::mutex> lock(queue_mutex_);
      stopping_ = true;
      queue_.clear();
    }

    queue_cv_.notify_all();

    if (worker_.joinable()) {
      worker_.join();
    }
  }

private:
  static std::size_t write_callback(
    char * data,
    std::size_t size,
    std::size_t count,
    void * user_data)
  {
    const std::size_t bytes = size * count;

    auto * response =
      static_cast<std::string *>(user_data);

    response->append(data, bytes);
    return bytes;
  }

  static std::string trim(std::string text)
  {
    const auto not_space = [](unsigned char c) {
      return std::isspace(c) == 0;
    };

    const auto first = std::find_if(
      text.begin(), text.end(), not_space);

    const auto last = std::find_if(
      text.rbegin(), text.rend(), not_space).base();

    if (first >= last) {
      return {};
    }

    return std::string(first, last);
  }


  static std::string normalize_intent_text(std::string text)
  {
    text = trim(std::move(text));
  
    static const std::string punctuation[] = {
      "。", "！", "？", "，", "、", "；", "：",
      ".", ",", "!", "?", ";", ":",
      "\"", "'", "“", "”", " ",
      "\t", "\r", "\n"
    };
  
    for (const auto & mark : punctuation) {
      std::size_t pos = 0;
      while ((pos = text.find(mark, pos)) != std::string::npos) {
        text.erase(pos, mark.size());
      }
    }
  
    std::transform(
      text.begin(), text.end(), text.begin(),
      [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
      });
  
    return text;
  }
  
  static bool match_greeting_reply(
    const std::string & raw_text,
    std::string & reply)
  {
    const std::string text = normalize_intent_text(raw_text);
  
    if (text == "你好" ||
        text == "您好" ||
        text == "你好啊" ||
        text == "嗨" ||
        text == "嗨嗨" ||
        text == "哈喽" ||
        text == "hello" ||
        text == "hi") {
      reply = "你好！很高兴见到你。";
      return true;
    }
  
    if (text == "早上好" || text == "早安" || text == "上午好") {
      reply = "早上好！今天也一起加油。";
      return true;
    }
  
    if (text == "下午好") {
      reply = "下午好！很高兴见到你。";
      return true;
    }
  
    if (text == "晚上好") {
      reply = "晚上好！今天辛苦了。";
      return true;
    }
  
    return false;
  }
  

  void enqueue_user_text(const std::string & raw_text)
  {
    const std::string text = trim(raw_text);

    if (text.empty()) {
      RCLCPP_WARN(
        get_logger(),
        "Received empty ASR text; ignored");
      return;
    }

    const auto intent_result =
      intent_recognizer_.recognize(text);

    RCLCPP_INFO(
      get_logger(),
      "Intent recognized: %s | text: %s",
      g1_voice_router::IntentRecognizer::to_string(
        intent_result.intent),
      intent_result.normalized_text.c_str());



      // 明确问候语走固定规则，不经过 Ollama。
      // 动作请求和 TTS 回复分别发布，由两个节点并行处理。
      std::string greeting_reply;
    
      if (intent_result.intent == g1_voice_router::Intent::GREETING &&
        match_greeting_reply(text, greeting_reply)) {
        std_msgs::msg::String action_msg;
        action_msg.data = "wave";
        action_publisher_->publish(action_msg);
    
        RCLCPP_INFO(
          get_logger(),
          "Greeting intent detected: [%s], published action [wave]",
          text.c_str());
    
        publish_reply(greeting_reply);
        return;
      }
    

    // --------------------------------------------------------
    // LAB_INTRO：本地知识库
    // 有资料时直接回答；资料缺失时返回安全兜底。
    // 不进入 Ollama 消息队列。
    // --------------------------------------------------------

    if (intent_result.intent ==
        g1_voice_router::Intent::LAB_INTRO)
    {
      const auto result = knowledge_base_.query(text);

      publish_reply(result.answer);

      RCLCPP_INFO(
        get_logger(),
        "LAB_INTRO local knowledge base completed: "
        "found=%s category=%s matched_id=%s",
        result.found ? "true" : "false",
        result.category.c_str(),
        result.matched_id.c_str());

      return;
    }


    // --------------------------------------------------------
    // ROBOT_INTRO：机器人自我介绍
    // 本地固定回答，不调用 Ollama
    // --------------------------------------------------------

    if (intent_result.intent ==
        g1_voice_router::Intent::ROBOT_INTRO)
    {
      const std::string reply =
        "我是宇树 G1 EDU 人形机器人。"
        "我可以通过语音识别接收你的问题，"
        "连接外部大语言模型进行中文对话，"
        "还可以执行挥手、举手和比心等预设手臂动作。"
        "很高兴为你介绍我们的机器人系统。";

      publish_reply(reply);

      RCLCPP_INFO(
        get_logger(),
        "ROBOT_INTRO local handling completed");

      return;
    }


    // --------------------------------------------------------
    // GOODBYE：告别
    // 本地固定回答，不调用 Ollama
    // --------------------------------------------------------

    if (intent_result.intent ==
        g1_voice_router::Intent::GOODBYE)
    {
      const std::string reply =
        "感谢你的参观！"
        "很高兴和你交流，祝你今天愉快，再见！";

      publish_reply(reply);

      RCLCPP_INFO(
        get_logger(),
        "GOODBYE local handling completed");

      return;
    }


    std::size_t queue_size = 0;

    {
      std::lock_guard<std::mutex> lock(queue_mutex_);

      if (stopping_) {
        return;
      }

      // 不做文本去重，每条 ASR 消息独立处理。
      queue_.push_back(text);
      queue_size = queue_.size();
    }

    RCLCPP_INFO(
      get_logger(),
      "ASR text queued for Ollama; pending=%zu",
      queue_size);

    queue_cv_.notify_one();
  }

  bool query_ollama(
    const std::string & user_text,
    std::string & answer)
  {
    Json messages = Json::array();

    for (const auto & message : history_) {
      messages.push_back(message);
    }

    messages.push_back(Json{
      {"role", "user"},
      {"content", user_text}
    });

    const Json request_json = {
      {"model", model_name_},
      {"messages", messages},
      {"stream", false},
      {"options", {
        {"temperature", 0.2},
        {"num_predict", 160}
      }}
    };

    const std::string request_body = request_json.dump();
    std::string response_body;

    CURL * curl = curl_easy_init();

    if (curl == nullptr) {
      RCLCPP_ERROR(
        get_logger(),
        "Failed to create CURL request");
      return false;
    }

    curl_slist * headers = nullptr;
    headers = curl_slist_append(
      headers, "Content-Type: application/json");

    if (headers == nullptr) {
      curl_easy_cleanup(curl);

      RCLCPP_ERROR(
        get_logger(),
        "Failed to create HTTP headers");
      return false;
    }

    curl_easy_setopt(
      curl, CURLOPT_URL, ollama_url_.c_str());

    curl_easy_setopt(
      curl, CURLOPT_HTTPHEADER, headers);

    curl_easy_setopt(
      curl, CURLOPT_POST, 1L);

    curl_easy_setopt(
      curl, CURLOPT_POSTFIELDS, request_body.c_str());

    curl_easy_setopt(
      curl, CURLOPT_POSTFIELDSIZE,
      static_cast<long>(request_body.size()));

    curl_easy_setopt(
      curl, CURLOPT_WRITEFUNCTION,
      &G1VoiceRouter::write_callback);

    curl_easy_setopt(
      curl, CURLOPT_WRITEDATA, &response_body);

    curl_easy_setopt(
      curl, CURLOPT_CONNECTTIMEOUT_MS, 3000L);

    curl_easy_setopt(
      curl, CURLOPT_TIMEOUT_MS,
      static_cast<long>(timeout_seconds_) * 1000L);

    // 在后台线程中执行同步 HTTP 请求。
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

    const CURLcode result = curl_easy_perform(curl);

    long http_status = 0;
    curl_easy_getinfo(
      curl, CURLINFO_RESPONSE_CODE, &http_status);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (result != CURLE_OK) {
      RCLCPP_ERROR(
        get_logger(),
        "Ollama HTTP request failed: %s",
        curl_easy_strerror(result));
      return false;
    }

    if (http_status < 200 || http_status >= 300) {
      const std::string detail =
        response_body.substr(0, 300);

      RCLCPP_ERROR(
        get_logger(),
        "Ollama HTTP status %ld; response: %s",
        http_status, detail.c_str());
      return false;
    }

    try {
      const Json response =
        Json::parse(response_body);

      if (!response.contains("message") ||
          !response["message"].is_object() ||
          !response["message"].contains("content") ||
          !response["message"]["content"].is_string()) {
        RCLCPP_ERROR(
          get_logger(),
          "Ollama response has no message.content field");
        return false;
      }

      answer = trim(
        response["message"]["content"].get<std::string>());

      if (answer.empty()) {
        RCLCPP_ERROR(
          get_logger(),
          "Ollama returned an empty answer");
        return false;
      }

      // 只有请求成功时才保存这轮对话。
      history_.push_back(Json{
        {"role", "user"},
        {"content", user_text}
      });

      history_.push_back(Json{
        {"role", "assistant"},
        {"content", answer}
      });

      // 保留系统提示词和最近最多六轮对话。
      while (history_.size() > 13) {
        history_.erase(std::next(history_.begin()));
        history_.erase(std::next(history_.begin()));
      }

      return true;
    } catch (const Json::exception & error) {
      RCLCPP_ERROR(
        get_logger(),
        "Failed to parse Ollama JSON: %s",
        error.what());
      return false;
    }
  }

  void publish_reply(const std::string & reply)
  {
    if (!rclcpp::ok()) {
      return;
    }

    std_msgs::msg::String output;
    output.data = reply;

    publisher_->publish(output);

    RCLCPP_INFO(
      get_logger(),
      "Reply published to /g1/tts (%zu bytes)",
      reply.size());
  }

  void worker_loop()
  {
    while (true) {
      std::string user_text;

      {
        std::unique_lock<std::mutex> lock(queue_mutex_);

        queue_cv_.wait(lock, [this]() {
          return stopping_ || !queue_.empty();
        });

        if (stopping_) {
          return;
        }

        user_text = std::move(queue_.front());
        queue_.pop_front();
      }

      RCLCPP_INFO(
        get_logger(),
        "Querying Ollama model: %s",
        model_name_.c_str());

      std::string reply;

      if (!query_ollama(user_text, reply)) {
        if (!rclcpp::ok()) {
          continue;
        }

        // 网络、模型或 API 出错时给出可听到的提示。
        reply = "抱歉，我暂时无法连接到对话模型。";

        RCLCPP_WARN(
          get_logger(),
          "Using fallback reply after Ollama failure");
      }

      publish_reply(reply);
    }
  }

  std::string ollama_url_;
  std::string model_name_;
  std::string system_prompt_;
  int timeout_seconds_{60};

  std::deque<Json> history_;

  std::string knowledge_file_;

  g1_voice_router::KnowledgeBase knowledge_base_;

  g1_voice_router::IntentRecognizer intent_recognizer_;

  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr
    subscription_;

  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr
    publisher_;

  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr
    action_publisher_;

  std::mutex queue_mutex_;
  std::condition_variable queue_cv_;
  std::deque<std::string> queue_;
  bool stopping_{false};
  std::thread worker_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);

  const CURLcode curl_status =
    curl_global_init(CURL_GLOBAL_DEFAULT);

  if (curl_status != CURLE_OK) {
    RCLCPP_ERROR(
      rclcpp::get_logger("g1_voice_router"),
      "curl_global_init failed");

    rclcpp::shutdown();
    return 1;
  }

  try {
    auto node = std::make_shared<G1VoiceRouter>();

    rclcpp::spin(node);

    node->stop_worker();
    node.reset();
  } catch (const std::exception & error) {
    RCLCPP_ERROR(
      rclcpp::get_logger("g1_voice_router"),
      "Fatal error: %s",
      error.what());
  }

  if (rclcpp::ok()) {
    rclcpp::shutdown();
  }

  curl_global_cleanup();
  return 0;
}
```


## A.8 `CMakeLists.txt`

- **远端实际路径**：`/home/unitree/unitree_ros2/example/g1_voice_router/CMakeLists.txt`

- **本地快照路径**：`remote_snapshot/g1_voice_router/CMakeLists.txt`

- **变更状态**：修改

- **SHA256**：`a7166376f6335ae346250d5b4af23e9fe9a6d624bf117420906ec46cee2feb8c`

- **行数**：94


```cmake
cmake_minimum_required(VERSION 3.8)
project(g1_voice_router)

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(ament_index_cpp REQUIRED)
find_package(CURL REQUIRED)
find_package(Threads REQUIRED)

add_library(knowledge_base STATIC
  src/knowledge_base.cpp
)

target_compile_features(knowledge_base PUBLIC cxx_std_17)

# 复用工作区现有的 nlohmann/json.hpp。
target_include_directories(knowledge_base PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/include"
)

add_executable(voice_router
  src/voice_router.cpp
  src/intent_recognizer.cpp
)

target_compile_features(voice_router PUBLIC cxx_std_17)

target_include_directories(voice_router PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/include"
)

ament_target_dependencies(voice_router
  rclcpp
  std_msgs
  ament_index_cpp
)

target_link_libraries(voice_router
  knowledge_base
  CURL::libcurl
  Threads::Threads
)

install(
  TARGETS voice_router
  DESTINATION lib/${PROJECT_NAME}
)

install(
  FILES config/lab_knowledge.json
  DESTINATION share/${PROJECT_NAME}/config
)

include(CTest)

if(BUILD_TESTING)
  add_executable(knowledge_base_test
    test/knowledge_base_test.cpp
  )

  target_link_libraries(knowledge_base_test
    knowledge_base
  )

  target_include_directories(knowledge_base_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/include"
  )

  add_test(
    NAME knowledge_base_test
    COMMAND knowledge_base_test
      "${CMAKE_CURRENT_SOURCE_DIR}/config/lab_knowledge.json"
  )

  add_executable(intent_recognizer_test
    test/intent_recognizer_test.cpp
    src/intent_recognizer.cpp
  )

  target_include_directories(intent_recognizer_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
  )

  add_test(
    NAME intent_recognizer_test
    COMMAND intent_recognizer_test
  )
endif()

ament_package()
```


## A.9 `package.xml`

- **远端实际路径**：`/home/unitree/unitree_ros2/example/g1_voice_router/package.xml`

- **本地快照路径**：`remote_snapshot/g1_voice_router/package.xml`

- **变更状态**：修改

- **SHA256**：`22d0f4a6341c455bc83122d7983c15490a0bd96730136dab247ae5d2a42dcb39`

- **行数**：19


```xml
<?xml version="1.0"?>
<package format="3">
  <name>g1_voice_router</name>
  <version>0.2.0</version>
  <description>G1 voice router with remote Ollama inference</description>

  <maintainer email="unitree@example.com">G1 Developer</maintainer>
  <license>Apache-2.0</license>

  <buildtool_depend>ament_cmake</buildtool_depend>
  <depend>rclcpp</depend>
  <depend>std_msgs</depend>
  <depend>ament_index_cpp</depend>
  <depend>libcurl-dev</depend>

  <export>
    <build_type>ament_cmake</build_type>
  </export>
</package>
```


## A.10 远端包内遗留备份文件清单

以下 3 个文件位于远端 `src/`，不属于构建目标，不参与编译；本附录不展开其全文，仅记录存在性与哈希。

| 文件 | 大小 | SHA256 | 说明 |
|---|---|---|---|
| `src/voice_router.cpp.bak` | 2351 B | `24eb907d05b048d21babbc533a4b23686757215bebd1edfd028bf8619d373dec` | 旧版 voice_router.cpp 备份 |
| `src/voice_router.cpp.before_action` | 10471 B | `1f20d072af8516dcfa025ade403baf043fc99fce449edc6bbea4da0639724dd2` | 加入动作发布前的备份 |
| `src/voice_router.cpp.before_local_reception` | 13304 B | `87a95c5f3792c55eb4c905c60248a7321344d38c5112690be85152044d98bf8c` | 本地接待逻辑前的备份 |

> 说明：这些 `.bak` 文件不是 CMake 源文件列表的一部分；删除或保留不影响当前构建，但本轮未做任何删除。
