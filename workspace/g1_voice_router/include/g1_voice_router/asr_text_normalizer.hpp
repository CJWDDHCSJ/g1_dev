#ifndef G1_VOICE_ROUTER_ASR_TEXT_NORMALIZER_HPP_
#define G1_VOICE_ROUTER_ASR_TEXT_NORMALIZER_HPP_

#include <string>

namespace g1_voice_router
{

// ASR 文本归一化结果（P0-21）。
// raw_text 永远保留原始识别文本，normalized_text 仅在命中
// 明确别名规则时与 raw_text 不同。
struct AsrNormalization
{
    std::string raw_text;        // 原始 ASR 文本（不得被覆盖）
    std::string normalized_text; // 归一化后的文本
    bool changed{false};
    std::string rule_id;         // 命中的规则编号，未命中为空
    std::string reason;          // 未命中的原因（便于日志排查）
};

// 有限的 ASR 专有名词归一化。
//
// 设计边界（必须保持）：
//  - 只做词典式替换，不使用模型、不做编辑距离/模糊匹配；
//  - 只修正有真实 ASR 日志证据的专有名词；
//  - 含否定/停止/取消等控制词时整体跳过，绝不改写成可执行命令；
//  - 不触碰数字，不猜测人名等不确定专有词；
//  - 原始文本始终保留。
class AsrTextNormalizer
{
public:
    // 对一条 ASR 文本做归一化。
    AsrNormalization normalize(const std::string & raw_text) const;

    // 原始文本的安全摘要（用于日志，不输出完整语音内容）。
    static std::string digest(const std::string & text);
};

}  // namespace g1_voice_router

#endif  // G1_VOICE_ROUTER_ASR_TEXT_NORMALIZER_HPP_
