#include "g1_voice_router/interaction_manager.hpp"

#include <atomic>
#include <iostream>
#include <set>
#include <string>
#include <thread>
#include <vector>

using g1_voice_router::AdmissionResult;
using g1_voice_router::InteractionManager;
using g1_voice_router::InteractionRequest;
using g1_voice_router::InteractionStats;
using g1_voice_router::InteractionStatus;
using g1_voice_router::TextVerdict;

namespace
{

int passed = 0;
int failed = 0;

void check(
    const std::string & name,
    bool condition,
    const std::string & detail = "")
{
    if (condition) {
        ++passed;
        std::cout << "[PASS] " << name << '\n';
    } else {
        ++failed;
        std::cout << "[FAIL] " << name;

        if (!detail.empty()) {
            std::cout << " | " << detail;
        }

        std::cout << '\n';
    }
}

// -------- 文本过滤 --------

void test_filter()
{
    struct Case
    {
        const char * input;
        TextVerdict expected;
    };

    const Case cases[] = {
        {"", TextVerdict::FILTERED_EMPTY},
        {"    ", TextVerdict::FILTERED_EMPTY},
        {"\t\n", TextVerdict::FILTERED_EMPTY},
        {"。。！", TextVerdict::FILTERED_EMPTY},
        {"...", TextVerdict::FILTERED_EMPTY},
        {"嗯", TextVerdict::FILTERED_NOISE},
        {"啊", TextVerdict::FILTERED_NOISE},
        {"呃", TextVerdict::FILTERED_NOISE},
        {"唔", TextVerdict::FILTERED_NOISE},
        {"哦", TextVerdict::FILTERED_NOISE},
        {"额", TextVerdict::FILTERED_NOISE},
        {"嗯嗯", TextVerdict::FILTERED_NOISE},
        {"嗯嗯嗯嗯", TextVerdict::FILTERED_NOISE},
        {"嗯，介绍一下实验室", TextVerdict::ACCEPT},
        {"嗯嗯，那实验室呢", TextVerdict::ACCEPT},
        {"你好", TextVerdict::ACCEPT},
        {"嗨", TextVerdict::ACCEPT},
        {"哈喽", TextVerdict::ACCEPT},
        {"IMU", TextVerdict::ACCEPT},
        {"G1", TextVerdict::ACCEPT},
        {"脑机接口", TextVerdict::ACCEPT},
        {"电量多少", TextVerdict::ACCEPT},
        {"停止", TextVerdict::ACCEPT},
        {"取消", TextVerdict::ACCEPT},
        {"啊这", TextVerdict::ACCEPT},
    };

    for (const auto & item : cases) {
        std::string reason;
        const auto verdict = g1_voice_router::classify_text(item.input, reason);
        check(
            std::string("[FILTER] ") + item.input,
            verdict == item.expected,
            "reason=" + reason);
    }
}

// -------- 归一化 --------

void test_normalize()
{
    check(
        "[NORM] 英文大小写与标点等价",
        g1_voice_router::normalize_text("IMU 是什么！") ==
        g1_voice_router::normalize_text("imu是什么"));

    check(
        "[NORM] 尾部标点等价",
        g1_voice_router::normalize_text("你好。") ==
        g1_voice_router::normalize_text("你好"));

    check(
        "[NORM] 内部标点等价",
        g1_voice_router::normalize_text("嗯，介绍一下实验室") ==
        g1_voice_router::normalize_text("嗯介绍一下实验室"));

    check(
        "[NORM] 不同问题不相等",
        g1_voice_router::normalize_text("脑机接口是什么") !=
        g1_voice_router::normalize_text("imu是什么"));

    check(
        "[NORM] UTF-8 中文未被破坏",
        g1_voice_router::normalize_text("脑机接口") == "脑机接口");
}

// -------- 去重 --------

void test_dedupe()
{
    // 队列容量需 >= 本用例接受条数，避免队列满干扰去重判定。
    InteractionManager manager(5, 1200);

    const auto first = manager.submit("你好", 1000);
    check("[DEDUPE] 首次接受", first.accepted);

    const auto second = manager.submit("你好", 1500);
    check(
        "[DEDUPE] 窗口内重复被拒",
        !second.accepted &&
        second.status == InteractionStatus::DUPLICATE);

    const auto punctuated = manager.submit("你好！", 1800);
    check(
        "[DEDUPE] 仅标点不同也判重",
        !punctuated.accepted &&
        punctuated.status == InteractionStatus::DUPLICATE);

    const auto later = manager.submit("你好", 2400);
    check("[DEDUPE] 超出窗口再次接受", later.accepted);

    const auto other = manager.submit("脑机接口是什么", 2500);
    const auto other2 = manager.submit("IMU是什么", 2500);
    check(
        "[DEDUPE] 不同问题分别接受",
        other.accepted && other2.accepted);

    const auto stats = manager.stats();
    check("[DEDUPE] 统计 duplicates=2", stats.duplicates == 2, std::to_string(stats.duplicates));
    check("[DEDUPE] request_id 唯一", first.request_id != later.request_id);
}

// -------- 有界队列 --------

void test_queue()
{
    InteractionManager manager(2, 1200);

    const auto a = manager.submit("第一个问题", 0);
    const auto b = manager.submit("第二个问题", 1);
    const auto c = manager.submit("第三个问题", 2);

    check("[QUEUE] 前两条进入队列", a.accepted && b.accepted);
    check(
        "[QUEUE] 队列满拒绝最新",
        !c.accepted &&
        c.status == InteractionStatus::REJECTED &&
        c.reason == "QUEUE_FULL");

    check("[QUEUE] 队列深度不超过上限", manager.queue_depth() == 2);

    InteractionRequest request;

    check("[QUEUE] 取出第一条", manager.try_pop(request));
    check("[QUEUE] FIFO 顺序 - 第一条", request.text == "第一个问题");

    check("[QUEUE] 取出第二条", manager.try_pop(request));
    check("[QUEUE] FIFO 顺序 - 第二条", request.text == "第二个问题");

    check("[QUEUE] 队列已空", !manager.try_pop(request));

    // 队列空出后可继续接收新请求（后续正常请求不被永久阻塞）。
    const auto d = manager.submit("第四个问题", 3000);
    check("[QUEUE] 空出后可继续接收", d.accepted);

    const auto stats = manager.stats();
    check("[QUEUE] rejected_full 计数", stats.rejected_full == 1);
    check("[QUEUE] max_queue_depth 记录", stats.max_queue_depth == 2);
}

// -------- 状态机 --------

void test_state_machine()
{
    check(
        "[STATE] QUEUED -> PROCESSING 合法",
        InteractionManager::can_transition(
            InteractionStatus::QUEUED, InteractionStatus::PROCESSING));

    check(
        "[STATE] QUEUED -> COMPLETED 非法",
        !InteractionManager::can_transition(
            InteractionStatus::QUEUED, InteractionStatus::COMPLETED));

    check(
        "[STATE] PROCESSING -> WAITING_OLLAMA 合法",
        InteractionManager::can_transition(
            InteractionStatus::PROCESSING,
            InteractionStatus::WAITING_OLLAMA));

    check(
        "[STATE] COMPLETED 是终态",
        !InteractionManager::can_transition(
            InteractionStatus::COMPLETED,
            InteractionStatus::PROCESSING));

    InteractionManager manager(3, 1200);
    const auto admission = manager.submit("脑机接口是什么", 0);
    check("[STATE] 提交后为 QUEUED", admission.accepted);

    // 另一条留在队列里，用于验证 QUEUED 状态下的非法转移。
    const auto queued = manager.submit("另一个问题", 1);

    InteractionRequest request;
    manager.try_pop(request);
    check("[STATE] 取出后为 PROCESSING",
          request.status == InteractionStatus::PROCESSING);

    check(
        "[STATE] 非法转移被拒绝（QUEUED -> COMPLETED）",
        !manager.mark_status(
            queued.request_id,
            InteractionStatus::COMPLETED,
            10));

    check(
        "[STATE] 合法转移成功（PROCESSING -> WAITING_OLLAMA）",
        manager.mark_status(
            request.request_id,
            InteractionStatus::WAITING_OLLAMA,
            20));

    check(
        "[STATE] 完成后统计 COMPLETED",
        manager.mark_status(
            request.request_id,
            InteractionStatus::REPLY_READY,
            30) &&
        manager.mark_status(
            request.request_id,
            InteractionStatus::COMPLETED,
            40));

    check(
        "[STATE] 终态不可再变更",
        !manager.mark_status(
            request.request_id,
            InteractionStatus::PROCESSING,
            50));

    check("[STATE] completed 计数",
          manager.stats().completed == 1);
}

// -------- 停机清理 --------

void test_stop()
{
    InteractionManager manager(3, 1200);
    manager.submit("问题一", 0);
    manager.submit("问题二", 1);

    check("[STOP] 停机前队列非空", manager.queue_depth() == 2);

    manager.stop();

    check("[STOP] 停机后队列清空", manager.queue_depth() == 0);
    check("[STOP] stopping 标记", manager.stopping());

    InteractionRequest request;
    check("[STOP] wait_pop 返回 false", !manager.wait_pop(request));

    const auto rejected = manager.submit("问题三", 100);
    check(
        "[STOP] 停机后拒绝新请求",
        !rejected.accepted &&
        rejected.reason == "STOPPING");
}

// -------- 并发提交 --------

void test_concurrency()
{
    InteractionManager manager(3, 100000);

    std::atomic<int> accepted{0};
    std::atomic<int> rejected{0};
    std::vector<std::thread> threads;

    const int kThreads = 4;
    const int kPerThread = 25;

    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&manager, t, &accepted, &rejected]() {
            for (int i = 0; i < kPerThread; ++i) {
                const std::string text =
                    "并发问题" + std::to_string(t) + "-" + std::to_string(i);
                const auto result = manager.submit(text, 1000 + i);
                if (result.accepted) {
                    ++accepted;
                } else {
                    ++rejected;
                }
            }
        });
    }

    for (auto & thread : threads) {
        thread.join();
    }

    const auto stats = manager.stats();
    check(
        "[CONC] 总接收数正确",
        stats.received == static_cast<std::uint64_t>(kThreads * kPerThread),
        std::to_string(stats.received));
    check(
        "[CONC] 接受 + 拒绝 = 总数",
        accepted + rejected == kThreads * kPerThread,
        std::to_string(accepted) + "+" + std::to_string(rejected));
    check(
        "[CONC] 队列深度未超上限",
        manager.queue_depth() <= 3);
    check(
        "[CONC] 去重未误伤不同文本",
        stats.duplicates == 0);
    check(
        "[CONC] 峰值深度记录合理",
        stats.max_queue_depth <= 3);
}

// -------- 顺序与去重组合（模拟 worker）--------

void test_ordering()
{
    InteractionManager manager(3, 1200);

    manager.submit("脑机接口是什么", 0);
    manager.submit("IMU是什么", 1);
    manager.submit("多模态融合是什么", 2);

    InteractionRequest first;
    InteractionRequest second;
    InteractionRequest third;

    const bool ok1 = manager.try_pop(first);
    const bool ok2 = manager.try_pop(second);
    const bool ok3 = manager.try_pop(third);

    check(
        "[ORDER] FIFO 顺序与提交一致",
        ok1 && ok2 && ok3 &&
        first.text == "脑机接口是什么" &&
        second.text == "IMU是什么" &&
        third.text == "多模态融合是什么");

    check(
        "[ORDER] request_id 单调递增",
        first.request_id < second.request_id &&
        second.request_id < third.request_id);
}

// -------- P0-20：动作等待状态与去重缓存有界 --------

void test_action_waiting_and_dedupe_bounds()
{
    check(
        "[P020] PROCESSING -> ACTION_WAITING 合法",
        InteractionManager::can_transition(
            InteractionStatus::PROCESSING,
            InteractionStatus::ACTION_WAITING));

    check(
        "[P020] ACTION_WAITING -> COMPLETED 合法",
        InteractionManager::can_transition(
            InteractionStatus::ACTION_WAITING,
            InteractionStatus::COMPLETED));

    check(
        "[P020] ACTION_WAITING -> PROCESSING 非法",
        !InteractionManager::can_transition(
            InteractionStatus::ACTION_WAITING,
            InteractionStatus::PROCESSING));

    InteractionManager manager(64, 100);
    InteractionRequest request;

    // 连续提交大量不同文本（跨越多个去重窗口），缓存必须保持有界。
    for (int i = 0; i < 400; ++i) {
        manager.submit("测试文本" + std::to_string(i), 1000 + i * 200);
        while (manager.try_pop(request)) {
            manager.mark_status(
                request.request_id,
                InteractionStatus::COMPLETED,
                1000 + i * 200 + 1);
        }
    }

    const std::size_t cache_size = manager.dedupe_history_size();

    check(
        "[P020] 去重缓存有界（<64 且远小于 400）",
        cache_size < 64,
        "size=" + std::to_string(cache_size));

    // 过期条目被清理后，早期文本可以再次被接受。
    const auto again = manager.submit("测试文本0", 1000 + 400 * 200);
    check(
        "[P020] 过期去重记录可再次接受",
        again.accepted);
}

}  // namespace

int main()
{
    test_filter();
    test_normalize();
    test_dedupe();
    test_queue();
    test_state_machine();
    test_stop();
    test_concurrency();
    test_ordering();
    test_action_waiting_and_dedupe_bounds();

    std::cout << "\nTotal: " << (passed + failed)
              << " | passed: " << passed
              << " | failed: " << failed << '\n';

    return failed > 0 ? 1 : 0;
}
