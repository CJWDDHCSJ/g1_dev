#ifndef G1_ACTION_EXECUTOR_ARM_ACTION_EXECUTOR_HPP_
#define G1_ACTION_EXECUTOR_ARM_ACTION_EXECUTOR_HPP_

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "g1/g1_arm_action_client.hpp"

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"
#include "unitree_hg/msg/low_state.hpp"
#include "unitree_hg/msg/sport_mode_state.hpp"

class ArmActionExecutor : public rclcpp::Node
{
public:
    ArmActionExecutor();
    ~ArmActionExecutor() override;

private:
    // ========================================================
    // 动作命令
    // ========================================================

    struct ActionCommand
    {
        std::string name;
        int32_t action_id{0};

        // 是否要求检查 mode_machine
        bool requires_mode_machine{false};
    };

    // ROS 2 请求回调
    void on_action_request(
        const std_msgs::msg::String::SharedPtr msg);

    // 手臂动作状态
    void on_arm_state(
        const std_msgs::msg::String::SharedPtr msg);

    // FSM 状态
    void on_sport_mode_state(
        const unitree_hg::msg::SportModeState::SharedPtr msg);

    // 机器人低状态
    void on_low_state(
        const unitree_hg::msg::LowState::SharedPtr msg);

    // ========================================================
    // 工作线程
    // ========================================================

    void worker_loop();

    bool wait_for_action_start(
        std::chrono::seconds timeout);

    bool wait_for_action_idle(
        std::chrono::seconds timeout);

    void set_action_busy(bool busy);

    // ========================================================
    // 工具函数
    // ========================================================

    static std::string normalize_command(
        const std::string &input);

    void publish_result(
        const std::string &result);

    // ========================================================
    // Unitree 官方 ROS 2 动作客户端
    // ========================================================

    std::shared_ptr<unitree::robot::g1::G1ArmActionClient>
        arm_client_;

    // ========================================================
    // ROS 2 Subscribers
    // ========================================================

    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr
        action_request_sub_;

    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr
        arm_state_sub_;

    rclcpp::Subscription<unitree_hg::msg::SportModeState>::SharedPtr
        sport_mode_sub_;

    rclcpp::Subscription<unitree_hg::msg::LowState>::SharedPtr
        low_state_sub_;

    // ========================================================
    // ROS 2 Publisher
    // ========================================================

    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr
        action_result_pub_;

    // ========================================================
    // FSM 状态
    // ========================================================

    std::atomic<int32_t> fsm_id_{0};
    std::atomic<int32_t> fsm_mode_{0};
    std::atomic<bool> fsm_received_{false};

    // ========================================================
    // 机器人硬件模式
    // ========================================================

    std::atomic<int32_t> mode_machine_{-1};
    std::atomic<bool> low_state_received_{false};

    // ========================================================
    // 手臂动作状态
    // ========================================================

    std::atomic<bool> arm_state_received_{false};

    std::atomic<bool> arm_holding_{true};

    // 当前动作 ID
    // -1 表示尚未收到有效状态
    // 0 表示空闲
    // 99 表示释放动作
    std::atomic<int32_t> arm_action_id_{-1};

    std::string last_arm_state_;

    // ========================================================
    // 动作开始 / 完成检测
    // ========================================================

    // 当前正在等待的目标动作 ID
    std::atomic<int32_t> expected_action_id_{-1};

    // 是否观察到了目标动作或 ID 99
    std::atomic<bool> action_state_active_seen_{false};

    // 用于工作线程等待动作状态变化
    std::mutex arm_state_wait_mutex_;
    std::condition_variable arm_state_cv_;

    // ========================================================
    // 动作任务队列
    // ========================================================

    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;

    std::deque<ActionCommand> action_queue_;

    bool worker_stopping_{false};

    // 由 queue_mutex_ 保护
    bool action_busy_{false};

    // ========================================================
    // 时间控制
    // ========================================================

    using SteadyClock = std::chrono::steady_clock;

    SteadyClock::time_point last_accepted_time_{};

    const std::chrono::seconds cooldown_{3};

    // 等待动作状态开始的最长时间
    const std::chrono::seconds action_start_timeout_{5};

    // 等待动作返回空闲的最长时间
    const std::chrono::seconds action_completion_timeout_{30};
    const std::chrono::seconds post_action_settle_{3};

    std::thread worker_thread_;
};

#endif
