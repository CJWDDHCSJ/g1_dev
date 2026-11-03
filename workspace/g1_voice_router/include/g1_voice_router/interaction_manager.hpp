#ifndef G1_VOICE_ROUTER_INTERACTION_MANAGER_HPP_
#define G1_VOICE_ROUTER_INTERACTION_MANAGER_HPP_

#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>
#include <string>
#include <unordered_map>

#include "g1_voice_router/interaction_types.hpp"

namespace g1_voice_router
{

// 文本归一化：去首尾空白、压缩连续空白、删除中英文标点、ASCII 小写。
// 不对 UTF-8 多字节序列做 tolower（仅处理 ASCII 字节）。
std::string normalize_text(const std::string & text);

// 文本合法性/噪声判定（纯函数，便于单测）。
// 只过滤"完整独立的明确噪声"与空/纯标点文本；
// 短但有效的输入（你好、嗨、IMU、G1、停止、取消）一律保留。
TextVerdict classify_text(
    const std::string & text,
    std::string & reason);

// 交互任务管理器（P0-19）：
// - 线程安全提交（ROS 回调线程调用 submit）
// - 文本过滤 + 时间窗口去重
// - 有界 FIFO 队列（max_pending 指"等待中"的请求数，不含正在处理的一条）
// - 队列满时拒绝最新请求（返回 REJECTED + 原因），不静默丢弃
// - 状态机与结构化统计
class InteractionManager
{
public:
    static constexpr std::size_t kDefaultMaxPending = 3;
    static constexpr long kDefaultDedupeWindowMs = 1200;

    explicit InteractionManager(
        std::size_t max_pending = kDefaultMaxPending,
        long dedupe_window_ms = kDefaultDedupeWindowMs);

    ~InteractionManager();

    InteractionManager(const InteractionManager &) = delete;
    InteractionManager & operator=(const InteractionManager &) = delete;

    // 提交一条 ASR 文本。返回是否被接受及原因。
    AdmissionResult submit(
        const std::string & text,
        long now_ms);

    // worker 调用：阻塞等待队列中的下一条；被 stop() 唤醒时返回 false。
    bool wait_pop(InteractionRequest & out);

    // 非阻塞取一条（测试用）。
    bool try_pop(InteractionRequest & out);

    // 当前仍待处理的已接受请求数（不含正在执行的一条）。
    std::size_t pending_count() const;

    // 更新指定请求的状态；非法状态转移返回 false。
    bool mark_status(
        std::uint64_t request_id,
        InteractionStatus next,
        long now_ms,
        const std::string & note = "");

    // 状态转移合法性（静态，便于单测）。
    static bool can_transition(
        InteractionStatus from,
        InteractionStatus to);

    // 停止：标记停机、唤醒 worker、把等待中的请求全部标记 REJECTED。
    void stop();

    bool stopping() const;

    InteractionStats stats() const;

    std::size_t queue_depth() const;

    // 仅用于测试：清空去重历史。
    void clear_dedupe_history();

    // 仅用于测试：观察去重缓存规模（验证有界性）。
    std::size_t dedupe_history_size() const;

private:
    InteractionStatus status_of_locked(std::uint64_t request_id) const;

    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<InteractionRequest> queue_;
    bool stopping_{false};

    std::size_t max_pending_;
    long dedupe_window_ms_;

    std::unordered_map<std::string, long> last_accepted_ms_;
    unsigned long dedupe_prune_counter_{0};
    std::map<std::uint64_t, InteractionStatus> status_map_;
    std::uint64_t next_request_id_{1};
    InteractionStats stats_;
};

}  // namespace g1_voice_router

#endif  // G1_VOICE_ROUTER_INTERACTION_MANAGER_HPP_
