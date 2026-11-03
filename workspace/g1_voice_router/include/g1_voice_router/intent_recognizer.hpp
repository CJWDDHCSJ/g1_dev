#ifndef G1_VOICE_ROUTER_INTENT_RECOGNIZER_HPP_
#define G1_VOICE_ROUTER_INTENT_RECOGNIZER_HPP_

#include <string>

namespace g1_voice_router
{

// 用户意图类型
enum class Intent
{
    GREETING,       // 问候
    LAB_INTRO,      // 实验室相关问题
    ROBOT_INTRO,    // 机器人介绍
    DEMO_ACTION,    // 动作演示
    ROBOT_STATUS,   // 机器人状态查询
    GOODBYE,        // 告别
    GENERAL_QA      // 普通问题
};

// 一次意图识别的结果
struct IntentResult
{
    Intent intent{Intent::GENERAL_QA};

    // 去除标点、空格并进行英文小写转换后的文本
    std::string normalized_text;
};

// 意图识别器
class IntentRecognizer
{
public:
    // 输入 ASR 识别文本，返回对应意图
    IntentResult recognize(const std::string &text) const;

    // 将枚举类型转换为可读字符串，方便日志输出
    static const char *to_string(Intent intent) noexcept;

private:
    // 文本归一化
    static std::string normalize(const std::string &text);
};

}  // namespace g1_voice_router

#endif  // G1_VOICE_ROUTER_INTENT_RECOGNIZER_HPP_
