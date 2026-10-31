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
