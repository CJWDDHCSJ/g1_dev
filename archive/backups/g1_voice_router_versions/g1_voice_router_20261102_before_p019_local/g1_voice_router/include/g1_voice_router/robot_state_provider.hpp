#ifndef G1_VOICE_ROUTER_ROBOT_STATE_PROVIDER_HPP_
#define G1_VOICE_ROUTER_ROBOT_STATE_PROVIDER_HPP_

#include <mutex>
#include <string>

#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/string.hpp>
#include <unitree_hg/msg/bms_state.hpp>
#include <unitree_hg/msg/low_state.hpp>
#include <unitree_hg/msg/sport_mode_state.hpp>

#include "g1_voice_router/robot_state_evaluator.hpp"

namespace g1_voice_router
{

// RobotStateProvider 节点：
// 只订阅机器人原生状态话题，更新缓存并低频发布聚合 JSON；
// 不发布任何控制指令，不访问 Ollama，不依赖大模型。
class RobotStateProvider : public rclcpp::Node
{
public:
    RobotStateProvider();

private:
    // 计时统一使用 steady_clock（单调，不受系统时间跳变影响）。
    static long steady_now_ms();

    void on_bms_state(
        const unitree_hg::msg::BmsState::SharedPtr msg);

    void on_low_state(
        const unitree_hg::msg::LowState::SharedPtr msg);

    void on_sport_mode_state(
        const unitree_hg::msg::SportModeState::SharedPtr msg);

    void on_arm_action_state(
        const std_msgs::msg::String::SharedPtr msg);

    void on_publish_timer();

    RobotStateEvaluator evaluator_;
    std::mutex evaluator_mutex_;

    rclcpp::Subscription<unitree_hg::msg::BmsState>::SharedPtr
        bms_subscription_;
    rclcpp::Subscription<unitree_hg::msg::LowState>::SharedPtr
        low_state_subscription_;
    rclcpp::Subscription<unitree_hg::msg::SportModeState>::SharedPtr
        sport_mode_subscription_;
    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr
        arm_action_subscription_;

    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr state_publisher_;
    rclcpp::TimerBase::SharedPtr publish_timer_;
};

}  // namespace g1_voice_router

#endif  // G1_VOICE_ROUTER_ROBOT_STATE_PROVIDER_HPP_
