#include "g1_voice_router/action_command_mapper.hpp"
#include "g1_voice_router/interaction_manager.hpp"

#include <iostream>
#include <string>

using g1_voice_router::ActionCommand;
using g1_voice_router::ActionCommandMapper;

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

ActionCommand map_text(const ActionCommandMapper & mapper, const std::string & text)
{
    return mapper.map(g1_voice_router::normalize_text(text));
}

// 六个白名单动作的多种自然语言表达。
void test_whitelist_phrases()
{
    const ActionCommandMapper mapper;

    struct Case
    {
        const char * text;
        const char * name;
        int id;
    };

    const Case cases[] = {
        {"挥个手", "wave", 26},
        {"你挥挥手", "wave", 26},
        {"挥一挥手", "wave", 26},
        {"轻轻挥手", "wave_low", 25},
        {"低位挥手", "wave_low", 25},
        {"举起双手", "hands_up", 15},
        {"举手", "hands_up", 15},
        {"拍拍手", "clap", 17},
        {"拍个手", "clap", 17},
        {"鼓掌", "clap", 17},
        {"比个心", "heart", 20},
        {"比心", "heart", 20},
        {"给我比个耶", "victory", 30},
        {"胜利手势", "victory", 30},
        {"剪刀手", "victory", 30},
    };

    for (const auto & item : cases) {
        const auto result = map_text(mapper, item.text);

        check(
            std::string("[MAP+] ") + item.text,
            result.matched &&
            result.action_name == item.name &&
            result.action_id == item.id,
            "name=" + result.action_name +
            " id=" + std::to_string(result.action_id) +
            " type=" + result.match_type);
    }
}

// 否定 / 疑问 / 技术讨论 / 禁用：一律不执行动作。
void test_rejections()
{
    const ActionCommandMapper mapper;

    const char * negated[] = {
        "不要挥手",
        "别动",
        "停止挥手",
        "取消动作",
        "不用比心",
    };

    for (const auto * text : negated) {
        const auto result = map_text(mapper, text);
        check(
            std::string("[MAP-] 否定不执行: ") + text,
            !result.matched && result.match_type == "NEGATED",
            "type=" + result.match_type);
    }

    const char * questions[] = {
        "挥手这个动作怎么实现",
        "机器人为什么能挥手",
        "你会不会挥手",
        "你能做哪些动作",
        "有哪些动作",
        "挥手的原理是什么",
    };

    for (const auto * text : questions) {
        const auto result = map_text(mapper, text);
        check(
            std::string("[MAP-] 疑问不执行: ") + text,
            !result.matched && result.match_type == "QUESTION",
            "type=" + result.match_type);
    }

    const char * forbidden[] = {
        "把手放在心口",
        "hand_on_heart",
    };

    for (const auto * text : forbidden) {
        const auto result = map_text(mapper, text);
        check(
            std::string("[MAP-] 禁用动作: ") + text,
            !result.matched && result.match_type == "DISABLED",
            "type=" + result.match_type);
    }

    const auto unknown = map_text(mapper, "跳个舞");
    check(
        "[MAP-] 未知动作不匹配",
        !unknown.matched && unknown.match_type == "UNKNOWN",
        "type=" + unknown.match_type);

    const auto generic = map_text(mapper, "给我们表演一个动作");
    check(
        "[MAP-] 含糊命令不猜动作",
        !generic.matched,
        "name=" + generic.action_name);
}

// 白名单与永久禁用校验。
void test_whitelist_guard()
{
    check("[ID] wave=26", ActionCommandMapper::whitelist_id("wave") == 26);
    check("[ID] wave_low=25", ActionCommandMapper::whitelist_id("wave_low") == 25);
    check("[ID] hands_up=15", ActionCommandMapper::whitelist_id("hands_up") == 15);
    check("[ID] clap=17", ActionCommandMapper::whitelist_id("clap") == 17);
    check("[ID] heart=20", ActionCommandMapper::whitelist_id("heart") == 20);
    check("[ID] victory=30", ActionCommandMapper::whitelist_id("victory") == 30);
    check(
        "[ID] hand_on_heart 不在白名单",
        ActionCommandMapper::whitelist_id("hand_on_heart") == 0);
    check(
        "[ID] 33 为永久禁用常量",
        ActionCommandMapper::kForbiddenHandOnHeartId == 33);

    // 任何输入都不会输出 33 或白名单外 ID。
    const ActionCommandMapper mapper;
    const char * inputs[] = {
        "挥手", "比心", "举手", "拍手", "比耶", "轻轻挥手",
        "不要挥手", "把手放在心口", "跳个舞", "随便做个动作",
    };

    bool all_valid = true;

    for (const auto * text : inputs) {
        const auto result = map_text(mapper, text);

        if (result.matched) {
            const int id = ActionCommandMapper::whitelist_id(result.action_name);

            if (id == 0 || id != result.action_id ||
                id == ActionCommandMapper::kForbiddenHandOnHeartId)
            {
                all_valid = false;
            }
        }
    }

    check("[ID] 所有输出都在白名单内且无 33", all_valid);
}

}  // namespace

int main()
{
    test_whitelist_phrases();
    test_rejections();
    test_whitelist_guard();

    std::cout << "\nTotal: " << (passed + failed)
              << " | passed: " << passed
              << " | failed: " << failed << '\n';

    return failed > 0 ? 1 : 0;
}
