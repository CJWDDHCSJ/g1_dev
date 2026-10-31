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
