#include <cstdint>
#include <memory>
#include <string>

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"

namespace
{

// 解析 4 位十六进制数，用来处理 JSON 中的 \uXXXX。
bool parse_hex4(
  const std::string & json,
  std::size_t pos,
  std::uint32_t & value)
{
  if (pos + 4 > json.size()) {
    return false;
  }

  value = 0;

  for (std::size_t j = 0; j < 4; ++j) {
    const char c = json[pos + j];
    std::uint32_t digit = 0;

    if (c >= '0' && c <= '9') {
      digit = static_cast<std::uint32_t>(c - '0');
    } else if (c >= 'a' && c <= 'f') {
      digit = static_cast<std::uint32_t>(c - 'a' + 10);
    } else if (c >= 'A' && c <= 'F') {
      digit = static_cast<std::uint32_t>(c - 'A' + 10);
    } else {
      return false;
    }

    value = (value << 4) | digit;
  }

  return true;
}

// 把 Unicode 码点编码成 UTF-8。
void append_utf8(std::string & output, std::uint32_t cp)
{
  if (cp <= 0x7F) {
    output.push_back(static_cast<char>(cp));
  } else if (cp <= 0x7FF) {
    output.push_back(static_cast<char>(0xC0 | (cp >> 6)));
    output.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  } else if (cp <= 0xFFFF) {
    output.push_back(static_cast<char>(0xE0 | (cp >> 12)));
    output.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
    output.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  } else {
    output.push_back(static_cast<char>(0xF0 | (cp >> 18)));
    output.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
    output.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
    output.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  }
}

// 从 JSON 字符串中提取名为 text 的字符串字段。
// 不依赖额外 JSON 库，支持常见转义和 \uXXXX。
std::string extract_text(const std::string & json)
{
  const std::string key = "\"text\"";
  const auto key_pos = json.find(key);

  if (key_pos == std::string::npos) {
    return {};
  }

  const auto colon = json.find(':', key_pos + key.size());

  if (colon == std::string::npos) {
    return {};
  }

  const auto start = json.find_first_not_of(" \t\r\n", colon + 1);

  if (start == std::string::npos || json[start] != '"') {
    return {};
  }

  std::string output;

  for (std::size_t i = start + 1; i < json.size(); ++i) {
    const char c = json[i];

    if (c == '"') {
      return output;
    }

    if (c != '\\') {
      output.push_back(c);
      continue;
    }

    if (++i >= json.size()) {
      return {};
    }

    switch (json[i]) {
      case '"':  output.push_back('"');  break;
      case '\\': output.push_back('\\'); break;
      case '/':  output.push_back('/');  break;
      case 'b':  output.push_back('\b'); break;
      case 'f':  output.push_back('\f'); break;
      case 'n':  output.push_back('\n'); break;
      case 'r':  output.push_back('\r'); break;
      case 't':  output.push_back('\t'); break;

      case 'u': {
        std::uint32_t cp = 0;

        if (!parse_hex4(json, i + 1, cp)) {
          return {};
        }

        i += 4;

        // 处理 UTF-16 代理对。
        if (cp >= 0xD800 && cp <= 0xDBFF) {
          if (i + 6 < json.size() &&
              json[i + 1] == '\\' &&
              json[i + 2] == 'u') {
            std::uint32_t low = 0;

            if (parse_hex4(json, i + 3, low) &&
                low >= 0xDC00 && low <= 0xDFFF) {
              cp = 0x10000 + ((cp - 0xD800) << 10)
                   + (low - 0xDC00);
              i += 6;
            } else {
              cp = 0xFFFD;
            }
          } else {
            cp = 0xFFFD;
          }
        } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
          cp = 0xFFFD;
        }

        append_utf8(output, cp);
        break;
      }

      default:
        return {};
    }
  }

  return {};
}

}  // namespace

class AsrJsonBridge : public rclcpp::Node
{
public:
  AsrJsonBridge()
  : Node("g1_asr_bridge")
  {
    const auto input_topic =
      this->declare_parameter<std::string>(
        "input_topic", "/audio_msg");

    const auto output_topic =
      this->declare_parameter<std::string>(
        "output_topic", "/g1/asr_text");

    // BEST_EFFORT 订阅可以兼容可靠和尽力传输的发布端。
    const auto input_qos =
      rclcpp::QoS(rclcpp::KeepLast(10)).best_effort();

    publisher_ =
      this->create_publisher<std_msgs::msg::String>(
        output_topic, 10);

    subscription_ =
      this->create_subscription<std_msgs::msg::String>(
        input_topic,
        input_qos,
        [this](const std_msgs::msg::String::SharedPtr msg) {
          const auto text = extract_text(msg->data);

          if (text.empty()) {
            RCLCPP_WARN(
              this->get_logger(),
              "No usable non-empty 'text' field in ASR JSON");
            return;
          }

          std_msgs::msg::String output;
          output.data = text;
          publisher_->publish(output);

          RCLCPP_INFO(
            this->get_logger(),
            "ASR recognized: %s",
            text.c_str());
        });

    RCLCPP_INFO(
      this->get_logger(),
      "ASR bridge started: %s -> %s",
      input_topic.c_str(),
      output_topic.c_str());
  }

private:
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr subscription_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr publisher_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<AsrJsonBridge>());
  rclcpp::shutdown();
  return 0;
}
