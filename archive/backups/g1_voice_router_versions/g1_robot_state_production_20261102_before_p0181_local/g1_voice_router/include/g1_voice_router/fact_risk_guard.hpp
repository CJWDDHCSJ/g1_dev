#ifndef G1_VOICE_ROUTER_FACT_RISK_GUARD_HPP_
#define G1_VOICE_ROUTER_FACT_RISK_GUARD_HPP_

#include <string>

#include "g1_voice_router/knowledge_base.hpp"

namespace g1_voice_router
{

// 事实风险等级。
// NORMAL          普通通用知识，允许进入 Ollama。
// PRICE           产品价格类问题，无可靠实时来源，必须拦截。
// REALTIME_STATUS 机器人实时状态问题，当前无状态读取业务，必须拦截。
// LAB_FACT        实验室具体事实问题，必须先查本地知识库，
//                 有可靠匹配才使用本地答案，否则兜底，不进 Ollama。
enum class FactRisk
{
    NORMAL,
    LAB_FACT,
    PRICE,
    REALTIME_STATUS
};

const char * to_string(FactRisk risk);

struct FactRiskResult
{
    FactRisk risk{FactRisk::NORMAL};

    // true 表示不允许进入 Ollama 队列。
    bool blocked{false};

    // true 表示已有本地知识库可靠答案，直接播报 result 中的答案。
    bool use_local_answer{false};

    // 拦截/放行原因，用于日志。
    std::string reason;

    // blocked 时的安全兜底回答。
    std::string fallback_answer;

    // use_local_answer 时本地知识库命中的答案。
    std::string local_answer;

    // 知识库诊断信息（仅在查询过知识库时有效）。
    std::string kb_match_type;
    std::string kb_matched_id;
    int kb_match_score{0};
};

// 大模型事实风险保护层。
// 在 GENERAL_QA 等文本进入 Ollama 队列之前调用：
// - 普通通用知识直接放行（不查询知识库，零额外开销）。
// - 价格 / 实时状态类问题直接拦截，不调用大模型。
// - 实验室事实类问题优先查询现有 KnowledgeBase：
//   有可靠匹配（found == true）时使用本地答案；
//   无可靠匹配（兜底结果）时明确兜底，绝不交给大模型猜测。
class FactRiskGuard
{
public:
    // knowledge_base 可为 nullptr（例如知识库加载失败的降级场景），
    // 此时实验室事实类问题一律安全兜底。
    FactRiskResult evaluate(
        const std::string & user_text,
        const KnowledgeBase * knowledge_base) const;
};

}  // namespace g1_voice_router

#endif  // G1_VOICE_ROUTER_FACT_RISK_GUARD_HPP_
