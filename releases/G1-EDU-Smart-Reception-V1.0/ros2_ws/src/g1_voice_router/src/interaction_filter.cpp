#include "g1_voice_router/interaction_manager.hpp"

#include <array>
#include <cstddef>
#include <string>

namespace g1_voice_router
{
namespace
{

bool is_space(unsigned char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

// 中英文标点（去首尾 + 归一化时统一删除）。
const char * kPunctuation[] = {
    "。", "！", "？", "，", "、", "；", "：", "“", "”", "‘", "’",
    "（", "）", "…", "～",
    ".", ",", "!", "?", ";", ":", "\"", "'", "(", ")",
    "-", "_", "~", "/", "\\", "|",
    nullptr
};

bool is_punctuation_at(const std::string & text, std::size_t pos, std::size_t & length)
{
    for (const char ** mark = kPunctuation; *mark != nullptr; ++mark) {
        const std::string token(*mark);
        if (text.compare(pos, token.size(), token) == 0) {
            length = token.size();
            return true;
        }
    }
    return false;
}

// 明确噪声短语（归一化后完全相等才算）。
const char * kNoisePhrases[] = {
    "嗯", "啊", "呃", "唔", "哦", "额",
    "嗯嗯", "啊啊", "呃呃", "嗯啊", "哦哦",
    nullptr
};

bool is_noise_only_chars(const std::string & text)
{
    // 仅由语气词字符组成且长度 <= 4 时视为噪声（如"嗯嗯嗯"）。
    static const std::string noise_chars = "嗯啊呃唔哦额";

    std::size_t count = 0;
    std::size_t i = 0;

    while (i < text.size()) {
        const unsigned char byte = static_cast<unsigned char>(text[i]);
        std::size_t char_len = 1;

        if (byte >= 0xF0) {
            char_len = 4;
        } else if (byte >= 0xE0) {
            char_len = 3;
        } else if (byte >= 0xC0) {
            char_len = 2;
        }

        if (i + char_len > text.size()) {
            return false;
        }

        const std::string ch = text.substr(i, char_len);

        if (noise_chars.find(ch) == std::string::npos) {
            return false;
        }

        ++count;
        i += char_len;

        if (count > 4) {
            return false;
        }
    }

    return count > 0;
}

}  // namespace

std::string normalize_text(const std::string & text)
{
    // 1. 删除首尾空白与标点（迭代直到稳定）。
    std::string trimmed = text;

    bool changed = true;
    while (changed && !trimmed.empty()) {
        changed = false;

        while (!trimmed.empty() &&
               is_space(static_cast<unsigned char>(trimmed.front())))
        {
            trimmed.erase(trimmed.begin());
            changed = true;
        }

        while (!trimmed.empty() &&
               is_space(static_cast<unsigned char>(trimmed.back())))
        {
            trimmed.pop_back();
            changed = true;
        }

        std::size_t mark_len = 0;

        if (!trimmed.empty() && is_punctuation_at(trimmed, 0, mark_len)) {
            trimmed.erase(0, mark_len);
            changed = true;
            continue;
        }

        if (!trimmed.empty()) {
            // 尾部标点：从最后一个 UTF-8 字符起点尝试匹配。
            std::size_t start = trimmed.size() - 1;
            while (start > 0 &&
                   (static_cast<unsigned char>(trimmed[start]) & 0xC0) == 0x80)
            {
                --start;
            }

            if (is_punctuation_at(trimmed, start, mark_len) &&
                start + mark_len == trimmed.size())
            {
                trimmed.erase(start, mark_len);
                changed = true;
            }
        }
    }

    // 2. 删除内部空白与标点（用于比较，不分词）。
    std::string result;
    result.reserve(trimmed.size());

    for (std::size_t i = 0; i < trimmed.size();) {
        const unsigned char byte = static_cast<unsigned char>(trimmed[i]);

        if (is_space(byte)) {
            ++i;
            continue;
        }

        std::size_t mark_len = 0;
        if (is_punctuation_at(trimmed, i, mark_len)) {
            i += mark_len;
            continue;
        }

        char c = static_cast<char>(byte);
        if (byte >= 'A' && byte <= 'Z') {
            c = static_cast<char>(byte - 'A' + 'a');
        }

        result.push_back(c);
        ++i;
    }

    return result;
}

TextVerdict classify_text(
    const std::string & text,
    std::string & reason)
{
    if (text.empty()) {
        reason = "EMPTY_TEXT";
        return TextVerdict::FILTERED_EMPTY;
    }

    bool has_non_space = false;

    for (char c : text) {
        if (!is_space(static_cast<unsigned char>(c))) {
            has_non_space = true;
            break;
        }
    }

    if (!has_non_space) {
        reason = "WHITESPACE_ONLY";
        return TextVerdict::FILTERED_EMPTY;
    }

    const std::string normalized = normalize_text(text);

    if (normalized.empty()) {
        // 纯标点文本。
        reason = "PUNCTUATION_ONLY";
        return TextVerdict::FILTERED_EMPTY;
    }

    for (const char ** phrase = kNoisePhrases; *phrase != nullptr; ++phrase) {
        if (normalized == *phrase) {
            reason = "NOISE_PHRASE";
            return TextVerdict::FILTERED_NOISE;
        }
    }

    if (is_noise_only_chars(normalized)) {
        reason = "NOISE_CHARS_REPEATED";
        return TextVerdict::FILTERED_NOISE;
    }

    reason.clear();
    return TextVerdict::ACCEPT;
}

const char * to_string(InteractionStatus status)
{
    switch (status) {
        case InteractionStatus::RECEIVED: return "RECEIVED";
        case InteractionStatus::FILTERED: return "FILTERED";
        case InteractionStatus::DUPLICATE: return "DUPLICATE";
        case InteractionStatus::QUEUED: return "QUEUED";
        case InteractionStatus::PROCESSING: return "PROCESSING";
        case InteractionStatus::WAITING_OLLAMA: return "WAITING_OLLAMA";
        case InteractionStatus::ACTION_WAITING: return "ACTION_WAITING";
        case InteractionStatus::REPLY_READY: return "REPLY_READY";
        case InteractionStatus::SPEAKING: return "SPEAKING";
        case InteractionStatus::COMPLETED: return "COMPLETED";
        case InteractionStatus::FAILED: return "FAILED";
        case InteractionStatus::TIMEOUT: return "TIMEOUT";
        case InteractionStatus::REJECTED: return "REJECTED";
        default: return "UNKNOWN";
    }
}

}  // namespace g1_voice_router
