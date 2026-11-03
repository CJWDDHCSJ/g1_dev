/**
 * Standalone diagnostic TTS test for the Unitree G1 audio API.
 * Reuses the official AudioClient and only calls TtsMaker.
 *
 * Added instrumentation (this file only, official sources untouched):
 *  - a long-lived observer node subscribed to /api/voice/response for the
 *    whole run, recording every TTS (api_id=1001) response with its arrival
 *    time, so we can tell apart "service answered late" from "per-call
 *    temporary subscription lost the discovery race";
 *  - timing prints around the TtsMaker call.
 */
#include <chrono>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

#include <rclcpp/executors.hpp>
#include <rclcpp/rclcpp.hpp>
#include <unitree_api/msg/response.hpp>

#include "g1/g1_audio_client.hpp"

using unitree_api::msg::Response;

int main(int argc, char **argv) {
  const std::string text =
      argc > 1 ? argv[1] : "你好，我是宇树G1机器人，语音测试成功";
  const auto t0 = std::chrono::steady_clock::now();
  auto elapsed = [t0]() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0)
        .count();
  };

  int32_t ret = -99;
  try {
    rclcpp::init(argc, argv);
    auto client = std::make_shared<unitree::ros2::g1::AudioClient>();

    struct Hit {
      double t;
      long long id;
      int32_t code;
    };
    auto hits = std::make_shared<std::vector<Hit>>();
    auto observer = std::make_shared<rclcpp::Node>("tts_response_observer");
    auto sub = observer->create_subscription<Response>(
        "/api/voice/response", rclcpp::QoS(10),
        [t0, hits](const std::shared_ptr<const Response> msg) {
          if (msg->header.identity.api_id == 1001) {
            hits->push_back(Hit{
                std::chrono::duration<double>(
                    std::chrono::steady_clock::now() - t0).count(),
                static_cast<long long>(msg->header.identity.id),
                msg->header.status.code});
          }
        });
    (void)sub;

    rclcpp::executors::SingleThreadedExecutor exec;
    exec.add_node(client);
    exec.add_node(observer);
    std::thread spin_thread([&exec]() { exec.spin(); });

    std::printf("[TTS] nodes up, warming up discovery for 3s\n");
    std::this_thread::sleep_for(std::chrono::seconds(3));

    std::printf("[TTS] sending request at t=%.2fs: \"%s\" (speaker_id=0)\n",
                elapsed(), text.c_str());
    ret = client->TtsMaker(text, 0);
    std::printf("[TTS] TtsMaker returned ret=%d at t=%.2fs\n", ret, elapsed());

    std::printf("[TTS] holding executor 15s for playback / late responses\n");
    std::this_thread::sleep_for(std::chrono::seconds(15));

    std::printf("[TTS] observer captured %zu TTS response(s) in this run:\n",
                hits->size());
    for (const auto &h : *hits) {
      std::printf("      t=%.2fs id=%lld status=%d\n", h.t, h.id, h.code);
    }

    rclcpp::shutdown();
    if (spin_thread.joinable()) {
      spin_thread.join();
    }
  } catch (const rclcpp::exceptions::RCLError &e) {
    std::fprintf(stderr, "[TTS] RCLError caught: %s\n", e.what());
    return 1;
  }

  return ret == 0 ? 0 : 1;
}
