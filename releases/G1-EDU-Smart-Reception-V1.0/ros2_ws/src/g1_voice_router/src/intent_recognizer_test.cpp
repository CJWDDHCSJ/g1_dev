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
        {"学院有什么在研项目", Intent::LAB_INTRO},
        {"脑机中心负责人是谁", Intent::LAB_INTRO},
        {"学院有哪些科研团队", Intent::LAB_INTRO},
        {"学院学生拿过什么奖", Intent::LAB_INTRO},
        {"学院什么时候成立", Intent::LAB_INTRO},
        {"学院地址在哪里", Intent::LAB_INTRO},
        {"具身智能开放平台是什么", Intent::LAB_INTRO},
        {"肖汉光教授是谁", Intent::LAB_INTRO},
        {"有机器狗吗", Intent::LAB_INTRO},
        {"学院有哪些专业", Intent::LAB_INTRO},
        {"学院有什么专业", Intent::LAB_INTRO},
        {"机器狗多少钱", Intent::GENERAL_QA},
        {"宇树机器人多少钱？", Intent::GENERAL_QA},
        {"脑机接口是什么", Intent::GENERAL_QA},
        {"具身智能是什么", Intent::GENERAL_QA},
        {"Embodied AI 是什么意思", Intent::GENERAL_QA},
        {"什么是脑机接口", Intent::GENERAL_QA},
        {"介绍一下脑机接口", Intent::GENERAL_QA},
        {"我们实验室如何开展脑机接口研究", Intent::LAB_INTRO},
        {"你们的具身智能研究方向是什么", Intent::LAB_INTRO},
        {"重点实验室什么时候获批", Intent::LAB_INTRO},
        {"重点实验室什么时候成立", Intent::LAB_INTRO},
        {"你们实验室主要研究什么", Intent::LAB_INTRO},
        {"你现在运行状态怎么样", Intent::ROBOT_STATUS},
        {"你现在有多少电量", Intent::ROBOT_STATUS},
        {"你的IMU数据正常吗", Intent::ROBOT_STATUS},
        {"你们实验室有哪些研究方向", Intent::LAB_INTRO},
        // P0-17.2 语义边界修正：实验室状态询问归 LAB_INTRO，
        // 不再被宽泛的“状态”关键词抢占为 ROBOT_STATUS。
        {"实验室当前状态怎么样", Intent::LAB_INTRO},
        {"实验室运行正常吗", Intent::LAB_INTRO},
        {"给我们表演一个动作", Intent::DEMO_ACTION},
        {"做个动作", Intent::DEMO_ACTION},
        // ---- P0-17.2 测试矩阵 8.1：ROBOT_STATUS 正向 ----
        {"机器人现在运行正常吗", Intent::ROBOT_STATUS},
        {"G1当前电池电量是多少", Intent::ROBOT_STATUS},
        {"机器人现在在做什么", Intent::ROBOT_STATUS},
        {"动作做完了吗", Intent::ROBOT_STATUS},
        {"刚才的挥手动作完成了吗", Intent::ROBOT_STATUS},
        {"你的关节温度是多少", Intent::ROBOT_STATUS},
        {"你现在正在执行什么动作", Intent::ROBOT_STATUS},
        {"机器人当前姿态怎么样", Intent::ROBOT_STATUS},
        // ---- P0-17.2 测试矩阵 8.2：GENERAL_QA 负向（知识问法）----
        {"机器人如何判断自己的运行状态", Intent::GENERAL_QA},
        {"机器人运行状态监测的原理是什么", Intent::GENERAL_QA},
        {"如何通过IMU判断机器人姿态", Intent::GENERAL_QA},
        {"IMU是什么", Intent::GENERAL_QA},
        {"惯性测量单元是什么", Intent::GENERAL_QA},
        {"如何使用IMU估计姿态", Intent::GENERAL_QA},
        {"机器人状态机是什么", Intent::GENERAL_QA},
        {"动作规划是什么", Intent::GENERAL_QA},
        {"机器人如何判断动作是否完成", Intent::GENERAL_QA},
        {"多传感器如何进行状态融合", Intent::GENERAL_QA},
        // ---- P0-17.2 测试矩阵 8.3：LAB_INTRO ----
        {"实验室目前运行情况如何", Intent::LAB_INTRO},
        {"我们实验室现在怎么样", Intent::LAB_INTRO},
        {"实验室目前有哪些研究方向", Intent::LAB_INTRO},
        {"实验室现在有哪些项目", Intent::LAB_INTRO},
        {"实验室有哪些设备", Intent::LAB_INTRO},
        {"实验室负责人是谁", Intent::LAB_INTRO},
        {"我们实验室主要研究什么", Intent::LAB_INTRO},
        // ---- P0-20：直接动作命令短语 ----
        {"挥个手", Intent::DEMO_ACTION},
        {"拍个手", Intent::DEMO_ACTION},
        {"鼓掌", Intent::DEMO_ACTION},
        {"举手", Intent::DEMO_ACTION},
        {"比个耶", Intent::DEMO_ACTION},
        {"剪刀手", Intent::DEMO_ACTION},
        {"轻轻挥手", Intent::DEMO_ACTION},
        {"不要挥手", Intent::DEMO_ACTION},
        // 疑问/原理类保持 GENERAL_QA（不得被动作短语抢占）
        {"挥手的原理是什么", Intent::GENERAL_QA},
        {"挥手是怎么实现的", Intent::GENERAL_QA},
        // ---- P0-17.2 测试矩阵 8.4：DEMO_ACTION 回归 ----
        {"挥挥手", Intent::DEMO_ACTION},
        {"做个胜利手势", Intent::DEMO_ACTION},
        {"给大家展示一下动作", Intent::DEMO_ACTION},
        {"展示一下具身智能开放平台", Intent::LAB_INTRO},
        {"演示一下具身智能开放平台", Intent::LAB_INTRO},
        {"展示一下实验室设备", Intent::LAB_INTRO}
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
