#include <memory>

#include "rclcpp/rclcpp.hpp"
#include "g1_action_executor/arm_action_executor.hpp"

int main(int argc, char *argv[])
{
    rclcpp::init(argc, argv);

    auto node = std::make_shared<ArmActionExecutor>();

    RCLCPP_INFO(
        node->get_logger(),
        "Starting G1 Action Executor...");

    // 主线程负责 ROS 2 回调，包括 BaseClient 的响应回调。
    // 动作 API 同步调用由 ArmActionExecutor 内部工作线程执行。
    rclcpp::spin(node);

    rclcpp::shutdown();

    return 0;
}
