#include "g1_voice_router/robot_state_evaluator.hpp"

#include <cstdio>
#include <iostream>
#include <string>

using g1_voice_router::AggregatedRobotState;
using g1_voice_router::DataStatus;
using g1_voice_router::RobotStateEvaluator;
using g1_voice_router::StatusQuestionType;

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

bool has_text(const std::string & text, const std::string & part)
{
    return text.find(part) != std::string::npos;
}

// -------- BMS / 电量 --------

void test_battery()
{
    RobotStateEvaluator evaluator;

    // 从未收到 BMS 数据：UNAVAILABLE
    auto state = evaluator.evaluate(1000);
    check("[BAT] no data => UNAVAILABLE",
          state.battery.state.status == DataStatus::UNAVAILABLE);
    check("[BAT] no data reason",
          state.battery.state.quality_reason == "SOURCE_MISSING");

    // 有效 SOC
    evaluator.update_bms(66, 94, true, 50173, 1000);
    state = evaluator.evaluate(1100);
    check("[BAT] valid soc",
          state.battery.state.status == DataStatus::VALID &&
          state.battery.soc_percent == 66);
    check("[BAT] valid soh", state.battery.soh_percent == 94);
    check("[BAT] voltage passthrough", state.battery.voltage_mv == 50173);

    // 断流超过 2 秒：STALE
    state = evaluator.evaluate(1000 + 2001);
    check("[BAT] timeout => STALE",
          state.battery.state.status == DataStatus::STALE);

    // SOC 超范围：INVALID，且不污染最近有效值
    evaluator.update_bms(250, 94, true, 50173, 5000);
    state = evaluator.evaluate(5001);
    check("[BAT] soc out of range => INVALID",
          state.battery.state.status == DataStatus::INVALID &&
          state.battery.state.quality_reason == "INVALID_RANGE");

    // 短窗口跳变（66 -> 90，间隔 30 秒 < 60 秒）：INVALID(UNSTABLE)
    evaluator.update_bms(66, 94, true, 50173, 6000);
    evaluator.update_bms(90, 94, true, 50173, 36000);
    state = evaluator.evaluate(36001);
    check("[BAT] soc jump => INVALID UNSTABLE",
          state.battery.state.status == DataStatus::INVALID &&
          state.battery.state.quality_reason == "UNSTABLE_SOC_JUMP");

    // 长间隔后的大幅变化（真实放电）是允许的
    RobotStateEvaluator evaluator2;
    evaluator2.update_bms(66, 94, true, 50173, 0);
    evaluator2.update_bms(35, 94, true, 46860, 60000 + 1000);
    state = evaluator2.evaluate(61001);
    check("[BAT] slow drain accepted",
          state.battery.state.status == DataStatus::VALID &&
          state.battery.soc_percent == 35);
}

// -------- IMU / LowState --------

void test_imu()
{
    RobotStateEvaluator evaluator;

    auto state = evaluator.evaluate(100);
    check("[IMU] no data => UNAVAILABLE",
          state.imu.state.status == DataStatus::UNAVAILABLE &&
          state.imu.data_available == false);

    evaluator.update_lowstate(true, 29, 49, 100);
    state = evaluator.evaluate(150);
    check("[IMU] valid reception",
          state.imu.state.status == DataStatus::VALID &&
          state.imu.data_available == true);

    // 断流 1 秒：STALE
    state = evaluator.evaluate(100 + 1001);
    check("[IMU] timeout => STALE",
          state.imu.state.status == DataStatus::STALE);

    // joints 摘要：单位未确认
    check("[JOINT] unit not confirmed",
          state.joints.unit_confirmed == false &&
          state.joints.temperature_max_raw == 49);

    // 温度问题：任何状态下都必须安全兜底，不播报数值
    const std::string temp_answer =
        RobotStateEvaluator::generate_status_answer(&state, "你的关节温度是多少");
    check("[JOINT] temperature always safe fallback",
          has_text(temp_answer, "暂时无法准确回答") &&
          !has_text(temp_answer, "49"));
}

// -------- 动作状态 --------

void test_action()
{
    RobotStateEvaluator evaluator;

    auto state = evaluator.evaluate(100);
    check("[ACT] no data => UNAVAILABLE",
          state.action.state.status == DataStatus::UNAVAILABLE);

    // 空闲：id=0
    evaluator.update_action("{\"holding\": false, \"id\": 0, \"name\": \"\"}", 100);
    state = evaluator.evaluate(150);
    check("[ACT] idle",
          state.action.state.status == DataStatus::VALID &&
          state.action.is_executing == false &&
          state.action.action_id == 0);

    // 执行中：id=26 wave_above_head（真实结构）
    evaluator.update_action(
        "{\"holding\": false, \"id\": 26, \"name\": \"wave_above_head\"}", 1000);
    state = evaluator.evaluate(1050);
    check("[ACT] executing",
          state.action.is_executing == true &&
          state.action.action_name == "wave_above_head");

    // 断流 1 秒：STALE
    state = evaluator.evaluate(1000 + 1001);
    check("[ACT] timeout => STALE",
          state.action.state.status == DataStatus::STALE);

    // JSON 错误：INVALID(PARSE_ERROR)，不影响已有字段的其他部分
    evaluator.update_action("{not valid json", 5000);
    state = evaluator.evaluate(5001);
    check("[ACT] parse error => INVALID",
          state.action.state.status == DataStatus::INVALID &&
          state.action.state.quality_reason == "PARSE_ERROR");

    // 非法结构（缺 id）：INVALID
    evaluator.update_action("{\"holding\": false}", 5200);
    state = evaluator.evaluate(5201);
    check("[ACT] missing id => INVALID",
          state.action.state.status == DataStatus::INVALID);

    // 空闲回答
    RobotStateEvaluator evaluator2;
    evaluator2.update_action("{\"holding\": false, \"id\": 0, \"name\": \"\"}", 0);
    const auto idle_state = evaluator2.evaluate(100);
    const std::string idle_answer =
        RobotStateEvaluator::generate_status_answer(&idle_state, "你现在正在执行什么动作");
    check("[ACT] idle answer",
          has_text(idle_answer, "没有在执行动作"));

    // 执行中回答使用真实动作名
    evaluator2.update_action(
        "{\"holding\": false, \"id\": 26, \"name\": \"wave_above_head\"}", 1000);
    const auto exec_state = evaluator2.evaluate(1100);
    const std::string exec_answer =
        RobotStateEvaluator::generate_status_answer(&exec_state, "你现在正在执行什么动作");
    check("[ACT] executing answer uses real name",
          has_text(exec_answer, "wave_above_head"));
}

// -------- 聚合独立性与 JSON 往返 --------

void test_aggregation_and_json()
{
    // 仅 BMS 有效，其他全部 UNAVAILABLE，互不影响
    RobotStateEvaluator evaluator;
    evaluator.update_bms(66, 94, true, 50173, 100);
    auto state = evaluator.evaluate(200);
    check("[AGG] battery valid while others unavailable",
          state.battery.state.status == DataStatus::VALID &&
          state.imu.state.status == DataStatus::UNAVAILABLE &&
          state.action.state.status == DataStatus::UNAVAILABLE);

    // JSON 序列化 → 解析往返
    const std::string json = RobotStateEvaluator::to_json(state);
    AggregatedRobotState parsed;
    check("[AGG] json parse ok",
          RobotStateEvaluator::parse_robot_state_json(json, parsed));
    check("[AGG] roundtrip battery",
          parsed.battery.state.status == DataStatus::VALID &&
          parsed.battery.soc_percent == 66);
    check("[AGG] roundtrip action unavailable",
          parsed.action.state.status == DataStatus::UNAVAILABLE);

    // 无效字段必须输出 null 而不是 0（用 SOC 超范围的 INVALID 状态验证）
    RobotStateEvaluator evaluator3;
    evaluator3.update_bms(250, 94, true, 50173, 100);
    const std::string invalid_json =
        RobotStateEvaluator::to_json(evaluator3.evaluate(200));
    check("[AGG] invalid soc serialized as null",
          has_text(invalid_json, "\"soc_percent\":null") &&
          has_text(invalid_json, "\"status\":\"INVALID\""));

    // 损坏 JSON 与未知 schema 拒绝
    AggregatedRobotState bad;
    check("[AGG] broken json rejected",
          !RobotStateEvaluator::parse_robot_state_json("{broken", bad));
    check("[AGG] unknown schema rejected",
          !RobotStateEvaluator::parse_robot_state_json(
              "{\"schema_version\": 99}", bad));
    check("[AGG] non-object rejected",
          !RobotStateEvaluator::parse_robot_state_json("[1,2,3]", bad));

    // STALE 的 JSON 原样保留状态，数值字段为 null
    RobotStateEvaluator evaluator2;
    evaluator2.update_bms(66, 94, true, 50173, 0);
    const auto stale_state = evaluator2.evaluate(3000);
    const std::string stale_json = RobotStateEvaluator::to_json(stale_state);
    AggregatedRobotState stale_parsed;
    RobotStateEvaluator::parse_robot_state_json(stale_json, stale_parsed);
    check("[AGG] stale battery roundtrip",
          stale_parsed.battery.state.status == DataStatus::STALE &&
          stale_parsed.battery.soc_percent == -1);
}

// -------- Voice Router 回答生成与兜底 --------

void test_answers()
{
    // Provider 未启动 / 缓存过期 / 解析失败：state=nullptr
    // 按问题子类型返回“无法确认”安全回答（绝不进 Ollama）
    const std::string null_battery =
        RobotStateEvaluator::generate_status_answer(nullptr, "你现在有多少电量");
    check("[ANS] null state battery => safe",
          has_text(null_battery, "无法准确确认电池电量"));

    const std::string null_runtime =
        RobotStateEvaluator::generate_status_answer(nullptr, "你现在运行状态怎么样");
    check("[ANS] null state runtime => generic fallback",
          has_text(null_runtime, "还没有接入完整的实时机器人状态数据"));

    const std::string null_action =
        RobotStateEvaluator::generate_status_answer(nullptr, "动作做完了吗");
    check("[ANS] null state action => safe",
          has_text(null_action, "无法准确确认当前动作执行状态"));

    // 全部 UNAVAILABLE 的状态同样兜底
    RobotStateEvaluator evaluator;
    const auto empty_state = evaluator.evaluate(100);
    const std::string empty_answer =
        RobotStateEvaluator::generate_status_answer(&empty_state, "你现在运行状态怎么样");
    check("[ANS] all unavailable => fallback",
          has_text(empty_answer, "还没有接入完整的实时机器人状态数据"));

    // 电量：VALID 播报百分比
    evaluator.update_bms(66, 94, true, 50173, 200);
    evaluator.update_lowstate(true, 29, 49, 200);
    const auto full_state = evaluator.evaluate(300);

    const std::string battery_answer =
        RobotStateEvaluator::generate_status_answer(&full_state, "你现在有多少电量");
    check("[ANS] battery valid answer",
          has_text(battery_answer, "百分之66"));

    const std::string battery_bad =
        RobotStateEvaluator::generate_status_answer(nullptr, "你的电池电量是多少");
    check("[ANS] battery invalid => safe",
          has_text(battery_bad, "无法准确确认电池电量"));

    // IMU：只描述接收情况，不做健康诊断
    const std::string imu_answer =
        RobotStateEvaluator::generate_status_answer(&full_state, "你的IMU数据正常吗");
    check("[ANS] imu reception-only answer",
          has_text(imu_answer, "持续接收到IMU数据") &&
          has_text(imu_answer, "健康诊断"));

    // 运行状态：汇总真实可用字段
    const std::string runtime_answer =
        RobotStateEvaluator::generate_status_answer(&full_state, "你现在运行状态怎么样");
    check("[ANS] runtime summary",
          has_text(runtime_answer, "持续接收") &&
          has_text(runtime_answer, "百分之66") &&
          has_text(runtime_answer, "IMU数据接收正常"));

    // 问题子类型分类
    check("[CLS] battery", RobotStateEvaluator::classify_status_question(
        "你现在还有多少电量") == StatusQuestionType::BATTERY);
    check("[CLS] imu", RobotStateEvaluator::classify_status_question(
        "当前IMU状态怎么样") == StatusQuestionType::IMU);
    check("[CLS] action", RobotStateEvaluator::classify_status_question(
        "动作做完了吗") == StatusQuestionType::ACTION);
    check("[CLS] temperature", RobotStateEvaluator::classify_status_question(
        "你的关节温度是多少") == StatusQuestionType::TEMPERATURE);
    check("[CLS] runtime", RobotStateEvaluator::classify_status_question(
        "你现在运行状态怎么样") == StatusQuestionType::RUNTIME);
}

}  // namespace

int main()
{
    test_battery();
    test_imu();
    test_action();
    test_aggregation_and_json();
    test_answers();

    std::cout << "\nTotal: " << (passed + failed)
              << " | passed: " << passed
              << " | failed: " << failed << '\n';

    return failed > 0 ? 1 : 0;
}
