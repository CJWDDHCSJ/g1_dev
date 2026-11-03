#include "g1_voice_router/asr_text_normalizer.hpp"

#include <array>
#include <cstdio>
#include <cstdint>
#include <string>

namespace g1_voice_router
{
namespace
{

bool contains_any(
    const std::string & text,
    std::initializer_list<const char *> keywords)
{
    for (const auto * keyword : keywords) {
        if (keyword != nullptr && text.find(keyword) != std::string::npos) {
            return true;
        }
    }
    return false;
}

bool replace_all(
    std::string & text,
    const std::string & from,
    const std::string & to)
{
    bool replaced = false;
    std::size_t pos = 0;

    while ((pos = text.find(from, pos)) != std::string::npos) {
        text.replace(pos, from.size(), to);
        pos += to.size();
        replaced = true;
    }

    return replaced;
}

// 控制/否定词：出现时完全跳过归一化，防止把否定句改成命令句。
const char * kControlTokens[] = {
    "不要", "不用", "别", "停止", "取消", "不准", "不许", "别动",
    nullptr
};

// 「语数之一」只有在明确指向机器人/宇树产品时才纠正。
const char * kRobotContextTokens[] = {
    "机器人", "g1", "宇树", "unitree", "动作", "挥手", "演示",
    "多少钱", "价格", "报价", "型号", "参数",
    nullptr
};

}  // namespace

std::string AsrTextNormalizer::digest(const std::string & text)
{
    // FNV-1a 32bit：仅用于日志关联，不用于任何业务判断。
    std::uint32_t hash = 2166136261u;

    for (unsigned char c : text) {
        hash ^= static_cast<std::uint32_t>(c);
        hash *= 16777619u;
    }

    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%08x", hash);
    return std::string(buffer);
}

AsrNormalization AsrTextNormalizer::normalize(
    const std::string & raw_text) const
{
    AsrNormalization result;
    result.raw_text = raw_text;
    result.normalized_text = raw_text;

    if (raw_text.empty()) {
        result.reason = "EMPTY_TEXT";
        return result;
    }

    // 1. 控制词保护：否定、停止、取消一律不参与归一化。
    if (contains_any(raw_text, {
            kControlTokens[0], kControlTokens[1], kControlTokens[2],
            kControlTokens[3], kControlTokens[4], kControlTokens[5],
            kControlTokens[6], kControlTokens[7]}))
    {
        result.reason = "CONTROL_TOKEN_PROTECTED";
        return result;
    }

    std::string text = raw_text;
    std::string matched_rule;

    // 2. 脑急接口 / 脑筋接口 -> 脑机接口（真实 ASR 日志证据：
    //    P0-20.1 回归出现"脑急接口"，P0-21 真机演示出现"脑筋接口"）。
    if (replace_all(text, "脑急接口", "脑机接口") ||
        replace_all(text, "脑筋接口", "脑机接口"))
    {
        matched_rule = "ASR001_BRAIN_COMPUTER_INTERFACE";
    }

    // 3. 语数之一/语数G1 -> 宇树G1（仅限机器人语境）。
    if (text.find("语数之一") != std::string::npos ||
        text.find("语数g1") != std::string::npos ||
        text.find("语数G1") != std::string::npos)
    {
        if (contains_any(text, {
                kRobotContextTokens[0], kRobotContextTokens[1],
                kRobotContextTokens[2], kRobotContextTokens[3],
                kRobotContextTokens[4], kRobotContextTokens[5],
                kRobotContextTokens[6], kRobotContextTokens[7],
                kRobotContextTokens[8], kRobotContextTokens[9],
                kRobotContextTokens[10]}))
        {
            replace_all(text, "语数之一", "宇树G1");
            replace_all(text, "语数g1", "宇树G1");
            replace_all(text, "语数G1", "宇树G1");

            if (matched_rule.empty()) {
                matched_rule = "ASR002_UNITREE_G1";
            } else {
                matched_rule += "+ASR002_UNITREE_G1";
            }
        } else {
            result.reason = "UNITREE_ALIAS_WITHOUT_CONTEXT";
            // 不修改文本；继续保留其他规则的结果。
        }
    }

    if (text != raw_text) {
        result.normalized_text = text;
        result.changed = true;
        result.rule_id = matched_rule;
        result.reason.clear();
    } else if (result.reason.empty()) {
        result.reason = "NO_RULE_MATCHED";
    }

    return result;
}

}  // namespace g1_voice_router
