#include "g1_voice_router/robot_state_provider.hpp"

#include <algorithm>

namespace g1_voice_router
{

long RobotStateProvider::steady_now_ms()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

RobotStateProvider::RobotStateProvider()
: Node("g1_robot_state_provider")
{
    // 原生 Publisher 为 RELIABLE/VOLATILE（P0-17 实测），
    // 订阅端 BEST_EFFORT 与其兼容且不增加底层负担。
    const auto best_effort = rclcpp::QoS(rclcpp::KeepLast(1))
        .best_effort();

    // 1kHz 话题（/lowstate）回调只做轻量缓存更新：
    // 遍历 motor_state 统计有效通道与温度最大值，不拷贝大数组、不做序列化。
    low_state_subscription_ =
        create_subscription<unitree_hg::msg::LowState>(
            "/lowstate",
            best_effort,
            [this](const unitree_hg::msg::LowState::SharedPtr msg) {
                on_low_state(msg);
            });

    bms_subscription_ =
        create_subscription<unitree_hg::msg::BmsState>(
            "/lf/bmsstate",
            best_effort,
            [this](const unitree_hg::msg::BmsState::SharedPtr msg) {
                on_bms_state(msg);
            });

    sport_mode_subscription_ =
        create_subscription<unitree_hg::msg::SportModeState>(
            "/sportmodestate",
            best_effort,
            [this](const unitree_hg::msg::SportModeState::SharedPtr msg) {
                on_sport_mode_state(msg);
            });

    arm_action_subscription_ =
        create_subscription<std_msgs::msg::String>(
            "/arm/action/state",
            best_effort,
            [this](const std_msgs::msg::String::SharedPtr msg) {
                on_arm_action_state(msg);
            });

    state_publisher_ =
        create_publisher<std_msgs::msg::String>("/g1/robot_state", 10);

    // 2Hz 聚合发布：远低于原生话题频率，避免高频 JSON 序列化。
    publish_timer_ = create_wall_timer(
        std::chrono::milliseconds(500),
        [this]() {
            on_publish_timer();
        });

    RCLCPP_INFO(
        get_logger(),
        "RobotStateProvider started: "
        "subscribing /lf/bmsstate /lowstate /sportmodestate /arm/action/state, "
        "publishing /g1/robot_state at 2 Hz");
}

void RobotStateProvider::on_bms_state(
    const unitree_hg::msg::BmsState::SharedPtr msg)
{
    const long now_ms = steady_now_ms();

    std::lock_guard<std::mutex> lock(evaluator_mutex_);
    evaluator_.update_bms(
        static_cast<int>(msg->soc),
        static_cast<int>(msg->soh),
        true,
        msg->bmsvoltage.empty() ? -1L
            : static_cast<long>(msg->bmsvoltage[0]),
        now_ms);
}

void RobotStateProvider::on_low_state(
    const unitree_hg::msg::LowState::SharedPtr msg)
{
    const long now_ms = steady_now_ms();

    // motor_state 共 35 路（P0-17 实测 29 路有效）；
    // 温度取全部通道原始最大值，单位未确认，仅内部诊断。
    int available_count = 0;
    int temp_max = -1;

    for (const auto & motor : msg->motor_state) {
        if (std::abs(motor.q) > 1e-4 || motor.temperature[0] != 0 ||
            motor.temperature[1] != 0)
        {
            ++available_count;
        }

        temp_max = std::max({temp_max,
            static_cast<int>(motor.temperature[0]),
            static_cast<int>(motor.temperature[1])});
    }

    const bool imu_ok =
        msg->imu_state.accelerometer.size() == 3 &&
        msg->imu_state.quaternion.size() == 4;

    std::lock_guard<std::mutex> lock(evaluator_mutex_);
    evaluator_.update_lowstate(imu_ok, available_count, temp_max, now_ms);
    evaluator_.update_sportmode(now_ms);
}

void RobotStateProvider::on_sport_mode_state(
    const unitree_hg::msg::SportModeState::SharedPtr msg)
{
    // SportModeState 的 FSM 数值仅记录用途，不做动作语义推断；
    // 这里只用于 runtime.available 的数据接收判断。
    const long now_ms = steady_now_ms();

    std::lock_guard<std::mutex> lock(evaluator_mutex_);
    evaluator_.update_sportmode(now_ms);
    (void)msg;
}

void RobotStateProvider::on_arm_action_state(
    const std_msgs::msg::String::SharedPtr msg)
{
    const long now_ms = steady_now_ms();

    std::lock_guard<std::mutex> lock(evaluator_mutex_);
    evaluator_.update_action(msg->data, now_ms);
}

void RobotStateProvider::on_publish_timer()
{
    const long now_ms = steady_now_ms();

    AggregatedRobotState state;

    {
        std::lock_guard<std::mutex> lock(evaluator_mutex_);
        state = evaluator_.evaluate(now_ms);
    }

    std_msgs::msg::String output;
    output.data = RobotStateEvaluator::to_json(state);

    state_publisher_->publish(output);
}

}  // namespace g1_voice_router
