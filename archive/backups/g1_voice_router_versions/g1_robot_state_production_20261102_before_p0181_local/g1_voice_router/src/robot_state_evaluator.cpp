#include "g1_voice_router/robot_state_evaluator.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>

#include <nlohmann/json.hpp>

namespace g1_voice_router
{

using Json = nlohmann::json;

namespace quality_reason
{
const char * kSourceMissing = "SOURCE_MISSING";
const char * kMessageTimeout = "MESSAGE_TIMEOUT";
const char * kInvalidRange = "INVALID_RANGE";
const char * kUnstableSocJump = "UNSTABLE_SOC_JUMP";
const char * kParseError = "PARSE_ERROR";
const char * kSchemaMismatch = "SCHEMA_MISMATCH";
const char * kNone = "";
}  // namespace quality_reason

namespace
{

using quality_reason::kInvalidRange;
using quality_reason::kMessageTimeout;
using quality_reason::kNone;
using quality_reason::kParseError;
using quality_reason::kSchemaMismatch;
using quality_reason::kSourceMissing;
using quality_reason::kUnstableSocJump;

std::string normalize_question(std::string text)
{
    static const char * punctuation[] = {
        "。", "！", "？", "，", "、", "；", "：",
        ".", ",", "!", "?", ";", ":",
        "\"", "'", "“", "”", "（", "）", "(", ")",
        " ", "\t", "\r", "\n"
    };

    for (const auto * mark : punctuation) {
        std::size_t pos = 0;
        while ((pos = text.find(mark, pos)) != std::string::npos) {
            text.erase(pos, std::string(mark).size());
        }
    }

    for (char & c : text) {
        const unsigned char byte = static_cast<unsigned char>(c);
        if (byte >= 'A' && byte <= 'Z') {
            c = static_cast<char>(byte - 'A' + 'a');
        }
    }

    return text;
}

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

FieldState make_field(
    DataStatus status,
    long received_age_ms,
    const char * reason)
{
    FieldState field;
    field.status = status;
    field.received_age_ms = received_age_ms;
    field.quality_reason = reason;
    return field;
}

}  // namespace

const char * to_string(DataStatus status)
{
    switch (status) {
        case DataStatus::VALID:
            return "VALID";
        case DataStatus::STALE:
            return "STALE";
        case DataStatus::UNAVAILABLE:
            return "UNAVAILABLE";
        case DataStatus::INVALID:
            return "INVALID";
        default:
            return "UNKNOWN";
    }
}

const char * RobotStateEvaluator::kDefaultStatusFallback =
    "我目前还没有接入完整的实时机器人状态数据，"
    "暂时无法准确判断这个状态，请以实际设备状态为准。";

RobotStateEvaluator::RobotStateEvaluator() = default;

void RobotStateEvaluator::update_bms(
    int soc_percent,
    int soh_percent,
    bool has_voltage,
    long voltage_mv,
    long now_ms)
{
    // 只要收到消息即证明数据源存在；
    // 之后按数据质量决定 VALID / INVALID。
    bms_received_ = true;
    bms_last_ms_ = now_ms;

    // 范围校验：SOC 必须是 0..100 的整数百分比。
    if (soc_percent < 0 || soc_percent > 100) {
        bms_invalid_ = true;
        bms_invalid_reason_ = kInvalidRange;
        return;
    }

    // 短窗口跳变检测：与最近有效样本比较。
    if (last_good_soc_ms_ >= 0 &&
        now_ms > last_good_soc_ms_ &&
        (now_ms - last_good_soc_ms_) < kSocJumpWindowMs)
    {
        const int diff = std::abs(soc_percent - last_good_soc_);

        if (diff >= kSocJumpThresholdPercent) {
            bms_invalid_ = true;
            bms_invalid_reason_ = kUnstableSocJump;
            return;
        }
    }

    bms_received_ = true;
    bms_invalid_ = false;
    bms_invalid_reason_ = kNone;
    bms_last_ms_ = now_ms;
    soc_percent_ = soc_percent;
    soh_percent_ = (soh_percent >= 0 && soh_percent <= 100) ? soh_percent : -1;
    has_voltage_ = has_voltage && voltage_mv > 0;
    voltage_mv_ = has_voltage_ ? voltage_mv : -1;

    last_good_soc_ = soc_percent;
    last_good_soc_ms_ = now_ms;
}

void RobotStateEvaluator::update_lowstate(
    bool imu_data_available,
    int available_motor_count,
    int temperature_max_raw,
    long now_ms)
{
    lowstate_received_ = true;
    lowstate_last_ms_ = now_ms;
    imu_available_ = imu_data_available;
    motor_count_ = available_motor_count;
    temp_max_raw_ = temperature_max_raw;
}

void RobotStateEvaluator::update_sportmode(long now_ms)
{
    sportmode_received_ = true;
    sportmode_last_ms_ = now_ms;
}

void RobotStateEvaluator::update_action(
    const std::string & json_text,
    long now_ms)
{
    Json parsed;

    // 收到消息即证明数据源存在；解析失败记 INVALID 而非 UNAVAILABLE。
    action_received_ = true;
    action_last_ms_ = now_ms;

    try {
        parsed = Json::parse(json_text);
    } catch (const Json::exception &) {
        action_invalid_ = true;
        action_invalid_reason_ = kParseError;
        return;
    }

    if (!parsed.is_object() ||
        !parsed.contains("id") ||
        !parsed["id"].is_number_integer())
    {
        action_invalid_ = true;
        action_invalid_reason_ = kParseError;
        return;
    }

    action_received_ = true;
    action_invalid_ = false;
    action_invalid_reason_ = kNone;
    action_last_ms_ = now_ms;
    action_id_ = parsed["id"].get<int>();
    action_is_releasing_ = (action_id_ == 99);
    action_is_executing_ = (action_id_ != 0);
    action_name_ = parsed.value("name", std::string{});
}

AggregatedRobotState RobotStateEvaluator::evaluate(long now_ms) const
{
    AggregatedRobotState state;

    // ---- 电池 ----
    if (!bms_received_) {
        state.battery.state =
            make_field(DataStatus::UNAVAILABLE, -1, kSourceMissing);
    } else if (bms_invalid_) {
        state.battery.state =
            make_field(DataStatus::INVALID, now_ms - bms_last_ms_, bms_invalid_reason_.c_str());
    } else {
        const long age = now_ms - bms_last_ms_;
        state.battery.state =
            (age > StateTimeouts::kBmsMs)
            ? make_field(DataStatus::STALE, age, kMessageTimeout)
            : make_field(DataStatus::VALID, age, kNone);
        state.battery.soc_percent = soc_percent_;
        state.battery.soh_percent = soh_percent_;
        state.battery.voltage_mv = voltage_mv_;
    }

    // ---- IMU ----
    if (!lowstate_received_) {
        state.imu.state =
            make_field(DataStatus::UNAVAILABLE, -1, kSourceMissing);
    } else {
        const long age = now_ms - lowstate_last_ms_;
        state.imu.state =
            (age > StateTimeouts::kLowStateMs)
            ? make_field(DataStatus::STALE, age, kMessageTimeout)
            : make_field(DataStatus::VALID, age, kNone);
        state.imu.data_available = imu_available_;
    }

    // ---- 底层运行状态 ----
    {
        const bool low_ok = lowstate_received_ &&
            (now_ms - lowstate_last_ms_) <= StateTimeouts::kLowStateMs;
        const bool sport_ok = sportmode_received_ &&
            (now_ms - sportmode_last_ms_) <= StateTimeouts::kSportModeMs;

        state.runtime.lowstate_available = low_ok;
        state.runtime.sportmode_available = sport_ok;

        if (!lowstate_received_ && !sportmode_received_) {
            state.runtime.state =
                make_field(DataStatus::UNAVAILABLE, -1, kSourceMissing);
        } else if (low_ok || sport_ok) {
            const long age = lowstate_received_
                ? (now_ms - lowstate_last_ms_)
                : (now_ms - sportmode_last_ms_);
            state.runtime.state = make_field(DataStatus::VALID, age, kNone);
        } else {
            const long age = lowstate_received_
                ? (now_ms - lowstate_last_ms_)
                : (now_ms - sportmode_last_ms_);
            state.runtime.state =
                make_field(DataStatus::STALE, age, kMessageTimeout);
        }
    }

    // ---- 动作状态 ----
    if (!action_received_) {
        state.action.state =
            make_field(DataStatus::UNAVAILABLE, -1, kSourceMissing);
    } else if (action_invalid_) {
        state.action.state =
            make_field(DataStatus::INVALID, now_ms - action_last_ms_, action_invalid_reason_.c_str());
    } else {
        const long age = now_ms - action_last_ms_;
        state.action.state =
            (age > StateTimeouts::kArmActionMs)
            ? make_field(DataStatus::STALE, age, kMessageTimeout)
            : make_field(DataStatus::VALID, age, kNone);
        state.action.is_executing = action_is_executing_;
        state.action.is_releasing = action_is_releasing_;
        state.action.action_id = action_id_;
        state.action.action_name = action_name_;
    }

    // ---- 关节摘要（温度单位未确认，仅内部诊断）----
    if (!lowstate_received_) {
        state.joints.state =
            make_field(DataStatus::UNAVAILABLE, -1, kSourceMissing);
    } else {
        const long age = now_ms - lowstate_last_ms_;
        state.joints.state =
            (age > StateTimeouts::kLowStateMs)
            ? make_field(DataStatus::STALE, age, kMessageTimeout)
            : make_field(DataStatus::VALID, age, kNone);
        state.joints.available_motor_count = motor_count_;
        state.joints.temperature_max_raw = temp_max_raw_;
        state.joints.unit_confirmed = false;
    }

    return state;
}

std::string RobotStateEvaluator::to_json(const AggregatedRobotState & state)
{
    // 数值仅在 VALID 时输出，其余一律 null，禁止用 0 伪装测量值。
    Json battery{
        {"status", to_string(state.battery.state.status)},
        {"received_age_ms", state.battery.state.received_age_ms},
        {"source_topic", "/lf/bmsstate"},
        {"quality_reason", state.battery.state.quality_reason},
        {"soc_percent", state.battery.state.status == DataStatus::VALID
            ? Json(state.battery.soc_percent) : Json(nullptr)},
        {"soh_percent", state.battery.state.status == DataStatus::VALID
            ? Json(state.battery.soh_percent) : Json(nullptr)},
        {"voltage_mv", (state.battery.state.status == DataStatus::VALID &&
                        state.battery.voltage_mv > 0)
            ? Json(state.battery.voltage_mv) : Json(nullptr)}
    };

    Json imu{
        {"status", to_string(state.imu.state.status)},
        {"received_age_ms", state.imu.state.received_age_ms},
        {"source_topic", "/lowstate"},
        {"quality_reason", state.imu.state.quality_reason},
        {"data_available", state.imu.state.status == DataStatus::VALID
            ? Json(state.imu.data_available) : Json(nullptr)}
    };

    Json runtime{
        {"status", to_string(state.runtime.state.status)},
        {"received_age_ms", state.runtime.state.received_age_ms},
        {"quality_reason", state.runtime.state.quality_reason},
        {"lowstate_available", state.runtime.state.status == DataStatus::VALID
            ? Json(state.runtime.lowstate_available) : Json(nullptr)},
        {"sportmode_available", state.runtime.state.status == DataStatus::VALID
            ? Json(state.runtime.sportmode_available) : Json(nullptr)}
    };

    Json action{
        {"status", to_string(state.action.state.status)},
        {"received_age_ms", state.action.state.received_age_ms},
        {"source_topic", "/arm/action/state"},
        {"quality_reason", state.action.state.quality_reason},
        {"is_executing", state.action.state.status == DataStatus::VALID
            ? Json(state.action.is_executing) : Json(nullptr)},
        {"action_id", state.action.state.status == DataStatus::VALID
            ? Json(state.action.action_id) : Json(nullptr)},
        {"action_name", state.action.state.status == DataStatus::VALID
            ? Json(state.action.action_name) : Json("")}
    };

    Json joints{
        {"status", to_string(state.joints.state.status)},
        {"received_age_ms", state.joints.state.received_age_ms},
        {"source_topic", "/lowstate"},
        {"quality_reason", state.joints.state.quality_reason},
        {"available_motor_count", state.joints.state.status == DataStatus::VALID
            ? Json(state.joints.available_motor_count) : Json(nullptr)},
        {"temperature_max_raw", state.joints.state.status == DataStatus::VALID
            ? Json(state.joints.temperature_max_raw) : Json(nullptr)},
        {"unit_confirmed", false}
    };

    const Json root = {
        {"schema_version", state.schema_version},
        {"battery", battery},
        {"imu", imu},
        {"runtime", runtime},
        {"action", action},
        {"joints", joints}
    };

    return root.dump();
}

bool RobotStateEvaluator::parse_robot_state_json(
    const std::string & json_text,
    AggregatedRobotState & state)
{
    Json root;

    try {
        root = Json::parse(json_text);
    } catch (const Json::exception &) {
        return false;
    }

    if (!root.is_object() ||
        !root.contains("schema_version") ||
        !root["schema_version"].is_number_integer() ||
        root["schema_version"].get<int>() != 1)
    {
        return false;
    }

    state = AggregatedRobotState();

    auto parse_field = [](const Json & section, FieldState & field) {
        if (!section.is_object()) {
            return;
        }
        const std::string status = section.value("status", std::string{"UNAVAILABLE"});
        if (status == "VALID") {
            field.status = DataStatus::VALID;
        } else if (status == "STALE") {
            field.status = DataStatus::STALE;
        } else if (status == "INVALID") {
            field.status = DataStatus::INVALID;
        } else {
            field.status = DataStatus::UNAVAILABLE;
        }
        field.received_age_ms = section.value("received_age_ms", -1L);
        field.quality_reason = section.value("quality_reason", std::string{});
    };

    if (root.contains("battery") && root["battery"].is_object()) {
        const Json & section = root["battery"];
        parse_field(section, state.battery.state);
        if (state.battery.state.status == DataStatus::VALID &&
            section.contains("soc_percent") &&
            section["soc_percent"].is_number())
        {
            state.battery.soc_percent = section["soc_percent"].get<int>();
        }
        if (state.battery.state.status == DataStatus::VALID &&
            section.contains("soh_percent") &&
            section["soh_percent"].is_number())
        {
            state.battery.soh_percent = section["soh_percent"].get<int>();
        }
        if (state.battery.state.status == DataStatus::VALID &&
            section.contains("voltage_mv") &&
            section["voltage_mv"].is_number())
        {
            state.battery.voltage_mv = section["voltage_mv"].get<long>();
        }
    }

    if (root.contains("imu") && root["imu"].is_object()) {
        const Json & section = root["imu"];
        parse_field(section, state.imu.state);
        if (state.imu.state.status == DataStatus::VALID &&
            section.contains("data_available") &&
            section["data_available"].is_boolean())
        {
            state.imu.data_available = section["data_available"].get<bool>();
        }
    }

    if (root.contains("runtime") && root["runtime"].is_object()) {
        const Json & section = root["runtime"];
        parse_field(section, state.runtime.state);
        if (state.runtime.state.status == DataStatus::VALID) {
            if (section.contains("lowstate_available") &&
                section["lowstate_available"].is_boolean())
            {
                state.runtime.lowstate_available =
                    section["lowstate_available"].get<bool>();
            }
            if (section.contains("sportmode_available") &&
                section["sportmode_available"].is_boolean())
            {
                state.runtime.sportmode_available =
                    section["sportmode_available"].get<bool>();
            }
        }
    }

    if (root.contains("action") && root["action"].is_object()) {
        const Json & section = root["action"];
        parse_field(section, state.action.state);
        if (state.action.state.status == DataStatus::VALID) {
            if (section.contains("is_executing") &&
                section["is_executing"].is_boolean())
            {
                state.action.is_executing = section["is_executing"].get<bool>();
            }
            if (section.contains("is_releasing") &&
                section["is_releasing"].is_boolean())
            {
                state.action.is_releasing = section["is_releasing"].get<bool>();
            }
            if (section.contains("action_id") &&
                section["action_id"].is_number_integer())
            {
                state.action.action_id = section["action_id"].get<int>();
            }
            state.action.action_name = section.value("action_name", std::string{});
        }
    }

    if (root.contains("joints") && root["joints"].is_object()) {
        const Json & section = root["joints"];
        parse_field(section, state.joints.state);
        state.joints.unit_confirmed =
            section.value("unit_confirmed", false);
        if (state.joints.state.status == DataStatus::VALID &&
            section.contains("available_motor_count") &&
            section["available_motor_count"].is_number())
        {
            state.joints.available_motor_count =
                section["available_motor_count"].get<int>();
        }
    }

    return true;
}

StatusQuestionType RobotStateEvaluator::classify_status_question(
    const std::string & user_text)
{
    const std::string text = normalize_question(user_text);

    if (contains_any(text, {"电量", "电池"})) {
        return StatusQuestionType::BATTERY;
    }

    if (contains_any(text, {"imu", "惯导", "陀螺仪", "姿态"})) {
        return StatusQuestionType::IMU;
    }

    if (contains_any(text, {"温度"})) {
        return StatusQuestionType::TEMPERATURE;
    }

    if (contains_any(text, {"动作", "执行", "挥手", "在做"})) {
        return StatusQuestionType::ACTION;
    }

    return StatusQuestionType::RUNTIME;
}

std::string RobotStateEvaluator::generate_status_answer(
    const AggregatedRobotState * state,
    const std::string & user_text)
{
    // Provider 未启动 / 缓存过期 / 解析失败：
    // 按问题子类型返回对应的“无法确认”安全回答（绝不进 Ollama）。
    if (state == nullptr) {
        switch (classify_status_question(user_text)) {
            case StatusQuestionType::BATTERY:
                return "我目前无法准确确认电池电量，请以实际设备显示为准。";
            case StatusQuestionType::IMU:
                return "我目前无法确认IMU数据状态。";
            case StatusQuestionType::ACTION:
                return "我目前无法准确确认当前动作执行状态。";
            case StatusQuestionType::TEMPERATURE:
                return "电机温度数据还没有经过确认的单位标准，"
                       "暂时无法准确回答，请以实际设备状态为准。";
            case StatusQuestionType::RUNTIME:
            default:
                return kDefaultStatusFallback;
        }
    }

    switch (classify_status_question(user_text)) {
        case StatusQuestionType::BATTERY: {
            if (state->battery.state.status == DataStatus::VALID &&
                state->battery.soc_percent >= 0)
            {
                return "我的电池电量大约是百分之" +
                       std::to_string(state->battery.soc_percent) + "。";
            }

            if (state->battery.state.status == DataStatus::STALE) {
                return "我的电池电量数据已经有一段时间没有更新了，"
                       "暂时无法准确确认，请以实际设备显示为准。";
            }

            return "我目前无法准确确认电池电量，请以实际设备显示为准。";
        }

        case StatusQuestionType::IMU: {
            if (state->imu.state.status == DataStatus::VALID) {
                return state->imu.data_available
                    ? "我目前能够持续接收到IMU数据，"
                      "但还没有完成完整的传感器健康诊断。"
                    : "我目前没有接收到有效的IMU数据。";
            }

            if (state->imu.state.status == DataStatus::STALE) {
                return "我的IMU数据已经有一段时间没有更新了，"
                       "暂时无法确认当前状态。";
            }

            return "我目前无法确认IMU数据状态。";
        }

        case StatusQuestionType::ACTION: {
            if (state->action.state.status == DataStatus::VALID) {
                if (state->action.is_releasing) {
                    return "动作已经完成，我正在放下手臂。";
                }
                if (state->action.is_executing) {
                    return "我正在执行" + state->action.action_name + "动作。";
                }
                return "我目前没有在执行动作，手臂处于空闲状态。";
            }

            if (state->action.state.status == DataStatus::STALE) {
                return "我的动作状态数据已经过期，暂时无法确认。";
            }

            return "我目前无法准确确认当前动作执行状态。";
        }

        case StatusQuestionType::TEMPERATURE:
            // 电机温度单位未确认、索引与关节映射未知，
            // 禁止播报任何具体温度数值。
            return "电机温度数据还没有经过确认的单位标准，"
                   "暂时无法准确回答，请以实际设备状态为准。";

        case StatusQuestionType::RUNTIME:
        default: {
            std::string answer;

            if (state->runtime.state.status == DataStatus::VALID) {
                answer += "我的核心状态数据正在持续接收。";
            }

            if (state->battery.state.status == DataStatus::VALID &&
                state->battery.soc_percent >= 0) {
                answer += "电池电量大约百分之" +
                          std::to_string(state->battery.soc_percent) + "。";
            }

            if (state->imu.state.status == DataStatus::VALID &&
                state->imu.data_available) {
                answer += "IMU数据接收正常。";
            }

            if (state->action.state.status == DataStatus::VALID) {
                answer += state->action.is_executing
                    ? "我正在执行" + state->action.action_name + "动作。"
                    : "当前没有在执行动作。";
            }

            if (answer.empty()) {
                return kDefaultStatusFallback;
            }

            return answer;
        }
    }
}

}  // namespace g1_voice_router
