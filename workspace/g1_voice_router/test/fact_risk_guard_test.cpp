#include "g1_voice_router/fact_risk_guard.hpp"

#include <cstdio>
#include <iostream>
#include <string>

#include "g1_voice_router/knowledge_base.hpp"

using g1_voice_router::FactRisk;
using g1_voice_router::FactRiskGuard;
using g1_voice_router::FactRiskResult;
using g1_voice_router::KnowledgeBase;

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

std::string risk_name(FactRisk risk)
{
    return g1_voice_router::to_string(risk);
}

bool contains(
    const FactRiskResult & result,
    const std::string & text)
{
    return result.fallback_answer.find(text) != std::string::npos ||
           result.local_answer.find(text) != std::string::npos;
}

// -------- 价格类：必须拦截，不得进入 Ollama --------

void check_price_cases(const FactRiskGuard & guard)
{
    const char * price_inputs[] = {
        "宇树机器人多少钱",
        "G1多少钱",
        "宇树机器狗价格是多少",
        "G1 EDU现在报价多少",
    };

    for (const auto * input : price_inputs) {
        const auto result = guard.evaluate(input, nullptr);

        check(
            std::string("[PRICE] risk==PRICE: ") + input,
            result.risk == FactRisk::PRICE,
            "risk=" + risk_name(result.risk));

        check(
            std::string("[PRICE] blocked: ") + input,
            result.blocked,
            "blocked=false");

        check(
            std::string("[PRICE] no local answer: ") + input,
            !result.use_local_answer);

        check(
            std::string("[PRICE] safe fallback text: ") + input,
            contains(result, "价格信息") &&
            contains(result, "不能随意给你报价"),
            "fallback=" + result.fallback_answer);
    }
}

// -------- 实时状态：必须拦截，不得让模型编造传感器数据 --------

void check_realtime_cases(const FactRiskGuard & guard)
{
    const char * realtime_inputs[] = {
        "G1当前温度是多少",
        "机器人现在正常吗",
        "当前关节温度是多少",
        "机器人现在电量多少",
    };

    for (const auto * input : realtime_inputs) {
        const auto result = guard.evaluate(input, nullptr);

        check(
            std::string("[STATUS] risk==REALTIME_STATUS: ") + input,
            result.risk == FactRisk::REALTIME_STATUS,
            "risk=" + risk_name(result.risk));

        check(
            std::string("[STATUS] blocked: ") + input,
            result.blocked,
            "blocked=false");

        check(
            std::string("[STATUS] safe fallback text: ") + input,
            contains(result, "暂时无法确认"),
            "fallback=" + result.fallback_answer);
    }

    // 防过度拦截：没有机器人语境的“温度”问题仍属普通知识。
    const auto weather = guard.evaluate("今天天气温度怎么样", nullptr);

    check(
        "[STATUS] non-robot temperature stays NORMAL",
        weather.risk == FactRisk::NORMAL && !weather.blocked,
        "risk=" + risk_name(weather.risk) +
        " blocked=" + (weather.blocked ? "true" : "false"));
}

// -------- 普通知识：必须放行进入 Ollama --------

void check_normal_cases(const FactRiskGuard & guard)
{
    const char * normal_inputs[] = {
        "脑机接口是什么",
        "具身智能是什么",
        "多模态融合是什么",
        "Transformer是什么",
        "给我们表演一个动作",
    };

    for (const auto * input : normal_inputs) {
        const auto result = guard.evaluate(input, nullptr);

        check(
            std::string("[NORMAL] risk==NORMAL: ") + input,
            result.risk == FactRisk::NORMAL,
            "risk=" + risk_name(result.risk));

        check(
            std::string("[NORMAL] not blocked: ") + input,
            !result.blocked);

        check(
            std::string("[NORMAL] no local answer: ") + input,
            !result.use_local_answer);
    }
}

// -------- P0-17.1：实时状态正向测试（Guard 独立防线）--------

void check_realtime_status_cases_p0171(const FactRiskGuard & guard)
{
    const char * status_inputs[] = {
        // 运行状态
        "你现在运行状态怎么样",
        "机器人现在运行正常吗",
        "机器人目前是什么状态",
        "你现在正常吗",
        "现在机器人状态怎么样",
        // IMU / 姿态 / 惯导
        "你的IMU数据正常吗",
        "IMU数据正常吗",
        "当前IMU状态怎么样",
        "你的姿态数据正常吗",
        "惯导数据是否正常",
        // 动作执行状态
        "你现在正在执行什么动作",
        "你当前执行的是什么动作",
        "机器人现在在做什么",
        "动作做完了吗",
        "刚才的挥手动作完成了吗",
        "你现在的动作执行完成了吗",
    };

    for (const auto * input : status_inputs) {
        const auto result = guard.evaluate(input, nullptr);

        check(
            std::string("[P0171+] risk==REALTIME_STATUS: ") + input,
            result.risk == FactRisk::REALTIME_STATUS,
            "risk=" + risk_name(result.risk) +
            " reason=" + result.reason);

        check(
            std::string("[P0171+] blocked: ") + input,
            result.blocked);
    }
}

// -------- P0-17.1：普通知识负向测试（防过度拦截）--------

void check_knowledge_negative_cases_p0171(const FactRiskGuard & guard)
{
    const char * knowledge_inputs[] = {
        "IMU是什么",
        "什么是惯性测量单元",
        "如何使用IMU估计姿态",
        "动作规划是什么",
        "机器人如何判断自己的运行状态",
        "机器人的状态机是如何工作的",
        "脑机接口是什么",
        "给我们表演一个动作",
    };

    for (const auto * input : knowledge_inputs) {
        const auto result = guard.evaluate(input, nullptr);

        check(
            std::string("[P0171-] risk==NORMAL: ") + input,
            result.risk == FactRisk::NORMAL,
            "risk=" + risk_name(result.risk) +
            " reason=" + result.reason);

        check(
            std::string("[P0171-] not blocked: ") + input,
            !result.blocked);
    }
}

// -------- 实验室事实：优先查知识库，无资料必须兜底 --------

void check_lab_fact_cases(
    const FactRiskGuard & guard,
    const KnowledgeBase & kb)
{
    // 有可靠匹配：使用本地答案，不进 Ollama。
    {
        const auto result =
            guard.evaluate("实验室有哪些设备", &kb);

        check("[LAB] equipment risk==LAB_FACT",
              result.risk == FactRisk::LAB_FACT);

        check("[LAB] equipment uses local answer",
              result.use_local_answer && !result.blocked);

        check("[LAB] equipment kb hit id",
              result.kb_matched_id == "faq_equipment",
              "id=" + result.kb_matched_id);

        check("[LAB] equipment answer verified",
              contains(result, "具身智能开放平台") &&
              contains(result, "没有公开"),
              "answer=" + result.local_answer);
    }

    {
        const auto result =
            guard.evaluate("脑机中心负责人是谁", &kb);

        check("[LAB] leaders risk==LAB_FACT",
              result.risk == FactRisk::LAB_FACT);

        check("[LAB] leaders uses local answer",
              result.use_local_answer && !result.blocked);

        check("[LAB] leaders kb hit id",
              result.kb_matched_id == "faq_bci_leaders",
              "id=" + result.kb_matched_id);

        check("[LAB] leaders answer verified",
              contains(result, "蒋田仔") || contains(result, "刘宏"),
              "answer=" + result.local_answer);
    }

    // 无可靠资料：必须兜底，绝不允许交给 Ollama 猜测。
    const char * no_data_inputs[] = {
        "实验室有多少名研究人员",
        "学院有哪些科研成果",
        "重点实验室目前有哪些研究成果",
        "实验室设备是什么型号",
        "实验室正在开展哪些项目",
    };

    for (const auto * input : no_data_inputs) {
        const auto result = guard.evaluate(input, &kb);

        check(
            std::string("[LAB] risk==LAB_FACT: ") + input,
            result.risk == FactRisk::LAB_FACT,
            "risk=" + risk_name(result.risk));

        check(
            std::string("[LAB] kb fallback => blocked: ") + input,
            result.blocked && !result.use_local_answer);

        check(
            std::string("[LAB] safe fallback text: ") + input,
            contains(result, "没有经过核实的公开资料") &&
            contains(result, "暂时无法准确回答"),
            "fallback=" + result.fallback_answer);
    }
}

// -------- 降级场景：知识库缺失时实验室事实必须兜底 --------

void check_degraded_cases(const FactRiskGuard & guard)
{
    {
        const auto result =
            guard.evaluate("实验室有哪些设备", nullptr);

        check("[DEGRADED] null kb => blocked",
              result.blocked && result.risk == FactRisk::LAB_FACT,
              "risk=" + risk_name(result.risk));

        check("[DEGRADED] null kb safe text",
              contains(result, "暂时无法准确回答"));
    }

    {
        KnowledgeBase unloaded_kb;
        const auto result =
            guard.evaluate("脑机中心负责人是谁", &unloaded_kb);

        check("[DEGRADED] unloaded kb => blocked",
              result.blocked && !result.use_local_answer);

        check("[DEGRADED] unloaded kb safe text",
              contains(result, "暂时无法准确回答"));
    }
}

}  // namespace

int main(int argc, char ** argv)
{
    FactRiskGuard guard;

    check_price_cases(guard);
    check_realtime_cases(guard);
    check_normal_cases(guard);
    check_realtime_status_cases_p0171(guard);
    check_knowledge_negative_cases_p0171(guard);
    check_degraded_cases(guard);

    if (argc >= 2) {
        KnowledgeBase kb;
        std::string error;

        if (!kb.load(argv[1], error)) {
            std::cerr << "knowledge base load failed: " << error << '\n';
            return 1;
        }

        check_lab_fact_cases(guard, kb);
    } else {
        std::cout << "[SKIP] no knowledge file given; "
                     "lab-fact cases skipped\n";
    }

    std::cout << "\nTotal: " << (passed + failed)
              << " | passed: " << passed
              << " | failed: " << failed << '\n';

    return failed > 0 ? 1 : 0;
}
