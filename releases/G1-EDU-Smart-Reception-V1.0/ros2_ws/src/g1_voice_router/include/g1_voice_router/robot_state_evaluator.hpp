#ifndef G1_VOICE_ROUTER_ROBOT_STATE_EVALUATOR_HPP_
#define G1_VOICE_ROUTER_ROBOT_STATE_EVALUATOR_HPP_

#include <string>
#include <vector>

#include "g1_voice_router/robot_state_types.hpp"

namespace g1_voice_router
{

// 分话题数据超时（毫秒）。
// 依据 P0-18 实测频率：/lowstate ≈1053Hz、/sportmodestate ≈100Hz、
// /arm/action/state ≈10Hz（持续发布，非事件型）、/lf/bmsstate ≈20Hz。
// 超时窗口取标称周期的 20~100 倍，容忍瞬时抖动；
// 事件型话题（如未来的 /g1/action_result）不适用本表。
struct StateTimeouts
{
    static constexpr long kLowStateMs = 1000;
    static constexpr long kSportModeMs = 1000;
    static constexpr long kArmActionMs = 1000;
    static constexpr long kBmsMs = 2000;
};

// Provider 聚合 JSON 的缓存有效期（Voice Router 侧）。
// Provider 以 2Hz 发布；缓存超过该时间视为 Provider 掉线。
constexpr long kRobotStateCacheTimeoutMs = 5000;

// SOC 稳定性规则（依据 P0-18 60 秒 1229 样本实测：
// 窗口内零跳变；跨小时单调下降与电压交叉验证一致）。
// 短时间（< 60 秒）内变化 >= 15 个百分点视为异常跳变。
constexpr int kSocJumpThresholdPercent = 15;
constexpr long kSocJumpWindowMs = 60000;

enum class StatusQuestionType
{
    BATTERY,
    IMU,
    ACTION,
    TEMPERATURE,
    RUNTIME
};

// 纯逻辑评估器：不依赖 ROS，时钟以毫秒参数注入，便于单元测试。
// 线程约定：Provider 侧由单一回调线程 + 定时器线程访问时需外部加锁；
// Voice Router 侧仅在解析缓存时使用纯函数。
class RobotStateEvaluator
{
public:
    RobotStateEvaluator();

    // /lf/bmsstate：soc/soh 百分比，电压 mV。
    // 内置范围校验（0..100）与短窗口跳变检测，
    // 异常样本会被拒绝并记录 INVALID 原因，不污染最近有效值。
    void update_bms(
        int soc_percent,
        int soh_percent,
        bool has_voltage,
        long voltage_mv,
        long now_ms);

    // /lowstate：IMU 数据可用性 + 关节温度原始值摘要。
    void update_lowstate(
        bool imu_data_available,
        int available_motor_count,
        int temperature_max_raw,
        long now_ms);

    // /sportmodestate：FSM 数值仅记录，不做语义推断。
    void update_sportmode(long now_ms);

    // /arm/action/state：JSON 字符串 {"holding":bool,"id":int,"name":str}。
    // 解析失败记 INVALID(PARSE_ERROR)，不覆盖最近有效值。
    void update_action(const std::string & json_text, long now_ms);

    // 依据当前时刻计算各字段有效性并聚合。
    AggregatedRobotState evaluate(long now_ms) const;

    // 序列化为 /g1/robot_state 负载（schema_version=1）。
    static std::string to_json(const AggregatedRobotState & state);

    // Voice Router 侧：解析 /g1/robot_state 负载。
    // schema_version 不为 1、JSON 非法或根对象缺失时返回 false。
    static bool parse_robot_state_json(
        const std::string & json_text,
        AggregatedRobotState & state);

    // Voice Router 侧：判断状态问题子类型（内部自带归一化）。
    static StatusQuestionType classify_status_question(
        const std::string & user_text);

    // Voice Router 侧：根据解析后的状态与问题生成本地回答。
    // state 为 nullptr（未收到/过期/解析失败）时返回安全兜底，
    // 任何路径都不会把状态问题交给 Ollama。
    static std::string generate_status_answer(
        const AggregatedRobotState * state,
        const std::string & user_text);

    static const char * kDefaultStatusFallback;

private:
    // 最近一次有效 BMS 采样与有效性覆盖标记。
    bool bms_received_{false};
    bool bms_invalid_{false};
    std::string bms_invalid_reason_;
    long bms_last_ms_{-1};
    int soc_percent_{-1};
    int soh_percent_{-1};
    long voltage_mv_{-1};
    bool has_voltage_{false};
    // SOC 跳变检测参考点（仅记录有效样本）。
    int last_good_soc_{-1};
    long last_good_soc_ms_{-1};

    bool lowstate_received_{false};
    bool imu_available_{false};
    long lowstate_last_ms_{-1};
    int motor_count_{-1};
    int temp_max_raw_{-1};

    bool sportmode_received_{false};
    long sportmode_last_ms_{-1};

    bool action_received_{false};
    bool action_invalid_{false};
    std::string action_invalid_reason_;
    long action_last_ms_{-1};
    bool action_is_executing_{false};
    bool action_is_releasing_{false};
    int action_id_{-1};
    std::string action_name_;
};

}  // namespace g1_voice_router

#endif  // G1_VOICE_ROUTER_ROBOT_STATE_EVALUATOR_HPP_
