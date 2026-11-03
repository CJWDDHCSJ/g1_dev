#include "g1_voice_router/interaction_manager.hpp"

#include <algorithm>

namespace g1_voice_router
{

InteractionManager::InteractionManager(
    std::size_t max_pending,
    long dedupe_window_ms)
    : max_pending_(max_pending == 0 ? 1 : max_pending)
    , dedupe_window_ms_(dedupe_window_ms < 0 ? 0 : dedupe_window_ms)
{
}

InteractionManager::~InteractionManager()
{
    stop();
}

AdmissionResult InteractionManager::submit(
    const std::string & text,
    long now_ms)
{
    AdmissionResult result;

    std::string reason;
    const TextVerdict verdict = classify_text(text, reason);

    if (verdict != TextVerdict::ACCEPT) {
        std::lock_guard<std::mutex> lock(mutex_);
        ++stats_.received;
        ++stats_.filtered;
        result.accepted = false;
        result.status = InteractionStatus::FILTERED;
        result.reason = reason;
        result.queue_depth = queue_.size();
        return result;
    }

    const std::string normalized = normalize_text(text);

    std::lock_guard<std::mutex> lock(mutex_);

    ++stats_.received;

    if (stopping_) {
        result.accepted = false;
        result.status = InteractionStatus::REJECTED;
        result.reason = "STOPPING";
        result.queue_depth = queue_.size();
        return result;
    }

    // 去重缓存有界化：清掉窗口外的历史，避免长期运行无限增长。
    if (last_accepted_ms_.size() >= 64 || (++dedupe_prune_counter_ % 32) == 0) {
        const long cutoff = now_ms - dedupe_window_ms_ * 2;

        for (auto it = last_accepted_ms_.begin(); it != last_accepted_ms_.end();) {
            if (it->second < cutoff) {
                it = last_accepted_ms_.erase(it);
            } else {
                ++it;
            }
        }
    }

    // 去重：同一归一化文本在窗口内只接受一次。
    const auto it = last_accepted_ms_.find(normalized);

    if (it != last_accepted_ms_.end() &&
        now_ms - it->second <= dedupe_window_ms_)
    {
        ++stats_.duplicates;
        result.accepted = false;
        result.status = InteractionStatus::DUPLICATE;
        result.reason = "DUPLICATE_WINDOW";
        result.queue_depth = queue_.size();
        return result;
    }

    // 有界队列：满则拒绝最新请求（不丢弃正在处理与已排队请求）。
    if (queue_.size() >= max_pending_) {
        ++stats_.rejected_full;
        result.accepted = false;
        result.status = InteractionStatus::REJECTED;
        result.reason = "QUEUE_FULL";
        result.queue_depth = queue_.size();
        return result;
    }

    InteractionRequest request;
    request.request_id = next_request_id_++;
    request.text = text;
    request.normalized = normalized;
    request.status = InteractionStatus::QUEUED;
    request.received_ms = now_ms;
    request.queued_ms = now_ms;

    last_accepted_ms_[normalized] = now_ms;
    status_map_[request.request_id] = InteractionStatus::QUEUED;

    queue_.push_back(request);

    ++stats_.accepted;
    stats_.max_queue_depth = std::max(stats_.max_queue_depth, queue_.size());

    result.accepted = true;
    result.status = InteractionStatus::QUEUED;
    result.request_id = request.request_id;
    result.queue_depth = queue_.size();

    cv_.notify_one();
    return result;
}

bool InteractionManager::wait_pop(InteractionRequest & out)
{
    std::unique_lock<std::mutex> lock(mutex_);

    cv_.wait(lock, [this]() {
        return stopping_ || !queue_.empty();
    });

    if (queue_.empty()) {
        // 停机且队列已空。
        return false;
    }

    out = queue_.front();
    queue_.pop_front();

    out.status = InteractionStatus::PROCESSING;
    out.started_ms = out.queued_ms;
    status_map_[out.request_id] = InteractionStatus::PROCESSING;

    return true;
}

std::size_t InteractionManager::pending_count() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
}

bool InteractionManager::try_pop(InteractionRequest & out)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (queue_.empty()) {
        return false;
    }

    out = queue_.front();
    queue_.pop_front();
    out.status = InteractionStatus::PROCESSING;
    out.started_ms = out.queued_ms;
    status_map_[out.request_id] = InteractionStatus::PROCESSING;
    return true;
}

bool InteractionManager::can_transition(
    InteractionStatus from,
    InteractionStatus to)
{
    switch (from) {
        case InteractionStatus::RECEIVED:
            return to == InteractionStatus::QUEUED ||
                   to == InteractionStatus::FILTERED ||
                   to == InteractionStatus::DUPLICATE ||
                   to == InteractionStatus::REJECTED;
        case InteractionStatus::QUEUED:
            return to == InteractionStatus::PROCESSING ||
                   to == InteractionStatus::REJECTED;
        case InteractionStatus::PROCESSING:
            return to == InteractionStatus::WAITING_OLLAMA ||
                   to == InteractionStatus::ACTION_WAITING ||
                   to == InteractionStatus::REPLY_READY ||
                   to == InteractionStatus::COMPLETED ||
                   to == InteractionStatus::FAILED ||
                   to == InteractionStatus::TIMEOUT;
        case InteractionStatus::ACTION_WAITING:
            return to == InteractionStatus::REPLY_READY ||
                   to == InteractionStatus::COMPLETED ||
                   to == InteractionStatus::FAILED ||
                   to == InteractionStatus::TIMEOUT;
        case InteractionStatus::WAITING_OLLAMA:
            return to == InteractionStatus::REPLY_READY ||
                   to == InteractionStatus::COMPLETED ||
                   to == InteractionStatus::FAILED ||
                   to == InteractionStatus::TIMEOUT;
        case InteractionStatus::REPLY_READY:
            return to == InteractionStatus::SPEAKING ||
                   to == InteractionStatus::COMPLETED ||
                   to == InteractionStatus::FAILED;
        case InteractionStatus::SPEAKING:
            return to == InteractionStatus::COMPLETED;
        case InteractionStatus::COMPLETED:
        case InteractionStatus::FAILED:
        case InteractionStatus::TIMEOUT:
        case InteractionStatus::REJECTED:
        case InteractionStatus::FILTERED:
        case InteractionStatus::DUPLICATE:
            return false;
        default:
            return false;
    }
}

InteractionStatus InteractionManager::status_of_locked(
    std::uint64_t request_id) const
{
    const auto it = status_map_.find(request_id);

    if (it != status_map_.end()) {
        return it->second;
    }

    return InteractionStatus::REJECTED;
}

bool InteractionManager::mark_status(
    std::uint64_t request_id,
    InteractionStatus next,
    long now_ms,
    const std::string & note)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (status_map_.find(request_id) == status_map_.end()) {
        return false;
    }

    const InteractionStatus current = status_of_locked(request_id);

    if (!can_transition(current, next)) {
        return false;
    }

    status_map_[request_id] = next;

    if (next == InteractionStatus::COMPLETED) {
        ++stats_.completed;
        status_map_.erase(request_id);
    } else if (next == InteractionStatus::FAILED) {
        ++stats_.failed;
        status_map_.erase(request_id);
    } else if (next == InteractionStatus::TIMEOUT) {
        ++stats_.timed_out;
        status_map_.erase(request_id);
    } else if (next == InteractionStatus::REJECTED) {
        status_map_.erase(request_id);
    }

    (void)now_ms;
    (void)note;
    return true;
}

void InteractionManager::stop()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;

        // 等待中的请求全部标记为 REJECTED（不静默丢弃，有明确终态）。
        for (auto & request : queue_) {
            status_map_.erase(request.request_id);
        }

        queue_.clear();
    }

    cv_.notify_all();
}

bool InteractionManager::stopping() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return stopping_;
}

InteractionStats InteractionManager::stats() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

std::size_t InteractionManager::queue_depth() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
}

void InteractionManager::clear_dedupe_history()
{
    std::lock_guard<std::mutex> lock(mutex_);
    last_accepted_ms_.clear();
}

std::size_t InteractionManager::dedupe_history_size() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return last_accepted_ms_.size();
}

}  // namespace g1_voice_router
