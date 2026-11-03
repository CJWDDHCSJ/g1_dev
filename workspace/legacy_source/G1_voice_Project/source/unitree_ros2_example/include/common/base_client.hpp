#pragma once

#include <chrono>
#include <cstdint>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>

#include <rclcpp/rclcpp.hpp>

#include "nlohmann/json.hpp"
#include "time_tools.hpp"
#include "unitree_api/msg/request.hpp"
#include "unitree_api/msg/response.hpp"
#include "ut_errror.hpp"

class BaseClient {
    using Request = unitree_api::msg::Request;
    using Response = unitree_api::msg::Response;

    using IdentityId =
        decltype(std::declval<Request>().header.identity.id);

    using PromisePtr =
        std::shared_ptr<
            std::promise<std::shared_ptr<const Response>>>;

    rclcpp::Node* node_;
    std::string topic_name_request_;
    std::string topic_name_response_;

    rclcpp::Publisher<Request>::SharedPtr req_puber_;
    rclcpp::Subscription<Response>::SharedPtr response_suber_;

    std::mutex pending_mutex_;
    std::unordered_map<IdentityId, PromisePtr> pending_calls_;

public:
    BaseClient(
        rclcpp::Node* node,
        const std::string& topic_name_request,
        std::string topic_name_response)
        : node_(node),
          topic_name_request_(topic_name_request),
          topic_name_response_(std::move(topic_name_response)),
          req_puber_(
              node_->create_publisher<Request>(
                  topic_name_request_,
                  rclcpp::QoS(1))) {

        // 响应订阅只创建一次，不再为每次请求创建临时订阅。
        response_suber_ =
            node_->create_subscription<Response>(
                topic_name_response_,
                rclcpp::QoS(1),
                [this](const std::shared_ptr<const Response> data) {
                    if (!data) {
                        return;
                    }

                    PromisePtr promise;

                    {
                        std::lock_guard<std::mutex> lock(pending_mutex_);

                        const auto it =
                            pending_calls_.find(
                                data->header.identity.id);

                        // 只处理当前客户端正在等待的请求。
                        if (it == pending_calls_.end()) {
                            return;
                        }

                        promise = it->second;
                        pending_calls_.erase(it);
                    }

                    try {
                        promise->set_value(data);
                    } catch (const std::future_error& e) {
                        std::cerr
                            << "Failed to deliver response: "
                            << e.what()
                            << std::endl;
                    }
                });
    }

    int32_t Call(Request req, nlohmann::json& js) {
        req.header.identity.id =
            unitree::common::GetSystemUptimeInNanoseconds();

        const IdentityId identity_id =
            req.header.identity.id;

        auto promise =
            std::make_shared<
                std::promise<std::shared_ptr<const Response>>>();

        auto response_future = promise->get_future();

        // 必须先登记待处理请求，再发布消息，避免响应先到而无法匹配。
        {
            std::lock_guard<std::mutex> lock(pending_mutex_);
            pending_calls_[identity_id] = promise;
        }

        req_puber_->publish(req);

        const auto status =
            response_future.wait_for(std::chrono::seconds(5));

        if (status != std::future_status::ready) {
            {
                std::lock_guard<std::mutex> lock(pending_mutex_);
                pending_calls_.erase(identity_id);
            }

            if (status == std::future_status::timeout) {
                return UT_ROBOT_TASK_TIMEOUT;
            }

            return UT_ROBOT_TASK_UNKNOWN_ERROR;
        }

        const auto response = response_future.get();

        if (!response) {
            return UT_ROBOT_TASK_UNKNOWN_ERROR;
        }

        if (response->header.status.code != 0) {
            std::cout
                << "error code: "
                << response->header.status.code
                << std::endl;

            return response->header.status.code;
        }

        try {
            js = nlohmann::json::parse(response->data.data());
        } catch (const nlohmann::detail::exception& e) {
            std::cerr
                << "Failed to parse response JSON: "
                << e.what()
                << std::endl;
        }

        return UT_ROBOT_SUCCESS;
    }

    int32_t Call(Request req) {
        nlohmann::json js;
        return Call(std::move(req), js);
    }
};
