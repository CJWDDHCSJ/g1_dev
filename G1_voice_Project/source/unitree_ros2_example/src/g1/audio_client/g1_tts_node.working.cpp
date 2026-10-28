#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/string.hpp>

#include "g1/g1_audio_client.hpp"
#include "nlohmann/json.hpp"

class G1TtsNode : public rclcpp::Node {
public:
    explicit G1TtsNode(
        const std::shared_ptr<unitree::ros2::g1::AudioClient>& audio_client,
        const std::string& config_path)
        : Node("g1_tts_node"),
          audio_client_(audio_client) {

        if (!audio_client_) {
            throw std::invalid_argument("AudioClient must not be null.");
        }

        load_config(config_path);

        subscription_ = this->create_subscription<std_msgs::msg::String>(
            "/g1/tts",
            rclcpp::QoS(10),
            [this](const std_msgs::msg::String::SharedPtr msg) {
                enqueue_text(msg->data, "ROS 2 topic");
            });

        RCLCPP_INFO(
            this->get_logger(),
            "G1 TTS node initialized. Listening on /g1/tts");
    }

    void start() {
        {
            std::lock_guard<std::mutex> lock(mutex_);

            if (started_) {
                RCLCPP_WARN(
                    this->get_logger(),
                    "G1 TTS node has already been started.");
                return;
            }

            if (stopping_) {
                RCLCPP_WARN(
                    this->get_logger(),
                    "Cannot start G1 TTS node after stopping.");
                return;
            }

            started_ = true;
        }

        worker_ = std::thread(&G1TtsNode::worker_loop, this);

        RCLCPP_INFO(
            this->get_logger(),
            "TTS worker started.");

        if (!config_text_.empty()) {
            enqueue_text(config_text_, "JSON config");
        } else {
            RCLCPP_WARN(
                this->get_logger(),
                "The text field in JSON is empty. No startup speech queued.");
        }
    }

    void stop() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stopping_ = true;

            std::queue<std::string> empty;
            queue_.swap(empty);
        }

        condition_.notify_all();

        if (worker_.joinable()) {
            worker_.join();
        }

        RCLCPP_INFO(
            this->get_logger(),
            "G1 TTS worker stopped.");
    }

    ~G1TtsNode() override {
        stop();
    }

private:
    void load_config(const std::string& config_path) {
        std::ifstream input(config_path);

        if (!input.is_open()) {
            throw std::runtime_error(
                "Cannot open TTS config file: " + config_path);
        }

        nlohmann::json config;
        input >> config;

        if (!config.is_object()) {
            throw std::runtime_error(
                "TTS config must be a JSON object.");
        }

        if (!config.contains("speaker_id") ||
            !config["speaker_id"].is_number_integer()) {
            throw std::runtime_error(
                "JSON field 'speaker_id' must be an integer.");
        }

        if (!config.contains("volume") ||
            !config["volume"].is_number_integer()) {
            throw std::runtime_error(
                "JSON field 'volume' must be an integer.");
        }

        if (!config.contains("text") ||
            !config["text"].is_string()) {
            throw std::runtime_error(
                "JSON field 'text' must be a string.");
        }

        speaker_id_ = config["speaker_id"].get<int>();
        volume_ = config["volume"].get<int>();
        config_text_ = config["text"].get<std::string>();

        if (speaker_id_ < 0) {
            throw std::runtime_error(
                "speaker_id cannot be negative.");
        }

        if (volume_ < 0 || volume_ > 100) {
            throw std::runtime_error(
                "volume must be between 0 and 100.");
        }

        RCLCPP_INFO(
            this->get_logger(),
            "Loaded TTS config: %s",
            config_path.c_str());

        RCLCPP_INFO(
            this->get_logger(),
            "speaker_id=%d, configured volume=%d",
            speaker_id_, volume_);

        RCLCPP_INFO(
            this->get_logger(),
            "Startup text: %s",
            config_text_.c_str());

        RCLCPP_WARN(
            this->get_logger(),
            "Volume is read from JSON but not applied yet.");
    }

    void enqueue_text(const std::string& text, const char* source) {
        if (text.empty()) {
            RCLCPP_WARN(
                this->get_logger(),
                "Received empty text from %s; ignoring.",
                source);
            return;
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);

            if (stopping_) {
                return;
            }

            constexpr std::size_t MAX_QUEUE_SIZE = 10;

            if (queue_.size() >= MAX_QUEUE_SIZE) {
                RCLCPP_WARN(
                    this->get_logger(),
                    "TTS queue full. Dropping newest message.");
                return;
            }

            queue_.push(text);
        }

        condition_.notify_one();

        RCLCPP_INFO(
            this->get_logger(),
            "Text queued from %s.",
            source);
    }

    void worker_loop() {
        while (true) {
            std::string text;

            {
                std::unique_lock<std::mutex> lock(mutex_);

                condition_.wait(lock, [this]() {
                    return stopping_ || !queue_.empty();
                });

                if (stopping_) {
                    return;
                }

                text = std::move(queue_.front());
                queue_.pop();
            }

            RCLCPP_INFO(
                this->get_logger(),
                "Calling TTS API: speaker_id=%d, text=%s",
                speaker_id_, text.c_str());

            const int32_t ret =
                audio_client_->TtsMaker(text, speaker_id_);

            RCLCPP_INFO(
                this->get_logger(),
                "TtsMaker returned ret=%d",
                ret);

            if (ret == 0) {
                RCLCPP_INFO(
                    this->get_logger(),
                    "TtsMaker API call succeeded.");
            } else {
                RCLCPP_ERROR(
                    this->get_logger(),
                    "TtsMaker failed. ret=%d",
                    ret);
            }
        }
    }

    std::shared_ptr<unitree::ros2::g1::AudioClient> audio_client_;

    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr subscription_;

    std::mutex mutex_;
    std::condition_variable condition_;
    std::queue<std::string> queue_;

    bool stopping_{false};
    bool started_{false};
    std::thread worker_;

    int speaker_id_{0};
    int volume_{80};
    std::string config_text_;
};

int main(int argc, char** argv) {
    rclcpp::init(argc, argv);

    const char* env_config = std::getenv("G1_TTS_CONFIG");

    const std::string config_path =
        (env_config != nullptr && env_config[0] != '\0')
            ? env_config
            : "/home/unitree/unitree_ros2/example/config/tts_config.json";

    try {
        auto audio_client =
            std::make_shared<unitree::ros2::g1::AudioClient>();

        auto tts_node =
            std::make_shared<G1TtsNode>(audio_client, config_path);

        rclcpp::executors::SingleThreadedExecutor executor;

        executor.add_node(audio_client);
        executor.add_node(tts_node);

        // 先启动 ROS 2 executor，再启动 TTS 工作线程。
        std::thread spin_thread([&executor]() {
            executor.spin();
        });

        RCLCPP_INFO(
            tts_node->get_logger(),
            "Executor thread started. Starting TTS worker.");

        // 这是启动缓冲，不保证 DDS discovery 已完全完成。
        std::this_thread::sleep_for(std::chrono::seconds(1));

        tts_node->start();

        // 主线程等待 Ctrl+C。收到退出信号后，退出 spin。
        while (rclcpp::ok()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        executor.cancel();

        if (spin_thread.joinable()) {
            spin_thread.join();
        }

        executor.remove_node(tts_node);
        executor.remove_node(audio_client);

        // 等待正在进行的 TtsMaker 调用结束后，再销毁 AudioClient。
        tts_node->stop();

        rclcpp::shutdown();

        tts_node.reset();
        audio_client.reset();

    } catch (const std::exception& e) {
        RCLCPP_ERROR(
            rclcpp::get_logger("g1_tts_node"),
            "Failed to start G1 TTS node: %s",
            e.what());

        if (rclcpp::ok()) {
            rclcpp::shutdown();
        }

        return 1;
    }

    return 0;
}
