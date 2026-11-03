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
// P0-17.1：补充“你/您”——“你现在正常吗”这类问法
// 以“你”指代机器人，同样属于机器人语境。
const std::array<const char *, 20> kRobotContextTokens = {
    "机器人", "g1", "宇树", "unitree", "关节", "自身", "本体",
    "你", "您",
    nullptr
};

const std::array<const char *, 20> kPriceKeywords = {
    "多少钱", "价格", "报价", "售价", "价位", "卖多少钱",
    "贵不贵", "多少元", "多少块", "多少万", "打折", "优惠价",
    nullptr
};

// P0-17.1：知识型问题标志。
// “IMU 是什么”“如何使用 IMU 估计姿态”“动作规划是什么”
// 问的是技术概念或原理，不是 G1 的当前实时状态；
// 命中这些形式时先按普通知识放行，再走后续规则。
// 注意“机器人目前是什么状态”含“什么状态”，必须豁免，
// 它是状态询问而不是知识提问。
const std::array<const char *, 20> kKnowledgeMarkers = {
    "是什么", "什么是", "什么意思", "如何", "怎么用", "怎么实现",
    "怎么估计", "怎么判断", "怎么计算", "为什么",
    "原理", "的定义", "指什么", "属于什么",
    nullptr
};

// 无需机器人语境即可判定为实时状态的传感器词。
// P0-17.1：补充 IMU/惯导/陀螺仪/姿态数据——
// “你的 IMU 数据正常吗”“惯导数据是否正常”都是
// 询问 G1 自身传感器当前状态。
const std::array<const char *, 20> kSensorKeywords = {
    "关节温度", "电量", "电池", "电压", "电流", "转速",
    "剩余多少电", "续航", "摔了没有", "摔倒没有",
    "imu", "惯导", "陀螺仪", "姿态数据",
    nullptr
};

// 需要机器人语境配合的泛化状态词。
// P0-17.1：补充“什么状态”——“机器人目前是什么状态”。
const std::array<const char *, 20> kStatusKeywords = {
    "正常吗", "正常么", "还好吗", "运行状态", "运行正常",
    "状态怎么样", "状态如何", "状态好吗", "开机了吗",
    "站得稳吗", "体温", "温度", "什么状态",
    nullptr
};

// P0-17.1：动作执行状态询问标志。
// “你现在正在执行什么动作”“动作做完了吗”询问的是
// G1 动作执行的实时进展，不允许交给大模型编造。
const std::array<const char *, 20> kActionStatusMarkers = {
    "做完了吗", "完成了吗", "执行完", "正在执行", "在做什么",
    "执行的是什么", "执行什么", "什么动作",
    nullptr
};

// 动作状态标志必须与动作/执行语境词同时出现，
// 防止把普通技术问题或无关语句误判为实时状态。
const std::array<const char *, 20> kActionContextTokens = {
    "动作", "执行", "挥手", "比心", "举手", "拍手",
    "机器人", "手臂", "腿", "你", "您", "g1", "宇树",
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

// 状态询问形式豁免词：“机器人目前是什么状态”含“是什么”，
// 但它是状态询问而不是知识提问，不能被知识形式规则放行。
const std::array<const char *, 20> kStatusInquiryException = {
    "什么状态",
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

    // 2. 动作执行状态询问（P0-17.1）。
    //    必须在知识形式判断之前：“你当前执行的是什么动作”
    //    含“是什么”，但不能被当成知识问题放行。
    //    标志词与动作语境词同时出现才命中。
    if (contains_any(text, kActionStatusMarkers) &&
        contains_any(text, kActionContextTokens))
    {
        reason = "matched action execution status pattern";
        return FactRisk::REALTIME_STATUS;
    }

    // 3. 知识型问题形式（P0-17.1）。
    //    “IMU 是什么”“如何使用 IMU 估计姿态”“动作规划是什么”
    //    问的是概念或原理，不是 G1 当前实时状态，保持 NORMAL。
    //    两个豁免：
    //    a) 含实验室实体词的继续走 LAB_FACT 分支
    //       （如“实验室设备是什么型号”仍是实验室事实）；
    //    b) 含“什么状态”的是状态询问（“机器人目前是什么状态”）。
    const bool lab_entity =
        contains_any(text, kLabEntityTokens);

    if (!lab_entity &&
        !contains_any(text, kStatusInquiryException) &&
        contains_any(text, kKnowledgeMarkers))
    {
        reason = "knowledge question form; not a realtime status query";
        return FactRisk::NORMAL;
    }

    // 4. 机器人实时状态：传感器词。
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

    // 5. 实验室具体事实。
    //    只认实体词，不认“具身智能”“脑机接口”这类通用技术词。
    if (lab_entity) {
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
