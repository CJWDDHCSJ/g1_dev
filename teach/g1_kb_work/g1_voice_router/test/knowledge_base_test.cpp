#include "g1_voice_router/knowledge_base.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>
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
    check("FAQ count is 21", knowledge.faq_count() == 21);
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
        result.matched_id == "faq_intro" &&
        result.match_type == "contained_question" &&
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
        result.matched_id == "research_directions" &&
        result.match_type == "category_keyword" &&
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
        result.matched_id == "faq_equipment" &&
        result.category == "equipment" &&
        result.match_type == "exact_question" &&
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
        result.match_type == "fallback_quantity_unverified" &&
        !result.answer.empty(),
        result.match_type + " / " + result.answer);

    result = knowledge.query("今天天气怎么样");
    check(
        "no-match question falls back",
        !result.found &&
        result.category == "fallback" &&
        result.match_type == "fallback" &&
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

    // =========================================================
    // 第一轮 P0 修复新增用例
    // =========================================================

    // P0-1 A：重点实验室成立时间必须与获批、学院成立、
    // 脑机中心成立区分；只有获批数据时如实说明获批。
    result = knowledge.query("重点实验室什么时候成立");
    check(
        "key lab establishment is not approval/college/bci date",
        result.found &&
        result.matched_id == "faq_embodied_established" &&
        result.match_type == "exact_question" &&
        has_text(result, "2025年7月获批") &&
        has_text(result, "不能把获批时间当作成立时间") &&
        result.answer.find("2018年6月9日") == std::string::npos &&
        result.answer.find("2025年12月30日") == std::string::npos,
        result.answer);

    result = knowledge.query("重点实验室什么时候获批");
    check(
        "key lab approval keeps approval answer",
        result.found &&
        result.matched_id == "faq_embodied_approval" &&
        result.match_type == "exact_question" &&
        has_text(result, "2025年7月"),
        result.answer);

    // P0-1 B：开放平台是什么 → 平台介绍，而不是 faq_embodied_name。
    result = knowledge.query("开放平台是什么");
    check(
        "open platform question hits platform FAQ",
        result.found &&
        result.matched_id == "faq_platform" &&
        result.match_type == "entity_intent_question" &&
        has_text(result, "高鲁棒感知") &&
        result.answer.find("没有只叫具身智能实验室") ==
            std::string::npos,
        result.answer);

    // P0-1 C：脑机中心人数无公开资料 → 兜底，不答研究方向。
    result = knowledge.query("脑机中心有多少人");
    check(
        "bci headcount falls back without directions",
        !result.found &&
        result.category == "fallback" &&
        result.matched_id.empty() &&
        result.match_type == "fallback_quantity_unverified" &&
        result.answer.find("神经信号感知") == std::string::npos &&
        result.answer.find("多模态融合") == std::string::npos,
        result.answer);

    // P0-1 E：研究方向+设备复合问题 → 澄清，不随机选一类。
    result = knowledge.query("实验室主要研究什么设备");
    check(
        "research plus equipment composite clarifies",
        result.found &&
        result.category == "clarification" &&
        result.match_type == "clarification" &&
        result.matched_id.empty() &&
        result.answer.find("研究方向") != std::string::npos &&
        result.answer.find("设备") != std::string::npos,
        result.answer);

    // 不存在资料的问题 → 显式兜底。
    result = knowledge.query("实验室的食堂几点开门");
    check(
        "unknown question falls back",
        !result.found &&
        result.match_type == "fallback" &&
        !result.answer.empty(),
        result.answer);

    // 诊断：FAQ 关键词命中记录 matched_id 与原因。
    result = knowledge.query("脑机中心方向");
    check(
        "faq keyword hit logs match reason",
        result.found &&
        result.matched_id == "faq_bci_directions" &&
        result.match_type == "faq_keyword" &&
        result.match_score > 0,
        result.match_type + " / " + result.matched_id);

    // =========================================================
    // FAQ 数组顺序交换后，命中结果必须完全一致
    // =========================================================

    nlohmann::json doc;

    {
        std::ifstream input(knowledge_path);
        doc = nlohmann::json::parse(input);
    }

    check(
        "order-swap fixture parsed",
        doc.contains("faq") && doc["faq"].is_array());

    nlohmann::json reversed_faq(doc["faq"]);
    std::reverse(reversed_faq.begin(), reversed_faq.end());
    doc["faq"] = reversed_faq;

    const std::string reversed_path = temp_path("_reversed.json");
    check(
        "order-swap fixture written",
        write_file(reversed_path, doc.dump(2)));

    KnowledgeBase reversed_kb;
    check(
        "reversed JSON load",
        reversed_kb.load(reversed_path, error),
        error);
    check("reversed FAQ count is 21", reversed_kb.faq_count() == 21);

    struct OrderCase
    {
        const char * query;
        std::string expected_id;
    };

    const std::vector<OrderCase> order_cases = {
        {"开放平台是什么", "faq_platform"},
        {"重点实验室什么时候成立", "faq_embodied_established"},
        {"重点实验室什么时候获批", "faq_embodied_approval"},
        {"脑机中心有多少人", ""},
        {"实验室有哪些设备", "faq_equipment"},
        {"实验室主要研究什么设备", ""},
        {"刘宏是谁", "faq_liu"},
        {"学院有哪些专业", ""},
        {"你们实验室主要研究什么", "research_directions"},
        {"介绍一下实验室", "faq_intro"}
    };

    bool order_independent = true;
    std::string order_detail;

    for (const auto & order_case : order_cases) {
        const auto baseline = knowledge.query(order_case.query);
        const auto shuffled = reversed_kb.query(order_case.query);

        if (baseline.matched_id != shuffled.matched_id ||
            baseline.answer != shuffled.answer ||
            baseline.match_type != shuffled.match_type ||
            baseline.matched_id != order_case.expected_id)
        {
            order_independent = false;
            order_detail = order_case.query;
            break;
        }
    }

    check(
        "FAQ order swap does not change results",
        order_independent,
        order_detail);

    // =========================================================
    // FAQ 关键词重叠：静态检查 + 共享关键词查询顺序无关
    // =========================================================

    std::map<std::string, std::set<std::string>> keyword_ids;

    for (const auto & faq : doc["faq"]) {
        const std::string id = faq.value("id", std::string{});

        for (const auto & keyword : faq["keywords"]) {
            if (keyword.is_string()) {
                keyword_ids[keyword.get<std::string>()].insert(id);
            }
        }
    }

    bool shared_keywords_stable = true;
    std::string shared_detail;
    std::size_t shared_keyword_count = 0;

    for (const auto & entry : keyword_ids) {
        if (entry.second.size() < 2) {
            continue;
        }

        ++shared_keyword_count;

        const auto baseline = knowledge.query(entry.first);
        const auto shuffled = reversed_kb.query(entry.first);

        if (baseline.matched_id != shuffled.matched_id ||
            baseline.answer != shuffled.answer ||
            baseline.match_type != shuffled.match_type)
        {
            shared_keywords_stable = false;
            shared_detail = entry.first;
            break;
        }
    }

    check(
        "shared FAQ keywords exist for conflict check",
        shared_keyword_count > 0,
        std::to_string(shared_keyword_count));

    check(
        "shared FAQ keywords resolve order-independently",
        shared_keywords_stable,
        shared_detail);

    // =========================================================
    // 两条 FAQ 关键词得分相同 → 兜底，不取数组第一项
    // =========================================================

    const std::string tie_json = R"JSON({
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
      "faq": [
        {
          "id": "faq_tie_a",
          "questions": ["甲问题"],
          "keywords": ["共同关键词"],
          "answer": "答案甲的内容。",
          "source_ids": ["S99"]
        },
        {
          "id": "faq_tie_b",
          "questions": ["乙问题"],
          "keywords": ["共同关键词"],
          "answer": "答案乙的内容。",
          "source_ids": ["S99"]
        }
      ],
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

    const std::string tie_path = temp_path("_tie.json");
    check("tie fixture written", write_file(tie_path, tie_json));

    KnowledgeBase tie_kb;
    check("tie fixture load", tie_kb.load(tie_path, error), error);

    result = tie_kb.query("共同关键词");
    check(
        "equal-score FAQ tie falls back instead of first item",
        !result.found &&
        result.category == "fallback" &&
        result.match_type == "fallback_ambiguous_faq_keyword" &&
        result.answer == "测试兜底回答。",
        result.match_type + " / " + result.answer);

    KnowledgeBase tie_kb_reversed;
    nlohmann::json tie_doc = nlohmann::json::parse(tie_json);
    std::reverse(tie_doc["faq"].begin(), tie_doc["faq"].end());

    const std::string tie_reversed_path =
        temp_path("_tie_reversed.json");
    check(
        "tie reversed fixture written",
        write_file(tie_reversed_path, tie_doc.dump(2)));

    check(
        "tie reversed load",
        tie_kb_reversed.load(tie_reversed_path, error),
        error);

    const auto tie_reversed_result =
        tie_kb_reversed.query("共同关键词");
    check(
        "equal-score FAQ tie is order-independent",
        !tie_reversed_result.found &&
        tie_reversed_result.match_type ==
            "fallback_ambiguous_faq_keyword" &&
        tie_reversed_result.answer == result.answer,
        tie_reversed_result.answer);

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
    std::remove(reversed_path.c_str());
    std::remove(tie_path.c_str());
    std::remove(tie_reversed_path.c_str());

    std::cout << "=========================================\n";
    std::cout << "Passed: " << passed << '\n';
    std::cout << "Failed: " << failed << '\n';

    return failed == 0 ? 0 : 1;
}
