#include "g1_voice_router/intent_recognizer.hpp"

#include <iostream>
#include <string>

using g1_voice_router::Intent;
using g1_voice_router::IntentRecognizer;

struct TestCase
{
    std::string input;
    Intent expected;
};

int main()
{
    IntentRecognizer recognizer;

    const TestCase cases[] = {
        {"你好", Intent::GREETING},
        {"您好！", Intent::GREETING},
        {"GOOD MORNING", Intent::GENERAL_QA},
        {"你好，介绍一下你们实验室", Intent::LAB_INTRO},
        {"你们实验室研究什么？", Intent::LAB_INTRO},
        {"介绍一下你自己", Intent::ROBOT_INTRO},
        {"你能做什么", Intent::ROBOT_INTRO},
        {"给我们展示一下", Intent::DEMO_ACTION},
        {"挥挥手", Intent::DEMO_ACTION},
        {"你现在状态怎么样", Intent::ROBOT_STATUS},
        {"当前运行模式是什么？", Intent::ROBOT_STATUS},
        {"谢谢", Intent::GOODBYE},
        {"再见！", Intent::GOODBYE},
        {"宇树机器人多少钱？", Intent::GENERAL_QA}
    };

    int passed = 0;
    int failed = 0;

    std::cout << "========== Intent Recognizer Test =========="
              << std::endl;

    for (const auto &test : cases)
    {
        const auto result = recognizer.recognize(test.input);

        const bool success = result.intent == test.expected;

        std::cout
            << (success ? "[PASS] " : "[FAIL] ")
            << test.input
            << " -> "
            << IntentRecognizer::to_string(result.intent)
            << std::endl;

        if (success)
        {
            ++passed;
        }
        else
        {
            ++failed;

            std::cout
                << "  Expected: "
                << IntentRecognizer::to_string(test.expected)
                << std::endl;
        }
    }

    std::cout << "============================================" << std::endl;
    std::cout << "Passed: " << passed << std::endl;
    std::cout << "Failed: " << failed << std::endl;

    return failed == 0 ? 0 : 1;
}
