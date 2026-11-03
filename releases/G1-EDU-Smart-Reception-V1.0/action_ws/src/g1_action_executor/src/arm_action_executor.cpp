#include <functional>

#include "g1_action_executor/arm_action_executor.hpp"

#include <algorithm>
#include <cctype>
#include <exception>
#include <utility>

#include "nlohmann/json.hpp"

using namespace std::chrono_literals;

// ============================================================
// 构造函数
// ============================================================

ArmActionExecutor::ArmActionExecutor()
    : Node("g1_action_executor")
{
    // 1. 初始化 Unitree 官方动作客户端
    arm_client_ =
        std::make_shared<unitree::robot::g1::G1ArmActionClient>(this);

    // 2. 订阅动作请求
    action_request_sub_ =
        this->create_subscription<std_msgs::msg::String>(
            "/g1/action_request",
            rclcpp::QoS(10),
            std::bind(
                &ArmActionExecutor::on_action_request,
                this,
                std::placeholders::_1));

    // 3. 订阅手臂动作状态
    arm_state_sub_ =
        this->create_subscription<std_msgs::msg::String>(
            "/arm/action/state",
            rclcpp::QoS(1),
            std::bind(
                &ArmActionExecutor::on_arm_state,
                this,
                std::placeholders::_1));

    // 4. 订阅 FSM 状态
    sport_mode_sub_ =
        this->create_subscription<unitree_hg::msg::SportModeState>(
            "/sportmodestate",
            rclcpp::QoS(10),
            std::bind(
                &ArmActionExecutor::on_sport_mode_state,
                this,
                std::placeholders::_1));

    // 5. 订阅机器人低状态
    // LowState 使用 Best Effort，因此采用 SensorDataQoS。
    low_state_sub_ =
        this->create_subscription<unitree_hg::msg::LowState>(
            "/lowstate",
            rclcpp::SensorDataQoS(),
            std::bind(
                &ArmActionExecutor::on_low_state,
                this,
                std::placeholders::_1));

    // 6. 发布动作执行结果
    action_result_pub_ =
        this->create_publisher<std_msgs::msg::String>(
            "/g1/action_result",
            rclcpp::QoS(10));

    // 7. 启动动作工作线程
    worker_thread_ =
        std::thread(&ArmActionExecutor::worker_loop, this);

    RCLCPP_INFO(
        this->get_logger(),
        "G1 Action Executor initialized.");

    RCLCPP_INFO(
        this->get_logger(),
        "Request topic: /g1/action_request");

    RCLCPP_INFO(
        this->get_logger(),
        "Supported commands:");

    RCLCPP_INFO(
        this->get_logger(),
        "wave, wave_low, hands_up, clap, victory, heart");
}


// ============================================================
// 析构函数
// ============================================================

ArmActionExecutor::~ArmActionExecutor()
{
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);

        worker_stopping_ = true;
        action_queue_.clear();
    }

    queue_cv_.notify_all();
    arm_state_cv_.notify_all();

    if (worker_thread_.joinable())
    {
        worker_thread_.join();
    }

    RCLCPP_INFO(
        this->get_logger(),
        "G1 Action Executor stopped.");
}


// ============================================================
// 命令格式归一化
// ============================================================

std::string ArmActionExecutor::normalize_command(
    const std::string &input)
{
    std::string result = input;

    const auto begin =
        result.find_first_not_of(" \t\r\n");

    if (begin == std::string::npos)
    {
        return "";
    }

    const auto end =
        result.find_last_not_of(" \t\r\n");

    result = result.substr(begin, end - begin + 1);

    std::transform(
        result.begin(),
        result.end(),
        result.begin(),
        [](unsigned char c)
        {
            return static_cast<char>(std::tolower(c));
        });

    return result;
}


// ============================================================
// 发布执行结果
// ============================================================

void ArmActionExecutor::publish_result(
    const std::string &result)
{
    std_msgs::msg::String msg;
    msg.data = result;

    action_result_pub_->publish(msg);

    RCLCPP_INFO(
        this->get_logger(),
        "[Action Result] %s",
        result.c_str());
}


// ============================================================
// 更新动作忙碌状态
// ============================================================

void ArmActionExecutor::set_action_busy(bool busy)
{
    std::lock_guard<std::mutex> lock(queue_mutex_);

    action_busy_ = busy;
}


// ============================================================
// ROS 2 请求回调：接收动作命令
// ============================================================

void ArmActionExecutor::on_action_request(
    const std_msgs::msg::String::SharedPtr msg)
{
    const std::string command =
        normalize_command(msg->data);

    if (command.empty())
    {
        publish_result("rejected: empty command");
        return;
    }

    ActionCommand action;

    // --------------------------------------------------------
    // 白名单映射
    // --------------------------------------------------------

    if (command == "wave")
    {
        action = {"wave", 26, false};
    }
    else if (command == "wave_low")
    {
        action = {"wave_low", 25, false};
    }
    else if (command == "hands_up")
    {
        action = {"hands_up", 15, false};
    }
    else if (command == "clap")
    {
        // 固件列表中的名称是 clamp，ID 为 17。
        // 对外使用更直观的命令 clap。
        action = {"clap", 17, false};
    }
    else if (command == "victory")
    {
        // 固件要求 mode_machine 为 5 或 6。
        action = {"victory", 30, true};
    }
    else if (command == "heart")
    {
        // 固件要求 mode_machine 为 5 或 6。
        action = {"heart", 20, true};
    }
    else
    {
        publish_result(
            "rejected: unknown command: " + command);

        RCLCPP_WARN(
            this->get_logger(),
            "Rejected unknown action command: %s",
            command.c_str());

        return;
    }

    // --------------------------------------------------------
    // 检查 FSM
    // --------------------------------------------------------

    if (!fsm_received_.load())
    {
        publish_result("rejected: FSM state not received");
        return;
    }

    const int32_t current_fsm =
        fsm_id_.load();

    const int32_t current_fsm_mode =
        fsm_mode_.load();

    if (current_fsm != 500 &&
        current_fsm != 501 &&
        current_fsm != 801)
    {
        publish_result(
            "rejected: unsupported FSM: " +
            std::to_string(current_fsm));

        return;
    }

    if (current_fsm == 801 &&
        current_fsm_mode != 0 &&
        current_fsm_mode != 3)
    {
        publish_result(
            "rejected: unsupported FSM mode: " +
            std::to_string(current_fsm_mode));

        return;
    }

    // --------------------------------------------------------
    // 检查硬件模式
    // --------------------------------------------------------

    if (action.requires_mode_machine)
    {
        if (!low_state_received_.load())
        {
            publish_result(
                "rejected: LowState not received");

            return;
        }

        const int32_t current_mode_machine =
            mode_machine_.load();

        if (current_mode_machine != 5 &&
            current_mode_machine != 6)
        {
            publish_result(
                "rejected: unsupported mode_machine: " +
                std::to_string(current_mode_machine));

            return;
        }
    }

    // --------------------------------------------------------
    // 检查手臂状态
    // --------------------------------------------------------

    if (!arm_state_received_.load())
    {
        publish_result("rejected: arm state not received");
        return;
    }

    if (arm_holding_.load())
    {
        publish_result(
            "rejected: arm is holding, manual inspection required");

        return;
    }

    const int32_t current_action_id =
        arm_action_id_.load();

    // 必须确认当前手臂处于真实空闲状态
    if (current_action_id != 0)
    {
        publish_result(
            "rejected: arm is not idle, current ID=" +
            std::to_string(current_action_id));

        return;
    }

    // --------------------------------------------------------
    // 检查队列、忙碌状态和冷却时间
    // --------------------------------------------------------

    std::string rejection;

    {
        std::lock_guard<std::mutex> lock(queue_mutex_);

        if (worker_stopping_)
        {
            rejection = "rejected: executor is stopping";
        }
        else if (action_busy_ || !action_queue_.empty())
        {
            rejection = "rejected: another action is running";
        }
        else
        {
            const auto now = SteadyClock::now();

            if (last_accepted_time_.time_since_epoch().count() != 0 &&
                now - last_accepted_time_ < cooldown_)
            {
                rejection = "rejected: action cooldown";
            }
            else
            {
                action_busy_ = true;

                last_accepted_time_ = now;

                action_queue_.push_back(action);
            }
        }
    }

    if (!rejection.empty())
    {
        publish_result(rejection);
        return;
    }

    RCLCPP_INFO(
        this->get_logger(),
        "Accepted action: %s, ID=%d, FSM=%d, mode_machine=%d",
        action.name.c_str(),
        action.action_id,
        current_fsm,
        mode_machine_.load());

    publish_result(
        "accepted:" + action.name +
        ":" + std::to_string(action.action_id));

    queue_cv_.notify_one();
}


// ============================================================
// ROS 2 回调：读取手臂动作状态
// ============================================================

void ArmActionExecutor::on_arm_state(
    const std_msgs::msg::String::SharedPtr msg)
{
    try
    {
        const auto state =
            nlohmann::json::parse(msg->data);

        // 默认值采用保守策略：
        // 如果字段缺失，不认为机器人处于安全空闲状态。
        const bool holding =
            state.value("holding", true);

        const int32_t action_id =
            state.value("id", -1);

        const std::string action_name =
            state.value("name", std::string(""));

        {
            std::lock_guard<std::mutex> lock(arm_state_wait_mutex_);

            arm_holding_.store(holding);
            arm_action_id_.store(action_id);
            arm_state_received_.store(true);

            const int32_t expected_id =
                expected_action_id_.load();

            // 只认当前目标动作，或官方释放状态 ID 99。
            if (expected_id > 0 &&
                (action_id == expected_id || action_id == 99))
            {
                action_state_active_seen_.store(true);
            }
        }

        arm_state_cv_.notify_all();

        const std::string summary =
            "id=" + std::to_string(action_id) +
            ", name=" + action_name +
            ", holding=" + (holding ? "true" : "false");

        if (summary != last_arm_state_)
        {
            last_arm_state_ = summary;

            RCLCPP_INFO(
                this->get_logger(),
                "[Arm State] %s",
                summary.c_str());
        }
    }
    catch (const nlohmann::json::exception &e)
    {
        RCLCPP_WARN(
            this->get_logger(),
            "Failed to parse arm state: %s",
            e.what());
    }
}


// ============================================================
// ROS 2 回调：读取 FSM
// ============================================================

void ArmActionExecutor::on_sport_mode_state(
    const unitree_hg::msg::SportModeState::SharedPtr msg)
{
    fsm_id_.store(static_cast<int32_t>(msg->fsm_id));
    fsm_mode_.store(static_cast<int32_t>(msg->fsm_mode));

    fsm_received_.store(true);
}


// ============================================================
// ROS 2 回调：读取 LowState 硬件模式
// ============================================================

void ArmActionExecutor::on_low_state(
    const unitree_hg::msg::LowState::SharedPtr msg)
{
    mode_machine_.store(
        static_cast<int32_t>(msg->mode_machine));

    low_state_received_.store(true);
}


// ============================================================
// 等待动作开始
// ============================================================

bool ArmActionExecutor::wait_for_action_start(
    std::chrono::seconds timeout)
{
    std::unique_lock<std::mutex> lock(arm_state_wait_mutex_);

    return arm_state_cv_.wait_for(
        lock,
        timeout,
        [this]()
        {
            return action_state_active_seen_.load();
        });
}


// ============================================================
// 等待动作结束并回到空闲状态
// ============================================================

bool ArmActionExecutor::wait_for_action_idle(
    std::chrono::seconds timeout)
{
    std::unique_lock<std::mutex> lock(arm_state_wait_mutex_);

    return arm_state_cv_.wait_for(
        lock,
        timeout,
        [this]()
        {
            return action_state_active_seen_.load() &&
                   arm_state_received_.load() &&
                   arm_action_id_.load() == 0 &&
                   !arm_holding_.load();
        });
}


// ============================================================
// 工作线程：执行真实动作
// ============================================================

void ArmActionExecutor::worker_loop()
{
    while (true)
    {
        ActionCommand action;

        {
            std::unique_lock<std::mutex> lock(queue_mutex_);

            queue_cv_.wait(
                lock,
                [this]()
                {
                    return worker_stopping_ ||
                           !action_queue_.empty();
                });

            if (worker_stopping_)
            {
                break;
            }

            action = action_queue_.front();
            action_queue_.pop_front();
        }

        RCLCPP_INFO(
            this->get_logger(),
            "[Worker] Executing %s, ID=%d",
            action.name.c_str(),
            action.action_id);

        // 每次动作开始前重置状态观察标记。
        {
            std::lock_guard<std::mutex> lock(arm_state_wait_mutex_);

            expected_action_id_.store(action.action_id);
            action_state_active_seen_.store(false);
        }

        int32_t ret = -2;

        try
        {
            // 同步 RPC 放在独立工作线程。
            // ROS 2 主线程继续处理 Unitree API 响应和状态回调。
            ret = arm_client_->ExecuteAction(action.action_id);
        }
        catch (const std::exception &e)
        {
            RCLCPP_ERROR(
                this->get_logger(),
                "Action API exception: %s",
                e.what());
        }

        RCLCPP_INFO(
            this->get_logger(),
            "[Worker] RPC returned: %d",
            ret);

        // ----------------------------------------------------
        // 等待目标动作真正开始
        // ----------------------------------------------------

        const bool started =
            wait_for_action_start(action_start_timeout_);

        if (!started)
        {
            const int32_t current_id =
                arm_action_id_.load();

            const bool holding =
                arm_holding_.load();

            if (ret != 0 && current_id == 0 && !holding)
            {
                publish_result(
                    "failed:" + action.name +
                    ":" + std::to_string(action.action_id) +
                    ":rpc_error=" + std::to_string(ret));

                expected_action_id_.store(-1);

                set_action_busy(false);
            }
            else
            {
                // RPC 成功却没有观察到动作开始，
                // 或者当前状态仍然不空闲。
                // 保持 busy 锁定，禁止继续发送动作。
                publish_result(
                    "completion_unknown:" + action.name +
                    ":start_not_observed:state_id=" +
                    std::to_string(current_id));

                RCLCPP_ERROR(
                    this->get_logger(),
                    "Action start not observed. Executor remains locked.");

                expected_action_id_.store(-1);
            }

            continue;
        }

        // ----------------------------------------------------
        // 已观察到动作开始，等待结束
        // ----------------------------------------------------

        const bool completed =
            wait_for_action_idle(action_completion_timeout_);

        if (completed)
        {
            RCLCPP_INFO(
                this->get_logger(),
                "Action reached idle; waiting %ld seconds for settling.",
                static_cast<long>(post_action_settle_.count()));

            // 动作已经报告空闲，但先不释放 busy。
            std::this_thread::sleep_for(post_action_settle_);

            // 缓冲期间如果再次出现活动状态，保持执行器锁定。
            if (arm_action_id_.load() != 0 || arm_holding_.load())
            {
                publish_result(
                    "completion_unknown:" + action.name +
                    ":state_changed_during_settle:id=" +
                    std::to_string(arm_action_id_.load()) +
                    ":holding=" +
                    (arm_holding_.load() ? "true" : "false"));

                expected_action_id_.store(-1);

                RCLCPP_ERROR(
                    this->get_logger(),
                    "Arm state changed during settling. "
                    "Executor remains locked.");

                continue;
            }

            if (ret == 0)
            {
                publish_result(
                    "success:" + action.name +
                    ":" + std::to_string(action.action_id));
            }
            else
            {
                // RPC 报错，但状态话题证明动作已经完成。
                publish_result(
                    "completed_with_rpc_error:" + action.name +
                    ":" + std::to_string(action.action_id) +
                    ":rpc_error=" + std::to_string(ret));
            }

            expected_action_id_.store(-1);

            set_action_busy(false);
        }
        else
        {
            const int32_t current_id =
                arm_action_id_.load();

            const bool holding =
                arm_holding_.load();

            publish_result(
                "completion_unknown:" + action.name +
                ":timeout:state_id=" +
                std::to_string(current_id) +
                ":holding=" +
                (holding ? "true" : "false"));

            RCLCPP_ERROR(
                this->get_logger(),
                "Action completion not confirmed. "
                "Executor remains locked. Check robot state manually.");

            // 不清除 action_busy_。
            // 必须由操作员确认机器人状态后再恢复执行。

            expected_action_id_.store(-1);
        }
    }
}
