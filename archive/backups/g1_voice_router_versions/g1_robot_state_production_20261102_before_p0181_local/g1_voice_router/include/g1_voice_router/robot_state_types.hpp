#ifndef G1_VOICE_ROUTER_ROBOT_STATE_TYPES_HPP_
#define G1_VOICE_ROUTER_ROBOT_STATE_TYPES_HPP_

#include <string>

namespace g1_voice_router
{

// 数据有效性状态（P0-18）。
enum class DataStatus
{
    VALID,        // 消息存在、字段有效、在有效时间窗口内
    STALE,        // 曾收到有效数据，但已超过有效时间窗口
    UNAVAILABLE,  // 当前没有收到有效来源数据
    INVALID       // 数据结构不合法、字段缺失或数值不符合验证规则
};

const char * to_string(DataStatus status);

// 质量原因标签（附在 FieldState.quality_reason 中，不作为独立状态）。
namespace quality_reason
{
extern const char * kSourceMissing;      // 从未收到数据
extern const char * kMessageTimeout;     // 超过有效时间窗口
extern const char * kInvalidRange;       // 数值超出合理范围
extern const char * kUnstableSocJump;    // SOC 短时间异常跳变
extern const char * kParseError;         // JSON/结构解析失败
extern const char * kSchemaMismatch;     // schema_version 不兼容
extern const char * kNone;
}  // namespace quality_reason

// 单项状态的状态头。
struct FieldState
{
    DataStatus status{DataStatus::UNAVAILABLE};
    long received_age_ms{-1};   // 距最后一次有效接收的毫秒数
    std::string quality_reason{quality_reason::kSourceMissing};
};

// 电池状态（来源 /lf/bmsstate，unitree_hg/msg/BmsState）。
// 仅在语义、单位和数据质量都确认后才填充数值；
// 电流与 BMS 温度单位未确认，不进入本结构，不对外播报。
struct BatteryInfo
{
    FieldState state;
    int soc_percent{-1};    // -1 表示无效，禁止伪装成 0
    int soh_percent{-1};
    long voltage_mv{-1};    // bmsvoltage[0]，单位 mV（与电芯电压交叉验证）
};

// IMU 数据可用性（来源 /lowstate.imu_state）。
// 数据持续接收 ≠ 传感器健康诊断通过。
struct ImuInfo
{
    FieldState state;
    bool data_available{false};
};

// 底层运行状态数据接收情况（/lowstate + /sportmodestate）。
// 通信正常 ≠ 整机健康。
struct RuntimeInfo
{
    FieldState state;
    bool lowstate_available{false};
    bool sportmode_available{false};
};

// 手臂动作状态（来源 /arm/action/state，JSON 字符串）。
// 字段含义以实际 JSON 结构 {"holding":bool,"id":int,"name":str} 为准，
// 不依据 FSM 数值猜测动作名称。
struct ActionInfo
{
    FieldState state;
    bool is_executing{false};   // id != 0
    bool is_releasing{false};   // id == 99（release_arm，复位过渡）
    int action_id{-1};
    std::string action_name;
};

// 关节状态摘要（来源 /lowstate.motor_state）。
// 温度单位未确认、索引与关节名映射未知，仅保留原始值做内部诊断，
// 禁止对外播报具体温度或关节名。
struct JointInfo
{
    FieldState state;
    int available_motor_count{-1};
    int temperature_max_raw{-1};
    bool unit_confirmed{false};
};

// 聚合状态（/g1/robot_state 的业务视图）。
struct AggregatedRobotState
{
    int schema_version{1};
    BatteryInfo battery;
    ImuInfo imu;
    RuntimeInfo runtime;
    ActionInfo action;
    JointInfo joints;
};

}  // namespace g1_voice_router

#endif  // G1_VOICE_ROUTER_ROBOT_STATE_TYPES_HPP_
