#ifndef G1_VOICE_ROUTER_INTERACTION_TYPES_HPP_
#define G1_VOICE_ROUTER_INTERACTION_TYPES_HPP_

#include <cstddef>
#include <cstdint>
#include <string>

namespace g1_voice_router
{

// 交互请求生命周期状态（P0-19）。
// 终态：COMPLETED / FAILED / TIMEOUT / REJECTED / FILTERED / DUPLICATE
enum class InteractionStatus
{
    RECEIVED,        // 已接收
    FILTERED,        // 噪声/空文本被过滤
    DUPLICATE,       // 去重窗口内重复
    QUEUED,          // 进入有界队列
    PROCESSING,      // worker 开始处理
    WAITING_OLLAMA,  // 等待模型响应
    ACTION_WAITING,  // 动作请求已发出，等待 Action Executor 结果
    REPLY_READY,     // 回复文本已生成并发布到 /g1/tts
    SPEAKING,        // 保留状态位（当前无播放完成反馈，仅内部语义）
    COMPLETED,       // 处理完成
    FAILED,          // 处理失败（HTTP/解析/异常）
    TIMEOUT,         // 超时
    REJECTED         // 队列满或停机丢弃
};

const char * to_string(InteractionStatus status);

// 单个请求的记录（仅在队列/处理期间存在，完成后不长期保留）。
struct InteractionRequest
{
    std::uint64_t request_id{0};
    std::string text;         // 原始文本（作业日志只输出长度）
    std::string normalized;   // 归一化文本（去重用）
    InteractionStatus status{InteractionStatus::RECEIVED};
    long received_ms{0};      // steady_clock 毫秒
    long queued_ms{0};        // 入队时刻
    long started_ms{0};       // 开始处理时刻
    long finished_ms{0};      // 结束时刻
    std::string note;         // 失败/过滤原因等
};

// 统计计数（只增不减，便于运行期观测）。
struct InteractionStats
{
    std::uint64_t received{0};
    std::uint64_t filtered{0};
    std::uint64_t duplicates{0};
    std::uint64_t rejected_full{0};
    std::uint64_t accepted{0};
    std::uint64_t completed{0};
    std::uint64_t failed{0};
    std::uint64_t timed_out{0};
    std::size_t max_queue_depth{0};
};

// 文本过滤结论。
enum class TextVerdict
{
    ACCEPT,
    FILTERED_EMPTY,
    FILTERED_NOISE
};

// 提交结果。
struct AdmissionResult
{
    bool accepted{false};
    InteractionStatus status{InteractionStatus::REJECTED};
    std::uint64_t request_id{0};
    std::string reason;
    std::size_t queue_depth{0};
};

}  // namespace g1_voice_router

#endif  // G1_VOICE_ROUTER_INTERACTION_TYPES_HPP_
