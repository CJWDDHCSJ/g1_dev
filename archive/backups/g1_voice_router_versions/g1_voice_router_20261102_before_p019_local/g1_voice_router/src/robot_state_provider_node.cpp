#include <rclcpp/rclcpp.hpp>

#include "g1_voice_router/robot_state_provider.hpp"

int main(int argc, char ** argv)
{
    rclcpp::init(argc, argv);

    try {
        auto node =
            std::make_shared<g1_voice_router::RobotStateProvider>();

        rclcpp::spin(node);
    } catch (const std::exception & error) {
        RCLCPP_ERROR(
            rclcpp::get_logger("g1_robot_state_provider"),
            "Fatal error: %s",
            error.what());
    }

    rclcpp::shutdown();
    return 0;
}
