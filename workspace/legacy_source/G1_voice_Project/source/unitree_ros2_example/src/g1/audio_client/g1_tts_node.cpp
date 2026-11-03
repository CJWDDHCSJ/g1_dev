#include <algorithm>
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
#include <vector>

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

        subscription_ = create_subscription<std_msgs::msg::String>(
            "/g1/tts",
            rclcpp::QoS(10),
            [this](const std_msgs::msg::String::SharedPtr msg) {
                enqueue_text(msg->data, "ROS 2 topic");
            });

        RCLCPP_INFO(
            get_logger(),
            "G1 TTS node initialized. Listening on /g1/tts");
    }

    void start() {
        {
            std::lock_guard<std::mutex> lock(mutex_);

            if (started_ || stopping_) {
                RCLCPP_WARN(get_logger(), "TTS worker cannot be started.");
                return;
            }

            started_ = true;
        }

        worker_ = std::thread(&G1TtsNode::worker_loop, this);

        RCLCPP_INFO(get_logger(), "TTS worker started.");

        if (!config_text_.empty()) {
            enqueue_text(config_text_, "JSON config");
        } else {
            RCLCPP_WARN(get_logger(), "Startup text is empty.");
        }
    }

    void stop() {
        bool first_stop = false;

        {
            std::lock_guard<std::mutex> lock(mutex_);

            if (!stopping_) {
                stopping_ = true;
                first_stop = true;

                std::queue<std::string> empty;
                queue_.swap(empty);
            }
        }

        condition_.notify_all();

        if (worker_.joinable()) {
            worker_.join();
        }

        if (first_stop) {
            RCLCPP_INFO(get_logger(), "G1 TTS worker stopped.");
        }
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
            throw std::runtime_error("TTS config must be a JSON object.");
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
            throw std::runtime_error("speaker_id cannot be negative.");
        }

        if (volume_ < 0 || volume_ > 100) {
            throw std::runtime_error("volume must be between 0 and 100.");
        }

        if (config.contains("pause_ms")) {
            if (!config["pause_ms"].is_number_integer()) {
                throw std::runtime_error(
                    "JSON field 'pause_ms' must be an integer.");
            }

            pause_ms_ = config["pause_ms"].get<int>();
        }

        if (pause_ms_ < 0 || pause_ms_ > 10000) {
            throw std::runtime_error(
                "pause_ms must be between 0 and 10000.");
        }

        RCLCPP_INFO(
            get_logger(), "Loaded TTS config: %s", config_path.c_str());

        RCLCPP_INFO(
            get_logger(),
            "speaker_id=%d, configured volume=%d, pause=%d ms",
            speaker_id_, volume_, pause_ms_);

        RCLCPP_WARN(
            get_logger(),
            "JSON volume is loaded but not applied by this node.");

        RCLCPP_INFO(
            get_logger(), "Startup text: %s", config_text_.c_str());
    }

    void enqueue_text(const std::string& text, const char* source) {
        if (text.empty()) {
            RCLCPP_WARN(get_logger(), "Received empty text from %s.", source);
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
                    get_logger(), "TTS queue full. Dropping newest message.");
                return;
            }

            queue_.push(text);
        }

        condition_.notify_one();

        RCLCPP_INFO(get_logger(), "Text queued from %s.", source);
    }

    std::vector<std::string> split_sentences(
        const std::string& text) const {

        std::vector<std::string> sentences;
        std::size_t start = 0;
        std::size_t i = 0;

        while (i < text.size()) {
            std::size_t punctuation_length = 0;

            if (text.compare(i, 3, "。") == 0 ||
                text.compare(i, 3, "！") == 0 ||
                text.compare(i, 3, "？") == 0) {
                punctuation_length = 3;
            } else if (
                text[i] == '.' ||
                text[i] == '!' ||
                text[i] == '?') {
                punctuation_length = 1;
            }

            if (punctuation_length > 0) {
                sentences.push_back(
                    text.substr(start, i + punctuation_length - start));

                i += punctuation_length;
                start = i;
            } else {
                ++i;
            }
        }

        if (start < text.size()) {
            sentences.push_back(text.substr(start));
        }

        return sentences;
    }

    std::size_t utf8_character_count(const std::string& text) const {
        std::size_t count = 0;

        for (unsigned char ch : text) {
            if ((ch & 0xC0) != 0x80) {
                ++count;
            }
        }

        return count;
    }

    std::chrono::milliseconds estimate_speech_time(
        const std::string& sentence) const {

        // 经验估算：约 250 ms/字符，至少等待 1 秒。
        const std::size_t count = utf8_character_count(sentence);
        const std::size_t estimated_ms =
            std::max<std::size_t>(1000, count * 250);

        return std::chrono::milliseconds(
            static_cast<std::chrono::milliseconds::rep>(estimated_ms));
    }

    bool interruptible_wait(std::chrono::milliseconds duration) {
        std::unique_lock<std::mutex> lock(mutex_);

        return !condition_.wait_for(
            lock, duration, [this]() { return stopping_; });
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

            const auto sentences = split_sentences(text);

            RCLCPP_INFO(
                get_logger(),
                "Message split into %zu sentence(s).",
                sentences.size());

            for (std::size_t i = 0; i < sentences.size(); ++i) {
                {
                    std::lock_guard<std::mutex> lock(mutex_);

                    if (stopping_) {
                        return;
                    }
                }

                const std::string& sentence = sentences[i];

                if (sentence.empty()) {
                    continue;
                }

                RCLCPP_INFO(
                    get_logger(),
                    "Speaking sentence %zu/%zu: %s",
                    i + 1, sentences.size(), sentence.c_str());

                const int32_t ret =
                    audio_client_->TtsMaker(sentence, speaker_id_);

                RCLCPP_INFO(
                    get_logger(),
                    "TtsMaker returned ret=%d for sentence %zu/%zu",
                    ret, i + 1, sentences.size());

                if (ret != 0) {
                    RCLCPP_ERROR(
                        get_logger(),
                        "TTS failed on sentence %zu. Remaining sentences skipped.",
                        i + 1);
                    break;
                }

                const auto speech_time = estimate_speech_time(sentence);

                RCLCPP_INFO(
                    get_logger(),
                    "Estimated speech time: %lld ms",
                    static_cast<long long>(speech_time.count()));

                if (!interruptible_wait(speech_time)) {
                    return;
                }

                if (i + 1 < sentences.size()) {
                    RCLCPP_INFO(
                        get_logger(),
                        "Waiting %d ms before next sentence.",
                        pause_ms_);

                    if (!interruptible_wait(
                            std::chrono::milliseconds(pause_ms_))) {
                        return;
                    }
                }
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
    int pause_ms_{1500};
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

        std::thread spin_thread([&executor]() {
            executor.spin();
        });

        RCLCPP_INFO(
            tts_node->get_logger(),
            "Executor thread started. Starting TTS worker.");

        // 让常驻响应订阅和 DDS 端点有时间稳定。
        std::this_thread::sleep_for(std::chrono::seconds(3));

        tts_node->start();

        while (rclcpp::ok()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        executor.cancel();

        if (spin_thread.joinable()) {
            spin_thread.join();
        }

        executor.remove_node(tts_node);
        executor.remove_node(audio_client);

        tts_node->stop();

        if (rclcpp::ok()) {
            rclcpp::shutdown();
        }

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
