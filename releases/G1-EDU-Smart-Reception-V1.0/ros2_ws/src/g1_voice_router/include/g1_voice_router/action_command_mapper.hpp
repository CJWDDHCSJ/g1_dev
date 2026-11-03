#ifndef G1_VOICE_ROUTER_ACTION_COMMAND_MAPPER_HPP_
#define G1_VOICE_ROUTER_ACTION_COMMAND_MAPPER_HPP_

#include <string>

namespace g1_voice_router
{

// 动作命令匹配结果（P0-20）。
// action_id 只可能是固定白名单里的值，或 0（未匹配）。
struct ActionCommand
{
    bool matched{false};
    std::string action_name;   // 规范名，直接作为 /g1/action_request 负载
    int action_id{0};
    std::string match_type;    // EXACT / SYNONYM / NEGATED / QUESTION / DISABLED / UNKNOWN
    std::string reject_reason; // 未匹配时的原因
};

// 语音动作命令 → 固定白名单动作的确定性映射。
// 设计原则：
//  - 白名单写死在程序里，任何路径都不可能输出白名单外的 ID；
//  - hand_on_heart / ID 33 永久禁用；
//  - 否定、疑问、技术讨论一律不触发动作；
//  - 命令含糊时返回未匹配 + 原因，由上层给出澄清提示。
class ActionCommandMapper
{
public:
    // 白名单动作名与 ID（与 g1_action_executor 实际实现一致）。
    static constexpr int kWaveId = 26;
    static constexpr int kWaveLowId = 25;
    static constexpr int kHandsUpId = 15;
    static constexpr int kClapId = 17;
    static constexpr int kHeartId = 20;
    static constexpr int kVictoryId = 30;

    // 永久禁用（hand_on_heart 曾导致真机后退）。
    static constexpr int kForbiddenHandOnHeartId = 33;

    // 白名单校验：返回 ID；不在白名单时返回 0。
    static int whitelist_id(const std::string & action_name);

    // 对已归一化的用户文本做动作命令判定。
    ActionCommand map(const std::string & normalized_text) const;

    // 能力介绍文本（用于含糊命令时的澄清提示）。
    static const char * capability_hint();
};

}  // namespace g1_voice_router

#endif  // G1_VOICE_ROUTER_ACTION_COMMAND_MAPPER_HPP_
