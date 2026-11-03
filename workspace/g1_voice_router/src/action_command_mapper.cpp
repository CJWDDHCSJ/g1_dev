#include "g1_voice_router/action_command_mapper.hpp"

#include <array>
#include <cstddef>

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

// 否定/停止表达：出现即不执行任何动作。
const char * kNegationTokens[] = {
    "不要", "不用", "不需要", "别", "停止", "取消", "不准", "不许",
    "别动", "不用动", "先不要", "暂时不要",
    nullptr
};

// 疑问/技术讨论表达：询问能力或原理，不执行动作。
const char * kQuestionTokens[] = {
    "会不会", "能不能", "可不可以", "可以吗", "行不行",
    "如何", "怎么", "为什么", "原理", "是什么", "什么是",
    "怎么实现", "怎么做到", "有没有动作", "能做哪些", "有哪些动作",
    "你都会", "都会哪些", "哪些动作", "会做", "能做", "你会做什么",
    nullptr
};

// 永久禁用的表达（hand_on_heart / ID 33）。
const char * kForbiddenTokens[] = {
    "hand_on_heart", "handonheart", "把手放在心口", "把手放胸口", "手贴心口",
    "贴胸", "摸心口",
    nullptr
};

struct ActionPhrase
{
    const char * action_name;
    int action_id;
    const char * phrase;
};

// 白名单动作的确定性短语表。
// 顺序无关，但同一句命中多个时取最长匹配（见 map()）。
const ActionPhrase kPhraseTable[] = {
    {"wave", 26, "挥挥手"},
    {"wave", 26, "挥一挥手"},
    {"wave", 26, "挥手"},
    {"wave", 26, "挥个手"},
    {"wave", 26, "打个招呼"},
    {"wave_low", 25, "低位挥手"},
    {"wave_low", 25, "轻轻挥手"},
    {"wave_low", 25, "小幅度挥手"},
    {"hands_up", 15, "举起双手"},
    {"hands_up", 15, "举双手"},
    {"hands_up", 15, "双手举起来"},
    {"hands_up", 15, "举手"},
    {"hands_up", 15, "把手举起来"},
    {"clap", 17, "拍拍手"},
    {"clap", 17, "拍个手"},
    {"clap", 17, "鼓掌"},
    {"clap", 17, "拍手"},
    {"heart", 20, "比个心"},
    {"heart", 20, "比心"},
    {"heart", 20, "爱心手势"},
    {"heart", 20, "爱心动作"},
    {"victory", 30, "比个耶"},
    {"victory", 30, "比耶"},
    {"victory", 30, "胜利手势"},
    {"victory", 30, "胜利姿势"},
    {"victory", 30, "剪刀手"},
};

}  // namespace

int ActionCommandMapper::whitelist_id(const std::string & action_name)
{
    if (action_name == "wave") {
        return kWaveId;
    }
    if (action_name == "wave_low") {
        return kWaveLowId;
    }
    if (action_name == "hands_up") {
        return kHandsUpId;
    }
    if (action_name == "clap") {
        return kClapId;
    }
    if (action_name == "heart") {
        return kHeartId;
    }
    if (action_name == "victory") {
        return kVictoryId;
    }
    return 0;
}

const char * ActionCommandMapper::capability_hint()
{
    return "你想让我做哪个动作呢？我可以挥手、轻轻挥手、举手、拍手、比心或者比耶。";
}

ActionCommand ActionCommandMapper::map(
    const std::string & normalized_text) const
{
    ActionCommand command;

    if (normalized_text.empty()) {
        command.match_type = "UNKNOWN";
        command.reject_reason = "EMPTY_TEXT";
        return command;
    }

    // 1. 永久禁用表达：任何情况下都不执行。
    if (contains_any(normalized_text, {
            kForbiddenTokens[0], kForbiddenTokens[1], kForbiddenTokens[2],
            kForbiddenTokens[6], kForbiddenTokens[7],
            kForbiddenTokens[3], kForbiddenTokens[4], kForbiddenTokens[5]}))
    {
        command.match_type = "DISABLED";
        command.reject_reason = "FORBIDDEN_ACTION";
        return command;
    }

    // 2. 否定/停止表达：不执行动作（也不假装已经停止正在播放/执行的内容）。
    if (contains_any(normalized_text, {
            kNegationTokens[0], kNegationTokens[1], kNegationTokens[2],
            kNegationTokens[3], kNegationTokens[4], kNegationTokens[5],
            kNegationTokens[6], kNegationTokens[7], kNegationTokens[8],
            kNegationTokens[9], kNegationTokens[10], kNegationTokens[11]}))
    {
        command.match_type = "NEGATED";
        command.reject_reason = "NEGATED";
        return command;
    }

    // 3. 疑问/技术讨论：问能力或原理，不执行动作。
    if (contains_any(normalized_text, {
            kQuestionTokens[0], kQuestionTokens[1], kQuestionTokens[2],
            kQuestionTokens[3], kQuestionTokens[4], kQuestionTokens[5],
            kQuestionTokens[6], kQuestionTokens[7], kQuestionTokens[8],
            kQuestionTokens[9], kQuestionTokens[10], kQuestionTokens[11],
            kQuestionTokens[12], kQuestionTokens[13], kQuestionTokens[14],
            kQuestionTokens[15], kQuestionTokens[16]}))
    {
        command.match_type = "QUESTION";
        command.reject_reason = "QUESTION_FORM";
        return command;
    }

    // 4. 确定性短语匹配：取最长命中，保证“低位挥手”不会被“挥手”抢走。
    const ActionPhrase * best = nullptr;

    for (const auto & entry : kPhraseTable) {
        if (entry.action_name == nullptr || entry.phrase == nullptr) {
            continue;
        }

        if (normalized_text.find(entry.phrase) == std::string::npos) {
            continue;
        }

        if (best == nullptr ||
            std::string(entry.phrase).size() > std::string(best->phrase).size())
        {
            best = &entry;
        }
    }

    if (best == nullptr) {
        command.match_type = "UNKNOWN";
        command.reject_reason = "NO_WHITELISTED_ACTION";
        return command;
    }

    // 白名单二次校验：即使短语表被误改，也不会输出白名单外 ID。
    const int confirmed_id = whitelist_id(best->action_name);

    if (confirmed_id == 0 || confirmed_id == kForbiddenHandOnHeartId) {
        command.match_type = "DISABLED";
        command.reject_reason = "NOT_IN_WHITELIST";
        return command;
    }

    command.matched = true;
    command.action_name = best->action_name;
    command.action_id = confirmed_id;
    command.match_type = "EXACT";
    return command;
}

}  // namespace g1_voice_router
