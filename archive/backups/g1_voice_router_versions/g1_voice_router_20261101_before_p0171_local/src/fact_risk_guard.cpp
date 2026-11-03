#include "g1_voice_router/fact_risk_guard.hpp"

#include <array>
#include <cctype>
#include <cstddef>

namespace g1_voice_router
{
namespace
{

const char * kPriceFallback =
    "抱歉，目前没有经过核实的实时价格信息，"
    "我不能随意给你报价。";

const char * kLabFactFallback =
    "抱歉，这部分信息目前没有经过核实的公开资料，"
    "我暂时无法准确回答。";

const char * kRealtimeStatusFallback =
    "抱歉，我目前还没有接入可靠的自身状态读取功能，"
    "暂时无法确认我的实时运行状态。";

// 与 IntentRecognizer 的 normalize 类似，但只对 ASCII 字母做小写转换，
// 不触碰 UTF-8 多字节序列，避免 locale 相关的字节破坏。
std::string normalize_text(std::string text)
{
    static const char * punctuation[] = {
        "。", "！", "？", "，", "、", "；", "：", "～",
        ".", ",", "!", "?", ";", ":",
        "\"", "'", "“", "”", "（", "）",
        "(", ")", " ", "\t", "\r", "\n"
    };

    for (const auto * mark : punctuation) {
        std::size_t pos = 0;
        while ((pos = text.find(mark, pos)) != std::string::npos) {
            text.erase(pos, std::string(mark).size());
        }
    }

    for (char & c : text) {
        const unsigned char byte = static_cast<unsigned char>(c);
        if (byte >= 'A' && byte <= 'Z') {
            c = static_cast<char>(byte - 'A' + 'a');
        }
    }

    return text;
}

bool contains_any(
    const std::string & text,
    const std::array<const char *, 20> & patterns)
{
    for (const auto * pattern : patterns) {
        if (pattern == nullptr) {
            continue;
        }

        if (text.find(pattern) != std::string::npos) {
            return true;
        }
    }

    return false;
}

// 机器人主体语境词，用于把“G1 当前温度”这类问题
// 和“今天天气温度”这类普通问题区分开。
const std::array<const char *, 20> kRobotContextTokens = {
    "机器人", "g1", "宇树", "unitree", "关节", "自身", "本体",
    nullptr
};

const std::array<const char *, 20> kPriceKeywords = {
    "多少钱", "价格", "报价", "售价", "价位", "卖多少钱",
    "贵不贵", "多少元", "多少块", "多少万", "打折", "优惠价",
    nullptr
};

// 无需机器人语境即可判定为实时状态的传感器词。
const std::array<const char *, 20> kSensorKeywords = {
    "关节温度", "电量", "电池", "电压", "电流", "转速",
    "剩余多少电", "续航", "摔了没有", "摔倒没有",
    nullptr
};

// 需要机器人语境配合的泛化状态词。
const std::array<const char *, 20> kStatusKeywords = {
    "正常吗", "正常么", "还好吗", "运行状态", "运行正常",
    "状态怎么样", "状态如何", "状态好吗", "开机了吗",
    "站得稳吗", "体温", "温度",
    nullptr
};

// 实验室实体词。
// 注意：不能放“脑机”这种宽泛前缀，
// 否则“脑机接口是什么”会被误判为实验室事实。
const std::array<const char *, 20> kLabEntityTokens = {
    "实验室", "研究中心", "脑机中心", "脑机交互",
    "重点实验室", "学院", "两江人工智能", "开放平台",
    "重庆理工", "你们学校", "你们实验室", "贵校",
    nullptr
};

FactRisk classify(const std::string & text, std::string & reason)
{
    // 1. 价格：只要出现价格类词，一律高风险。
    //    当前没有实时价格来源，无论是否提到具体产品都必须拦截。
    if (contains_any(text, kPriceKeywords)) {
        reason = "matched price keyword";
        return FactRisk::PRICE;
    }

    // 2. 机器人实时状态。
    //    传感器词无条件命中；泛化状态词需要机器人语境。
    if (contains_any(text, kSensorKeywords)) {
        reason = "matched robot sensor keyword";
        return FactRisk::REALTIME_STATUS;
    }

    if (contains_any(text, kStatusKeywords) &&
        contains_any(text, kRobotContextTokens))
    {
        reason = "matched robot status keyword with robot context";
        return FactRisk::REALTIME_STATUS;
    }

    // 3. 实验室具体事实。
    //    只认实体词，不认“具身智能”“脑机接口”这类通用技术词。
    if (contains_any(text, kLabEntityTokens)) {
        reason = "matched lab entity token";
        return FactRisk::LAB_FACT;
    }

    reason = "no high-risk pattern; general knowledge";
    return FactRisk::NORMAL;
}

bool is_kb_fallback_match_type(const std::string & match_type)
{
    return match_type.rfind("fallback", 0) == 0;
}

}  // namespace

const char * to_string(FactRisk risk)
{
    switch (risk) {
        case FactRisk::NORMAL:
            return "NORMAL";
        case FactRisk::LAB_FACT:
            return "LAB_FACT";
        case FactRisk::PRICE:
            return "PRICE";
        case FactRisk::REALTIME_STATUS:
            return "REALTIME_STATUS";
        default:
            return "UNKNOWN";
    }
}

FactRiskResult FactRiskGuard::evaluate(
    const std::string & user_text,
    const KnowledgeBase * knowledge_base) const
{
    FactRiskResult result;

    const std::string text = normalize_text(user_text);

    if (text.empty()) {
        result.risk = FactRisk::NORMAL;
        result.reason = "empty text after normalization";
        return result;
    }

    result.risk = classify(text, result.reason);

    switch (result.risk) {
        case FactRisk::NORMAL:
            // 普通通用知识直接放行，不查询知识库。
            result.blocked = false;
            result.use_local_answer = false;
            break;

        case FactRisk::PRICE:
            result.blocked = true;
            result.fallback_answer = kPriceFallback;
            break;

        case FactRisk::REALTIME_STATUS:
            result.blocked = true;
            result.fallback_answer = kRealtimeStatusFallback;
            break;

        case FactRisk::LAB_FACT:
            // 实验室事实：优先查现有知识库。
            if (knowledge_base == nullptr || !knowledge_base->loaded()) {
                result.blocked = true;
                result.reason += "; knowledge base unavailable";
                result.fallback_answer = kLabFactFallback;
                break;
            }

            {
                const KnowledgeResult kb = knowledge_base->query(user_text);

                result.kb_match_type = kb.match_type;
                result.kb_matched_id = kb.matched_id;
                result.kb_match_score = kb.match_score;

                // 只有 found == true（含 clarification）才是可靠本地答案；
                // match_type 以 fallback 开头的一律视为无资料。
                if (kb.found &&
                    kb.answer.size() > 0 &&
                    !is_kb_fallback_match_type(kb.match_type))
                {
                    result.use_local_answer = true;
                    result.local_answer = kb.answer;
                } else {
                    result.blocked = true;
                    result.reason += "; knowledge base has no verified answer";
                    result.fallback_answer = kLabFactFallback;
                }
            }
            break;
    }

    return result;
}

}  // namespace g1_voice_router
