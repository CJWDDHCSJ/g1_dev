#include <unitree/robot/channel/channel_factory.hpp>
#include <unitree/robot/channel/channel_subscriber.hpp>
#include <unitree/robot/g1/audio/g1_audio_client.hpp>
#include <unitree/idl/ros2/String_.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cctype>
#include <cstdint>
#include <csignal>
#include <memory>
#include <stdexcept>
#include <fstream>
#include <functional>
#include <iostream>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <utility>

using namespace unitree::robot;
using namespace unitree::robot::g1;
using StringMsg = std_msgs::msg::dds_::String_;

using namespace std::chrono_literals;

static volatile std::sig_atomic_t g_running = 1;

static void handle_sigint(int)
{
    g_running = 0;
}

// ------------------------------------------------------------
// Minimal JSON helpers.
// Handles normal JSON strings, escaped characters and \uXXXX.
// ------------------------------------------------------------

static void append_utf8(std::string &out, uint32_t cp)
{
    if (cp <= 0x7F) {
        out.push_back(static_cast<char>(cp));
    } else if (cp <= 0x7FF) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp <= 0xFFFF) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

static bool read_hex4(const std::string &s, size_t pos, uint32_t &value)
{
    if (pos + 4 > s.size()) {
        return false;
    }

    value = 0;
    for (size_t i = pos; i < pos + 4; ++i) {
        const char c = s[i];
        value <<= 4;

        if (c >= '0' && c <= '9') {
            value |= static_cast<uint32_t>(c - '0');
        } else if (c >= 'a' && c <= 'f') {
            value |= static_cast<uint32_t>(c - 'a' + 10);
        } else if (c >= 'A' && c <= 'F') {
            value |= static_cast<uint32_t>(c - 'A' + 10);
        } else {
            return false;
        }
    }
    return true;
}

static std::string json_string_field(
    const std::string &json, const std::string &key)
{
    const std::string token = "\"" + key + "\"";
    size_t pos = json.find(token);

    if (pos == std::string::npos) {
        return {};
    }

    pos = json.find(':', pos + token.size());
    if (pos == std::string::npos) {
        return {};
    }

    ++pos;
    while (pos < json.size() &&
           std::isspace(static_cast<unsigned char>(json[pos]))) {
        ++pos;
    }

    if (pos >= json.size() || json[pos] != '"') {
        return {};
    }

    ++pos;
    std::string result;

    while (pos < json.size()) {
        char c = json[pos++];

        if (c == '"') {
            return result;
        }

        if (c != '\\') {
            result.push_back(c);
            continue;
        }

        if (pos >= json.size()) {
            break;
        }

        const char escaped = json[pos++];

        switch (escaped) {
        case '"':  result.push_back('"');  break;
        case '\\': result.push_back('\\'); break;
        case '/':  result.push_back('/');  break;
        case 'b':  result.push_back('\b'); break;
        case 'f':  result.push_back('\f'); break;
        case 'n':  result.push_back('\n');  break;
        case 'r':  result.push_back('\r');  break;
        case 't':  result.push_back('\t');  break;

        case 'u': {
            uint32_t cp = 0;
            if (!read_hex4(json, pos, cp)) {
                return {};
            }
            pos += 4;

            // Combine UTF-16 surrogate pairs if present.
            if (cp >= 0xD800 && cp <= 0xDBFF &&
                pos + 6 <= json.size() &&
                json.compare(pos, 2, "\\u") == 0) {
                uint32_t low = 0;
                if (read_hex4(json, pos + 2, low) &&
                    low >= 0xDC00 && low <= 0xDFFF) {
                    cp = 0x10000 +
                         ((cp - 0xD800) << 10) +
                         (low - 0xDC00);
                    pos += 6;
                }
            }

            append_utf8(result, cp);
            break;
        }

        default:
            return {};
        }
    }

    return {};
}

static bool json_bool_field(
    const std::string &json,
    const std::string &key,
    bool &value)
{
    const std::string token = "\"" + key + "\"";
    size_t pos = json.find(token);

    if (pos == std::string::npos) {
        return false;
    }

    pos = json.find(':', pos + token.size());
    if (pos == std::string::npos) {
        return false;
    }

    ++pos;
    while (pos < json.size() &&
           std::isspace(static_cast<unsigned char>(json[pos]))) {
        ++pos;
    }

    if (json.compare(pos, 4, "true") == 0) {
        value = true;
        return true;
    }

    if (json.compare(pos, 5, "false") == 0) {
        value = false;
        return true;
    }

    if (pos < json.size() && json[pos] == '1') {
        value = true;
        return true;
    }

    if (pos < json.size() && json[pos] == '0') {
        value = false;
        return true;
    }

    const std::string string_value = json_string_field(json, key);
    if (string_value == "true" || string_value == "1") {
        value = true;
        return true;
    }
    if (string_value == "false" || string_value == "0") {
        value = false;
        return true;
    }

    return false;
}

static long long json_int_field(
    const std::string &json,
    const std::string &key,
    long long fallback = -1)
{
    const std::string token = "\"" + key + "\"";
    size_t pos = json.find(token);

    if (pos == std::string::npos) {
        return fallback;
    }

    pos = json.find(':', pos + token.size());
    if (pos == std::string::npos) {
        return fallback;
    }

    ++pos;
    while (pos < json.size() &&
           std::isspace(static_cast<unsigned char>(json[pos]))) {
        ++pos;
    }

    try {
        if (pos < json.size() && json[pos] == '"') {
            const std::string value = json_string_field(json, key);
            return std::stoll(value);
        }
        return std::stoll(json.substr(pos));
    } catch (...) {
        return fallback;
    }
}

static std::string clean_text(std::string text)
{
    for (char &c : text) {
        if (c == '\n' || c == '\r' || c == '\t') {
            c = ' ';
        }
    }
    return text;
}

static std::string normalize_language(const std::string &language)
{
    if (language.compare(0, 2, "en") == 0 ||
        language.compare(0, 2, "EN") == 0) {
        return "en";
    }
    return "zh";
}

// ------------------------------------------------------------
// SpeechManager: serial TTS queue.
// TtsMaker returns before audio playback finishes, so this first
// version uses estimated playback duration plus a safety margin.
// ------------------------------------------------------------

struct SpeechTask
{
    std::string text;
    int32_t speaker_id;
};

class SpeechManager
{
public:
    explicit SpeechManager(AudioClient &client)
        : client_(client), stopping_(false), busy_(false)
    {
        worker_ = std::thread(&SpeechManager::worker_loop, this);
    }

    ~SpeechManager()
    {
        shutdown();
    }

    bool speak(std::string text, int32_t speaker_id = 0)
    {
        if (text.empty()) {
            return false;
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (stopping_) {
                return false;
            }
            queue_.push({std::move(text), speaker_id});
        }

        cv_.notify_one();
        return true;
    }

    bool is_busy() const
    {
        return busy_.load();
    }

    void shutdown()
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (stopping_) {
                return;
            }
            stopping_ = true;
        }

        cv_.notify_all();

        if (worker_.joinable()) {
            worker_.join();
        }
    }

private:
    static double estimate_seconds(
        const std::string &text, int32_t speaker_id)
    {
        // Initial engineering estimate, not actual playback feedback.
        double seconds = 0.0;

        if (speaker_id == 0) {
            size_t utf8_chars = 0;
            for (unsigned char c : text) {
                if ((c & 0xC0) != 0x80) {
                    ++utf8_chars;
                }
            }
            seconds = static_cast<double>(utf8_chars) * 0.28 + 2.0;
        } else {
            size_t words = 0;
            bool in_word = false;

            for (unsigned char c : text) {
                if (std::isspace(c)) {
                    in_word = false;
                } else if (!in_word) {
                    ++words;
                    in_word = true;
                }
            }
            seconds = static_cast<double>(words) * 0.45 + 2.0;
        }

        return seconds < 3.0 ? 3.0 : seconds;
    }

    void worker_loop()
    {
        while (true) {
            SpeechTask task;

            {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.wait(lock, [this] {
                    return stopping_ || !queue_.empty();
                });

                if (stopping_ && queue_.empty()) {
                    break;
                }

                task = std::move(queue_.front());
                queue_.pop();
            }

            busy_.store(true);

            std::cout << "[TTS] " << task.text << std::endl;

            const auto start = std::chrono::steady_clock::now();
            int32_t ret = client_.TtsMaker(task.text, task.speaker_id);
            const auto end = std::chrono::steady_clock::now();

            const double call_ms =
                std::chrono::duration<double, std::milli>(
                    end - start).count();

            std::cout << "[TTS] ret=" << ret
                      << ", call_ms=" << call_ms << std::endl;

            if (ret == 0) {
                const double wait_seconds =
                    estimate_seconds(task.text, task.speaker_id);

                std::cout << "[TTS] Estimated wait: "
                          << wait_seconds << " s" << std::endl;

                std::this_thread::sleep_for(
                    std::chrono::duration<double>(wait_seconds));
            } else {
                std::cerr << "[TTS] Request failed; waiting 0.5 s"
                          << std::endl;
                std::this_thread::sleep_for(500ms);
            }

            busy_.store(false);
        }
    }

    AudioClient &client_;
    std::queue<SpeechTask> queue_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::thread worker_;
    bool stopping_;
    std::atomic<bool> busy_;
};

// ------------------------------------------------------------
// File bridge:
// ASR appends "zh|text" or "en|text" to dialogue.txt.
// This thread reads only newly appended complete lines.
// ------------------------------------------------------------

class DialogueFileBridge
{
public:
    DialogueFileBridge(
        const std::string &path, SpeechManager &speech)
        : path_(path), speech_(speech), stopping_(false)
    {
        // This file is an output log for the current run.
        std::ofstream clear_file(path_, std::ios::trunc);
        if (!clear_file) {
            throw std::runtime_error(
                "Cannot create dialogue file: " + path_);
        }
        clear_file.close();

        reader_thread_ =
            std::thread(&DialogueFileBridge::reader_loop, this);
    }

    ~DialogueFileBridge()
    {
        shutdown();
    }

    bool append_asr_text(
        const std::string &language, const std::string &text)
    {
        std::lock_guard<std::mutex> lock(file_mutex_);

        std::ofstream out(path_, std::ios::app);
        if (!out) {
            std::cerr << "[File] Cannot append: " << path_ << std::endl;
            return false;
        }

        out << language << "|" << text << "\n";
        out.flush();

        if (!out) {
            std::cerr << "[File] Write failed" << std::endl;
            return false;
        }

        std::cout << "[File] Appended: "
                  << language << "|" << text << std::endl;
        return true;
    }

    void shutdown()
    {
        stopping_.store(true);

        if (reader_thread_.joinable()) {
            reader_thread_.join();
        }
    }

private:
    void reader_loop()
    {
        std::ifstream in(path_);
        if (!in) {
            std::cerr << "[File] Cannot open: " << path_ << std::endl;
            return;
        }

        while (!stopping_.load()) {
            {
                std::lock_guard<std::mutex> lock(file_mutex_);

                // Clear EOF state so newly appended lines can be read.
                in.clear();

                std::string line;
                while (std::getline(in, line)) {
                    const size_t separator = line.find('|');

                    if (separator == std::string::npos ||
                        separator + 1 >= line.size()) {
                        continue;
                    }

                    const std::string language =
                        line.substr(0, separator);
                    const std::string text =
                        line.substr(separator + 1);

                    const int32_t speaker_id =
                        language == "en" ? 1 : 0;

                    std::cout << "[File] Read new line: "
                              << line << std::endl;

                    speech_.speak(text, speaker_id);
                }
            }

            std::this_thread::sleep_for(100ms);
        }
    }

    std::string path_;
    SpeechManager &speech_;
    std::mutex file_mutex_;
    std::atomic<bool> stopping_;
    std::thread reader_thread_;
};

// ------------------------------------------------------------
// ASR subscriber: rt/audio_msg
// Expected JSON fields include text, is_final, language, index.
// ------------------------------------------------------------

class AsrReceiver
{
public:
    AsrReceiver(
        DialogueFileBridge &file_bridge,
        SpeechManager &speech)
        : file_bridge_(file_bridge), speech_(speech),
          last_index_(-1)
    {
    }

    void handler(const void *message)
    {
        if (message == nullptr) {
            return;
        }

        const auto *msg =
            static_cast<const StringMsg *>(message);
        const std::string raw_json = msg->data();

        // Debug: inspect the actual ASR payload before assuming its schema.
        std::cout << "[ASR RAW] " << raw_json << std::endl;

        // Diagnostic mode: accept non-final ASR transcript events too.
        // The observed rt/audio_msg payloads currently have is_final=false.

        std::string text = json_string_field(raw_json, "text");
        text = clean_text(text);

        if (text.empty()) {
            return;
        }

        // First-version half-duplex guard: don't echo the robot's own
        // TTS back into the dialogue file while it is speaking.
        if (speech_.is_busy()) {
            std::cout << "[ASR] Ignored while TTS is speaking: "
                      << text << std::endl;
            return;
        }

        const long long index =
            json_int_field(raw_json, "index", -1);

        {
            std::lock_guard<std::mutex> lock(state_mutex_);

            // Avoid duplicate delivery of the same final ASR item.
            if (index >= 0 && index == last_index_ &&
                text == last_text_) {
                return;
            }

            if (index < 0 && text == last_text_) {
                return;
            }

            last_index_ = index;
            last_text_ = text;
        }

        const std::string language = normalize_language(
            json_string_field(raw_json, "language"));

        std::cout << "[ASR] Final text: " << text
                  << ", language=" << language
                  << ", index=" << index << std::endl;

        file_bridge_.append_asr_text(language, text);
    }

private:
    DialogueFileBridge &file_bridge_;
    SpeechManager &speech_;
    std::mutex state_mutex_;
    long long last_index_;
    std::string last_text_;
};

int main(int argc, char **argv)
{
    const std::string network_interface =
        argc >= 2 ? argv[1] : "eth0";

    const std::string dialogue_path =
        argc >= 3
            ? argv[2]
            : "/home/unitree/unitree/unitree_sdk2/dialogue.txt";

    std::signal(SIGINT, handle_sigint);

    try {
        std::cout << "[Main] Initializing DDS on "
                  << network_interface << std::endl;

        ChannelFactory::Instance()->Init(0, network_interface);

        AudioClient client;
        client.SetTimeout(10.0f);
        client.Init();

        uint8_t volume = 0;
        int32_t ret = client.GetVolume(volume);
        std::cout << "[Audio] GetVolume ret=" << ret
                  << ", volume=" << static_cast<int>(volume) << std::endl;

        ret = client.SetVolume(100);
        std::cout << "[Audio] SetVolume ret=" << ret << std::endl;

        SpeechManager speech(client);
        DialogueFileBridge file_bridge(dialogue_path, speech);
        AsrReceiver asr(file_bridge, speech);

        auto subscriber =
            std::make_shared<ChannelSubscriber<StringMsg>>(
                "rt/audio_msg");

        subscriber->InitChannel(
            std::bind(
                &AsrReceiver::handler,
                &asr,
                std::placeholders::_1),
            1);

        std::cout << "[Main] ASR -> TXT -> TTS loop is running."
                  << std::endl;
        std::cout << "[Main] ASR topic: rt/audio_msg" << std::endl;
        std::cout << "[Main] Dialogue file: "
                  << dialogue_path << std::endl;
        std::cout << "[Main] Speak to G1. Press Ctrl+C to exit."
                  << std::endl;

        while (g_running) {
            std::this_thread::sleep_for(200ms);
        }

        std::cout << "\n[Main] Stopping..." << std::endl;

        // Destroy subscriber before its callback target goes out of scope.
        subscriber.reset();

        // Stop file reader, then let queued TTS tasks finish.
        file_bridge.shutdown();
        speech.shutdown();

        std::cout << "[Main] Finished" << std::endl;
    } catch (const std::exception &e) {
        std::cerr << "[Main] Exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
