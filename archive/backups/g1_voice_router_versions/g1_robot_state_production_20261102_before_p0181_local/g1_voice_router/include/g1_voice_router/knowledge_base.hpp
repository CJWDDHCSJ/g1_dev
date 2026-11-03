#ifndef G1_VOICE_ROUTER_KNOWLEDGE_BASE_HPP_
#define G1_VOICE_ROUTER_KNOWLEDGE_BASE_HPP_

#include <cstddef>
#include <string>

#include <nlohmann/json.hpp>

namespace g1_voice_router
{

struct KnowledgeResult
{
    bool found{false};
    std::string category;
    std::string matched_id;
    // 命中原因：exact_question、contained_question、
    // entity_intent_question、faq_keyword、entry_keyword、
    // category_keyword、clarification、fallback。
    std::string match_type;
    int match_score{0};
    std::string answer;
};

class KnowledgeBase
{
public:
    // 加载并校验知识库；成功后常驻内存。
    bool load(
        const std::string &file_path,
        std::string &error);

    // 依次匹配 FAQ 精确问题、输入中包含的 FAQ 问题、
    // FAQ 关键词、条目关键词、类别关键词；全部失败时返回兜底。
    KnowledgeResult query(const std::string &user_text) const;

    bool loaded() const noexcept;

    std::size_t faq_count() const noexcept;
    std::size_t direction_count() const noexcept;
    std::size_t project_count() const noexcept;
    std::size_t equipment_count() const noexcept;

private:
    KnowledgeResult fallback_result(
        const std::string &match_type = "fallback") const;

    nlohmann::json data_;
    bool loaded_{false};
};

}  // namespace g1_voice_router

#endif  // G1_VOICE_ROUTER_KNOWLEDGE_BASE_HPP_
