#include "g1_voice_router/asr_text_normalizer.hpp"

#include <iostream>
#include <string>

using g1_voice_router::AsrNormalization;
using g1_voice_router::AsrTextNormalizer;

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

void expect_changed(
    const AsrTextNormalizer & normalizer,
    const std::string & input,
    const std::string & expected,
    const std::string & rule_prefix)
{
    const auto result = normalizer.normalize(input);

    check(
        "[ASR+] " + input + " -> " + expected,
        result.changed &&
        result.normalized_text == expected &&
        result.rule_id.rfind(rule_prefix, 0) == 0 &&
        result.raw_text == input,
        "changed=" + std::string(result.changed ? "1" : "0") +
        " out=" + result.normalized_text +
        " rule=" + result.rule_id);
}

void expect_unchanged(
    const AsrTextNormalizer & normalizer,
    const std::string & input)
{
    const auto result = normalizer.normalize(input);

    check(
        "[ASR=] 不修改: " + input,
        !result.changed &&
        result.normalized_text == input &&
        result.raw_text == input,
        "changed=" + std::string(result.changed ? "1" : "0") +
        " out=" + result.normalized_text +
        " reason=" + result.reason);
}

}  // namespace

int main()
{
    const AsrTextNormalizer normalizer;

    // 1. 已确认的 ASR 混淆：脑急接口 -> 脑机接口
    expect_changed(
        normalizer, "脑急接口是什么", "脑机接口是什么", "ASR001");

    // 1b. 另一种真实 ASR 变体：脑筋接口 -> 脑机接口
    expect_changed(
        normalizer, "脑筋接口是什么", "脑机接口是什么", "ASR001");

    // 2. 正确术语不修改
    expect_unchanged(normalizer, "脑机接口是什么");

    // 3. 语数之一：机器人语境下修正
    expect_changed(
        normalizer, "语数之一多少钱", "宇树G1多少钱", "ASR002");
    expect_changed(
        normalizer, "语数之一的动作演示", "宇树G1的动作演示", "ASR002");

    // 4. 语数之一：无机器人语境时不修改
    expect_unchanged(normalizer, "语数之一这个说法是什么意思");
    expect_unchanged(normalizer, "今天天气不错");

    // 5. 控制/否定词保护（绝不改成命令）
    expect_unchanged(normalizer, "不要挥手");
    expect_unchanged(normalizer, "别举手");
    expect_unchanged(normalizer, "停止动作");
    expect_unchanged(normalizer, "取消");
    expect_unchanged(normalizer, "不要脑急接口的解释");

    // 6. 数字与状态数值不被修改
    expect_unchanged(normalizer, "你现在还有多少电量");
    expect_unchanged(normalizer, "你的关节温度是多少度");
    expect_unchanged(normalizer, "电池百分之九十三");

    // 7. 空文本与纯标点
    expect_unchanged(normalizer, "");
    expect_unchanged(normalizer, "？？？");

    // 8. 原始文本始终保留
    {
        const auto result = normalizer.normalize("脑急接口是什么");
        check(
            "[ASR] raw_text 保留",
            result.raw_text == "脑急接口是什么",
            "raw=" + result.raw_text);
    }

    // 9. 摘要稳定且不含原文
    {
        const std::string d1 = AsrTextNormalizer::digest("脑急接口是什么");
        const std::string d2 = AsrTextNormalizer::digest("脑急接口是什么");
        const std::string d3 = AsrTextNormalizer::digest("脑机接口是什么");

        check("[ASR] digest 稳定", d1 == d2, d1 + " vs " + d2);
        check("[ASR] digest 区分不同文本", d1 != d3, d1 + " vs " + d3);
        check("[ASR] digest 长度 8", d1.size() == 8, d1);
    }

    // 10. 同时命中两条规则
    {
        const auto result = normalizer.normalize("脑急接口和语数之一多少钱");
        check(
            "[ASR+] 多规则同时命中",
            result.changed &&
            result.normalized_text == "脑机接口和宇树G1多少钱",
            "out=" + result.normalized_text);
    }

    std::cout << "\nTotal: " << (passed + failed)
              << " | passed: " << passed
              << " | failed: " << failed << '\n';

    return failed > 0 ? 1 : 0;
}
