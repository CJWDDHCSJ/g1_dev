#include "g1_voice_router/knowledge_base.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
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

bool contains_phrase(
    const std::string & text,
    std::initializer_list<const char *> phrases)
{
    for (const char * phrase : phrases) {
        if (text.find(phrase) != std::string::npos) {
            return true;
        }
    }

    return false;
}

struct EntityTerm
{
    const char * surface;
    const char * entity;
    int weight;
};

// 机构、人物等实体词。长词优先，避免“实验室”吞掉“重点实验室”。
const std::vector<EntityTerm> & entity_terms()
{
    static const std::vector<EntityTerm> terms = {
        {"人形机器人具身智能感知与自主学习重庆市重点实验室",
         "embodied_lab", 3},
        {"脑机交互智能研究中心", "bci_center", 3},
        {"脑机交互智能实验室", "bci_center", 3},
        {"具身智能开放平台", "open_platform", 3},
        {"具身智能实验室", "embodied_lab", 2},
        {"人形机器人实验室", "embodied_lab", 2},
        {"市级重点实验室", "embodied_lab", 2},
        {"重点实验室", "embodied_lab", 2},
        {"脑机中心", "bci_center", 2},
        {"具身智能平台", "open_platform", 2},
        {"开放平台", "open_platform", 2},
        {"脑机", "bci_center", 1},
        {"学院", "college", 2},
        {"肖汉光", "person_xiao", 3},
        {"刘宏", "person_liu", 3},
        {"赛力斯", "seres", 2},
        {"学生", "student", 1},
        {"实验室", "lab_generic", 1}
    };
    return terms;
}

std::map<std::string, int> extract_entities(
    const std::string & text)
{
    struct OrderedTerm
    {
        std::size_t length;
        std::string surface;
        const char * entity;
        int weight;
    };

    std::vector<OrderedTerm> ordered;

    for (const auto & term : entity_terms()) {
        const std::string surface = normalize_text(term.surface);

        if (surface.empty()) {
            continue;
        }

        ordered.push_back(
            {utf8_length(surface), surface, term.entity, term.weight});
    }

    std::sort(
        ordered.begin(), ordered.end(),
        [](const OrderedTerm & left, const OrderedTerm & right) {
            if (left.length != right.length) {
                return left.length > right.length;
            }
            if (left.surface != right.surface) {
                return left.surface < right.surface;
            }
            return std::string(left.entity) < std::string(right.entity);
        });

    std::map<std::string, int> entities;
    std::vector<bool> used(text.size(), false);

    for (const auto & term : ordered) {
        std::size_t position = 0;

        while ((position = text.find(term.surface, position)) !=
               std::string::npos)
        {
            bool overlaps = false;

            for (std::size_t offset = 0;
                 offset < term.surface.size(); ++offset)
            {
                if (used[position + offset]) {
                    overlaps = true;
                    break;
                }
            }

            if (!overlaps) {
                for (std::size_t offset = 0;
                     offset < term.surface.size(); ++offset)
                {
                    used[position + offset] = true;
                }

                int & weight = entities[term.entity];
                weight = std::max(weight, term.weight);
            }

            ++position;
        }
    }

    return entities;
}

std::set<std::string> extract_intents(
    const std::string & text)
{
    std::set<std::string> intents;

    const auto add = [&intents, &text](
        bool condition, const char * name) {
        if (condition) {
            intents.insert(name);
        }
    };

    add(contains_phrase(text, {
            "是什么", "是什么意思", "什么意思", "是啥",
            "什么叫", "什么是", "定义", "概念", "指什么",
            "指的是", "是做什么", "做什么的", "干什么的"
        }), "definition");

    add(contains_phrase(text, {
            "介绍", "概况", "简介", "讲解"
        }), "intro");

    add(contains_phrase(text, {
            "成立时间", "什么时候成立", "何时成立",
            "哪年成立", "哪一年成立", "啥时候成立",
            "什么时候建立", "何时建立", "哪年建立",
            "成立日期", "成立日"
        }), "establish");

    add(contains_phrase(text, {
            "获批", "批准", "批复", "获评", "审批"
        }), "approval");

    add(contains_phrase(text, {
            "多少人", "多少位", "几个人", "人数",
            "多少学生", "多少教师", "多少名"
        }), "quantity_people");

    add(contains_phrase(text, {
            "几台", "多少台", "多少机器人", "多少台机器人"
        }), "quantity_equipment");

    add(contains_phrase(text, {
            "设备", "机器人", "机器狗", "无人车",
            "载体", "型号", "硬件"
        }), "equipment");

    add(contains_phrase(text, {
            "研究什么", "研究方向", "主要研究", "研究内容",
            "科研方向", "做什么研究"
        }), "research");

    add(contains_phrase(text, {
            "在哪", "哪里", "哪儿", "地址", "位置",
            "怎么走", "哪个楼"
        }), "location");

    add(contains_phrase(text, {"团队"}), "team");

    add(contains_phrase(text, {
            "获奖", "奖项", "拿过什么奖", "竞赛", "比赛"
        }), "award");

    add(contains_phrase(text, {
            "项目", "揭榜挂帅", "在研"
        }), "project");

    add(contains_phrase(text, {
            "关系", "一回事", "区别", "不同", "同一个"
        }), "relation");

    add(contains_phrase(text, {
            "是谁", "谁是", "负责人", "主任",
            "院长", "教授", "老师"
        }), "person");

    return intents;
}

struct QueryFeatures
{
    std::map<std::string, int> entities;
    std::set<std::string> intents;
};

QueryFeatures extract_features(const std::string & text)
{
    QueryFeatures features;
    features.entities = extract_entities(text);
    features.intents = extract_intents(text);
    return features;
}

void merge_features(
    QueryFeatures & target,
    const QueryFeatures & source)
{
    for (const auto & entity : source.entities) {
        int & weight = target.entities[entity.first];
        weight = std::max(weight, entity.second);
    }

    target.intents.insert(
        source.intents.begin(), source.intents.end());
}

QueryFeatures keyword_features(const Json & keywords)
{
    QueryFeatures features;

    for (const auto & keyword : keywords) {
        if (!keyword.is_string()) {
            continue;
        }

        merge_features(
            features,
            extract_features(
                normalize_text(keyword.get<std::string>())));
    }

    return features;
}

QueryFeatures question_features(const Json & faq)
{
    QueryFeatures features;

    for (const auto & question : faq["questions"]) {
        if (!question.is_string()) {
            continue;
        }

        merge_features(
            features,
            extract_features(
                normalize_text(question.get<std::string>())));
    }

    return features;
}

std::vector<std::uint32_t> utf8_codepoints(
    const std::string & text)
{
    std::vector<std::uint32_t> codepoints;
    std::size_t index = 0;

    while (index < text.size()) {
        const unsigned char first =
            static_cast<unsigned char>(text[index]);
        std::uint32_t codepoint = first;
        std::size_t length = 1;

        if ((first & 0xE0U) == 0xC0U) {
            codepoint = first & 0x1FU;
            length = 2;
        } else if ((first & 0xF0U) == 0xE0U) {
            codepoint = first & 0x0FU;
            length = 3;
        } else if ((first & 0xF8U) == 0xF0U) {
            codepoint = first & 0x07U;
            length = 4;
        }

        if (index + length > text.size()) {
            length = 1;
            codepoint = first;
        }

        for (std::size_t offset = 1;
             offset < length; ++offset)
        {
            codepoint =
                (codepoint << 6) |
                (static_cast<unsigned char>(text[index + offset]) &
                 0x3FU);
        }

        codepoints.push_back(codepoint);
        index += length;
    }

    return codepoints;
}

std::size_t longest_common_substring(
    const std::vector<std::uint32_t> & left,
    const std::vector<std::uint32_t> & right)
{
    if (left.empty() || right.empty()) {
        return 0;
    }

    std::vector<std::size_t> previous(right.size() + 1, 0);
    std::vector<std::size_t> current(right.size() + 1, 0);
    std::size_t best = 0;

    for (std::size_t i = 1; i <= left.size(); ++i) {
        for (std::size_t j = 1; j <= right.size(); ++j) {
            if (left[i - 1] == right[j - 1]) {
                current[j] = previous[j - 1] + 1;
                best = std::max(best, current[j]);
            } else {
                current[j] = 0;
            }
        }

        previous.swap(current);
        std::fill(current.begin(), current.end(), 0);
    }

    return best;
}

int entity_overlap_score(
    const QueryFeatures & query,
    const QueryFeatures & candidate)
{
    int score = 0;

    for (const auto & entity : query.entities) {
        if (candidate.entities.find(entity.first) !=
            candidate.entities.end())
        {
            score += entity.second;
        }
    }

    return score;
}

int intent_overlap_count(
    const QueryFeatures & query,
    const QueryFeatures & candidate)
{
    int count = 0;

    for (const auto & intent : query.intents) {
        if (candidate.intents.find(intent) !=
            candidate.intents.end())
        {
            ++count;
        }
    }

    return count;
}

bool has_intent(
    const QueryFeatures & features,
    const char * intent)
{
    return features.intents.find(intent) !=
           features.intents.end();
}

bool compatible_intents(
    const QueryFeatures & query,
    const QueryFeatures & candidate)
{
    if (query.intents.empty()) {
        return true;
    }

    if (has_intent(query, "establish") &&
        !has_intent(candidate, "establish"))
    {
        return false;
    }

    if (has_intent(query, "approval") &&
        !has_intent(candidate, "approval"))
    {
        return false;
    }

    if (has_intent(query, "quantity_people") &&
        !has_intent(candidate, "quantity_people"))
    {
        return false;
    }

    if (has_intent(query, "quantity_equipment") &&
        !has_intent(candidate, "quantity_equipment"))
    {
        return false;
    }

    static const char * kSubstantiveIntents[] = {
        "definition", "intro", "establish", "approval",
        "quantity_people", "quantity_equipment", "equipment",
        "research", "location", "team", "award", "project",
        "relation", "person"
    };

    for (const char * intent : kSubstantiveIntents) {
        if (has_intent(query, intent) &&
            has_intent(candidate, intent))
        {
            return true;
        }
    }

    return false;
}

// 至少命中两个关键词，或命中一个不少于四个汉字的关键词。
int match_score(
    const std::string & text,
    const Json & keywords,
    int * longest_out = nullptr)
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

    if (longest_out != nullptr) {
        *longest_out = longest;
    }

    if (hits >= 2 || longest >= 4) {
        return score + hits;
    }

    return 0;
}

struct ScoredCandidate
{
    const Json * item{nullptr};
    std::string category;
    std::string match_type;
    int score{0};
    int entity_score{0};
    int intent_score{0};
    std::size_t specificity{0};
};

std::string candidate_answer(const ScoredCandidate & candidate)
{
    if (candidate.item == nullptr) {
        return {};
    }

    return candidate.item->value("answer", std::string{});
}

std::string candidate_id(const ScoredCandidate & candidate)
{
    if (candidate.item == nullptr) {
        return {};
    }

    return candidate.item->value("id", std::string{});
}

// 先比主分数，再比实体、意图、具体短语；完全并列且答案不同时判为歧义。
bool select_candidate(
    const std::vector<ScoredCandidate> & candidates,
    bool score_first,
    ScoredCandidate & selected)
{
    if (candidates.empty()) {
        return false;
    }

    const auto key = [score_first](const ScoredCandidate & candidate) {
        const int specificity =
            static_cast<int>(candidate.specificity);

        if (score_first) {
            return std::make_tuple(
                candidate.score,
                candidate.entity_score,
                candidate.intent_score,
                specificity);
        }

        return std::make_tuple(
            candidate.entity_score,
            candidate.intent_score,
            specificity,
            candidate.score);
    };

    auto best_key = key(candidates.front());

    for (const auto & candidate : candidates) {
        const auto candidate_key = key(candidate);

        if (candidate_key > best_key) {
            best_key = candidate_key;
        }
    }

    std::vector<const ScoredCandidate *> top;

    for (const auto & candidate : candidates) {
        if (key(candidate) == best_key) {
            top.push_back(&candidate);
        }
    }

    if (top.size() == 1) {
        selected = *top.front();
        return true;
    }

    const std::string first_answer =
        candidate_answer(*top.front());
    bool same_answer = true;

    for (const auto * candidate : top) {
        if (candidate_answer(*candidate) != first_answer) {
            same_answer = false;
            break;
        }
    }

    if (!same_answer) {
        return false;
    }

    const ScoredCandidate * chosen = top.front();

    for (const auto * candidate : top) {
        if (candidate_id(*candidate) < candidate_id(*chosen)) {
            chosen = candidate;
        }
    }

    selected = *chosen;
    return true;
}

KnowledgeResult make_clarification_result()
{
    KnowledgeResult result;
    result.found = true;
    result.category = "clarification";
    result.match_type = "clarification";
    result.answer =
        "您是想了解实验室的研究方向，还是想了解实验室有哪些设备？"
        "如果问的是研究设备，公开资料只披露具身智能开放平台"
        "使用智能机器人、特种无人车、智能机器狗等载体，"
        "具体型号和数量没有公开。";
    return result;
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
    const std::string & category,
    const std::string & match_type,
    int match_score)
{
    KnowledgeResult result;
    result.found = true;
    result.category = category;
    result.matched_id = item.value("id", std::string{});
    result.match_type = match_type;
    result.match_score = match_score;
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

    const QueryFeatures query_features = extract_features(text);
    const std::vector<std::uint32_t> query_codepoints =
        utf8_codepoints(text);

    // 1. FAQ 问题精确匹配。
    // 同一问题命中多条且答案不一致时，不按 JSON 数组顺序选择。
    std::vector<const Json *> exact_matches;
    std::set<std::string> exact_answers;

    for (const auto & faq : data_["faq"]) {
        for (const auto & question : faq["questions"]) {
            if (text == normalize_text(question.get<std::string>())) {
                exact_matches.push_back(&faq);
                exact_answers.insert(
                    faq.value("answer", std::string{}));
                break;
            }
        }
    }

    if (!exact_matches.empty()) {
        if (exact_answers.size() > 1) {
            return fallback_result("fallback_ambiguous_exact");
        }

        const Json * chosen = exact_matches.front();

        for (const Json * faq : exact_matches) {
            if (faq->value("id", std::string{}) <
                chosen->value("id", std::string{}))
            {
                chosen = faq;
            }
        }

        const KnowledgeResult result = make_result(
            *chosen,
            chosen->value("category", std::string("faq")),
            "exact_question",
            1000);

        if (!result.answer.empty()) {
            return result;
        }
    }

    // 2. FAQ 问题被包含在输入中。
    // 容忍“你好，介绍一下你们实验室”这类带前缀的说法；
    // 仍然先比实体和问题意图，再比问题短语长度。
    std::vector<ScoredCandidate> contained_candidates;

    for (const auto & faq : data_["faq"]) {
        for (const auto & question : faq["questions"]) {
            const std::string normalized_question =
                normalize_text(question.get<std::string>());
            const std::size_t question_length =
                utf8_length(normalized_question);

            if (question_length < 5 ||
                text.find(normalized_question) == std::string::npos)
            {
                continue;
            }

            const QueryFeatures candidate_features =
                extract_features(normalized_question);

            if (!compatible_intents(
                    query_features, candidate_features))
            {
                continue;
            }

            ScoredCandidate candidate;
            candidate.item = &faq;
            candidate.category =
                faq.value("category", std::string("faq"));
            candidate.match_type = "contained_question";
            candidate.entity_score = entity_overlap_score(
                query_features, candidate_features);
            candidate.intent_score = intent_overlap_count(
                query_features, candidate_features);
            candidate.specificity = question_length;
            candidate.score = static_cast<int>(question_length);
            contained_candidates.push_back(candidate);
        }
    }

    ScoredCandidate selected;

    if (select_candidate(contained_candidates, false, selected)) {
        const KnowledgeResult result = make_result(
            *selected.item,
            selected.category,
            "contained_question",
            static_cast<int>(selected.specificity));

        if (!result.answer.empty()) {
            return result;
        }
    } else if (!contained_candidates.empty()) {
        return fallback_result("fallback_ambiguous_contained");
    }

    // 3. 既问研究方向又问设备时先澄清，不随机选一类。
    if (has_intent(query_features, "research") &&
        has_intent(query_features, "equipment"))
    {
        return make_clarification_result();
    }

    // 4. 实体 + 问题意图约束的 FAQ 问题匹配。
    std::vector<ScoredCandidate> entity_candidates;

    for (const auto & faq : data_["faq"]) {
        for (const auto & question : faq["questions"]) {
            const std::string normalized_question =
                normalize_text(question.get<std::string>());
            const QueryFeatures candidate_features =
                extract_features(normalized_question);

            const int entity_score = entity_overlap_score(
                query_features, candidate_features);

            if (entity_score <= 0 ||
                !compatible_intents(
                    query_features, candidate_features))
            {
                continue;
            }

            const std::size_t specificity =
                longest_common_substring(
                    query_codepoints,
                    utf8_codepoints(normalized_question));

            if (query_features.intents.empty()) {
                if (specificity < 4) {
                    continue;
                }

                if (specificity * 10 <
                    query_codepoints.size() * 8)
                {
                    continue;
                }
            } else if (specificity < 2) {
                continue;
            }

            ScoredCandidate candidate;
            candidate.item = &faq;
            candidate.category =
                faq.value("category", std::string("faq"));
            candidate.match_type = "entity_intent_question";
            candidate.entity_score = entity_score;
            candidate.intent_score = intent_overlap_count(
                query_features, candidate_features);
            candidate.specificity = specificity;
            candidate.score =
                entity_score * 100 +
                candidate.intent_score * 10 +
                static_cast<int>(std::min<std::size_t>(
                    specificity, 9));
            entity_candidates.push_back(candidate);
        }
    }

    if (select_candidate(entity_candidates, false, selected)) {
        const KnowledgeResult result = make_result(
            *selected.item,
            selected.category,
            "entity_intent_question",
            selected.score);

        if (!result.answer.empty()) {
            return result;
        }
    } else if (!entity_candidates.empty()) {
        return fallback_result("fallback_ambiguous_entity_question");
    }

    // 5. 数量类问题没有可核验条目时直接兜底，
    // 避免用“研究方向”等无关条目冒充人数或台数。
    if (has_intent(query_features, "quantity_people") ||
        has_intent(query_features, "quantity_equipment"))
    {
        return fallback_result("fallback_quantity_unverified");
    }

    // 6. 成立/获批时间没有匹配到明确条目时兜底，
    // 避免“重点实验室成立时间”串到“学院成立时间”。
    if (has_intent(query_features, "establish") ||
        has_intent(query_features, "approval"))
    {
        return fallback_result("fallback_event_time_unverified");
    }

    // 7. FAQ 关键词匹配。
    std::vector<ScoredCandidate> keyword_candidates;

    for (const auto & faq : data_["faq"]) {
        int longest = 0;
        const int score =
            match_score(text, faq["keywords"], &longest);

        if (score <= 0) {
            continue;
        }

        const QueryFeatures candidate_features =
            question_features(faq);

        if (!compatible_intents(
                query_features, candidate_features))
        {
            continue;
        }

        ScoredCandidate candidate;
        candidate.item = &faq;
        candidate.category =
            faq.value("category", std::string("faq"));
        candidate.match_type = "faq_keyword";
        candidate.entity_score = entity_overlap_score(
            query_features, candidate_features);
        candidate.intent_score = intent_overlap_count(
            query_features, candidate_features);
        candidate.specificity =
            static_cast<std::size_t>(longest);
        candidate.score = score;
        keyword_candidates.push_back(candidate);
    }

    if (select_candidate(keyword_candidates, true, selected)) {
        const KnowledgeResult result = make_result(
            *selected.item,
            selected.category,
            "faq_keyword",
            selected.score);

        if (!result.answer.empty()) {
            return result;
        }
    } else if (!keyword_candidates.empty()) {
        return fallback_result("fallback_ambiguous_faq_keyword");
    }

    // 8. 研究方向、项目、设备条目匹配。
    const std::array<std::pair<const char *, const char *>, 3>
        entry_arrays = {{
            {"research_directions", "research_directions"},
            {"projects", "projects"},
            {"equipment", "equipment"}
        }};

    std::vector<ScoredCandidate> entry_candidates;

    for (const auto & spec : entry_arrays) {
        for (const auto & entry : data_[spec.first]) {
            int longest = 0;
            const int score =
                match_score(text, entry["keywords"], &longest);

            if (score <= 0) {
                continue;
            }

            const QueryFeatures candidate_features =
                keyword_features(entry["keywords"]);

            if (!compatible_intents(
                    query_features, candidate_features))
            {
                continue;
            }

            ScoredCandidate candidate;
            candidate.item = &entry;
            candidate.category = spec.second;
            candidate.match_type = "entry_keyword";
            candidate.entity_score = entity_overlap_score(
                query_features, candidate_features);
            candidate.intent_score = intent_overlap_count(
                query_features, candidate_features);
            candidate.specificity =
                static_cast<std::size_t>(longest);
            candidate.score = score;
            entry_candidates.push_back(candidate);
        }
    }

    if (select_candidate(entry_candidates, true, selected)) {
        const KnowledgeResult result = make_result(
            *selected.item,
            selected.category,
            "entry_keyword",
            selected.score);

        if (!result.answer.empty()) {
            return result;
        }
    } else if (!entry_candidates.empty()) {
        return fallback_result("fallback_ambiguous_entry_keyword");
    }

    // 9. 类别关键词匹配。
    std::vector<ScoredCandidate> category_candidates;

    for (const auto & category : data_["categories"]) {
        int longest = 0;
        const int score =
            match_score(text, category["keywords"], &longest);

        if (score <= 0) {
            continue;
        }

        const QueryFeatures candidate_features =
            keyword_features(category["keywords"]);

        if (!compatible_intents(
                query_features, candidate_features))
        {
            continue;
        }

        ScoredCandidate candidate;
        candidate.item = &category;
        candidate.category =
            category.value("id", std::string("category"));
        candidate.match_type = "category_keyword";
        candidate.entity_score = entity_overlap_score(
            query_features, candidate_features);
        candidate.intent_score = intent_overlap_count(
            query_features, candidate_features);
        candidate.specificity =
            static_cast<std::size_t>(longest);
        candidate.score = score;
        category_candidates.push_back(candidate);
    }

    if (select_candidate(category_candidates, true, selected)) {
        const KnowledgeResult result = make_result(
            *selected.item,
            selected.category,
            "category_keyword",
            selected.score);

        if (!result.answer.empty()) {
            return result;
        }
    } else if (!category_candidates.empty()) {
        return fallback_result("fallback_ambiguous_category_keyword");
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

KnowledgeResult KnowledgeBase::fallback_result(
    const std::string & match_type) const
{
    KnowledgeResult result;
    result.found = false;
    result.category = "fallback";
    result.match_type = match_type;
    result.match_score = 0;

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
