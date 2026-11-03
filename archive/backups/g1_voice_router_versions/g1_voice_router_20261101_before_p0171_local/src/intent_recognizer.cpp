#include "g1_voice_router/intent_recognizer.hpp"

#include <algorithm>
#include <cctype>
#include <initializer_list>
#include <string>
#include <vector>

namespace g1_voice_router
{
namespace
{

// 检查文本是否包含指定词语中的任意一个
bool contains_any(
    const std::string &text,
    std::initializer_list<std::string> keywords)
{
    for (const auto &keyword : keywords)
    {
        if (text.find(keyword) != std::string::npos)
        {
            return true;
        }
    }

    return false;
}

}  // namespace


// ============================================================
// 文本归一化
// ============================================================

std::string IntentRecognizer::normalize(const std::string &text)
{
    std::string result = text;

    // 去除中英文标点、空格和换行
    static const std::vector<std::string> punctuation = {
        "。", "！", "？", "，", "、", "；", "：",
        "“", "”", "‘", "’",
        ".", ",", "!", "?", ";", ":",
        "\"", "'",
        " ", "\t", "\r", "\n"
    };

    for (const auto &mark : punctuation)
    {
        std::size_t pos = 0;

        while ((pos = result.find(mark, pos)) != std::string::npos)
        {
            result.erase(pos, mark.size());
        }
    }

    // 只转换 ASCII 英文字母，不改变 UTF-8 中文字节
    std::transform(
        result.begin(),
        result.end(),
        result.begin(),
        [](unsigned char c)
        {
            if (c < 0x80)
            {
                return static_cast<char>(std::tolower(c));
            }

            return static_cast<char>(c);
        });

    return result;
}


// ============================================================
// 意图识别
// ============================================================

IntentResult IntentRecognizer::recognize(
    const std::string &text) const
{
    IntentResult result;

    result.normalized_text = normalize(text);

    const std::string &input = result.normalized_text;

    // 空文本按普通问题处理，后续由上层决定是否忽略
    if (input.empty())
    {
        result.intent = Intent::GENERAL_QA;
        return result;
    }


    // --------------------------------------------------------
    // 1. GREETING：问候
    // 只匹配独立问候语，避免误判复合问题
    // --------------------------------------------------------

    if (input == "你好" ||
        input == "您好" ||
        input == "你好啊" ||
        input == "您好啊" ||
        input == "嗨" ||
        input == "嗨嗨" ||
        input == "哈喽" ||
        input == "hello" ||
        input == "hi" ||
        input == "早上好" ||
        input == "早安" ||
        input == "上午好" ||
        input == "下午好" ||
        input == "晚上好")
    {
        result.intent = Intent::GREETING;
        return result;
    }


    // --------------------------------------------------------
    // 2. GOODBYE：告别
    // 同样先匹配明确的独立告别语
    // --------------------------------------------------------

    if (input == "再见" ||
        input == "拜拜" ||
        input == "谢谢" ||
        input == "谢谢你" ||
        input == "辛苦了" ||
        input == "结束讲解" ||
        input == "结束接待" ||
        input == "讲解结束" ||
        input == "到此为止" ||
        input == "就到这里")
    {
        result.intent = Intent::GOODBYE;
        return result;
    }


    // --------------------------------------------------------
    // 3. DEMO_ACTION：显式动作动词 + 动作宾语。
    // “展示/演示 + 机构名”是信息请求，不触发动作；
    // 因此动作门必须要求宾语，而不是把所有展示词提前。
    // --------------------------------------------------------

    const bool action_object =
        contains_any(input, {
            "动作", "挥手", "挥一挥手", "比心", "跳个舞", "举起双手"
        });

    const bool action_request =
        contains_any(input, {
            "表演", "演示", "展示", "来一个", "做一个",
            "来个", "做个"
        });

    if (action_request && action_object)
    {
        result.intent = Intent::DEMO_ACTION;
        return result;
    }


    // --------------------------------------------------------
    // 4. 通用概念解释：概念词 + 定义问法且无实验室语境时，
    // 属于普通技术问题，交给大模型，而不是本地知识库。
    // --------------------------------------------------------

    const bool general_concept =
        contains_any(input, {
            "脑机接口", "具身智能", "embodiedai"
        });

    const bool definition_marker =
        contains_any(input, {
            "是什么", "是什么意思", "什么意思", "是啥", "什么叫",
            "什么是", "定义", "概念", "指什么", "指的是",
            "介绍", "讲解"
        });

    const bool lab_context =
        contains_any(input, {
            "实验室", "学院", "中心", "重点实验室", "开放平台",
            "研究方向", "如何开展", "研究进展", "团队", "设备",
            "课题组", "赛力斯"
        });

    if (general_concept && definition_marker && !lab_context)
    {
        result.intent = Intent::GENERAL_QA;
        return result;
    }


    // --------------------------------------------------------
    // 5. ROBOT_STATUS：显式状态查询先于实验室词判断。
    // 即使句中出现“实验室”等词，明确的机器人状态问题
    // 也要识别为状态查询。
    // --------------------------------------------------------

    const bool lab_topic_state =
        contains_any(input, {
            "研究状态", "项目状态", "学习状态", "建设状态"
        });

    if (!lab_topic_state &&
        contains_any(input, {
            "你现在状态", "你当前状态", "你的状态", "你现在运行",
            "机器人状态", "机器人现在", "运行状态", "运行情况",
            "工作状态", "工作情况", "当前状态", "现在状态",
            "状态怎么样", "状态如何", "运行正常吗", "工作正常吗",
            "有没有故障", "有故障吗", "当前模式", "运行模式",
            "运动模式", "关节状态", "动作状态", "当前任务",
            "电量"
        }))
    {
        result.intent = Intent::ROBOT_STATUS;
        return result;
    }


    // --------------------------------------------------------
    // 6. LAB_INTRO：实验室相关问题
    // --------------------------------------------------------

    if (contains_any(input, {
        "实验室",
        "课题组",
        "研究方向",
        "科研项目",
        "实验设备",
        "实验成果",
        "脑机",
        "具身智能",
        "重点实验室",
        "开放平台",
        "赛力斯",
        "在研项目",
        "揭榜挂帅",
        "科研团队",
        "学院有哪些团队",
        "学院有哪些专业",
        "学院有什么专业",
        "学院开设哪些专业",
        "学院专业",
        "学院的专业",
        "学院学生拿过什么奖",
        "学院竞赛",
        "学院什么时候成立",
        "学院成立",
        "学院地址",
        "学院在哪里",
        "学院主要研究",
        "肖汉光",
        "刘宏",
        "有机器狗吗"
    }))
    {
        result.intent = Intent::LAB_INTRO;
        return result;
    }


    // --------------------------------------------------------
    // 7. ROBOT_STATUS：其余状态相关说法（兜底，较宽）
    // --------------------------------------------------------

    if (contains_any(input, {
        "状态",
        "运行情况",
        "工作情况",
        "当前模式",
        "运行模式",
        "运动模式",
        "姿态",
        "关节状态",
        "动作状态",
        "有没有故障",
        "有故障吗",
        "运行正常吗",
        "正常吗",
        "当前任务"
    }))
    {
        result.intent = Intent::ROBOT_STATUS;
        return result;
    }


    // --------------------------------------------------------
    // 8. DEMO_ACTION：动作演示（其余说法）
    // --------------------------------------------------------

    if (contains_any(input, {
        "给我们展示一下",
        "展示一下",
        "演示一下",
        "表演一下",
        "给我们表演",
        "动作演示",
        "做个动作",
        "来个动作",
        "挥挥手",
        "挥手",
        "比心",
        "比个心",
        "举起双手",
        "跳个舞"
    }))
    {
        result.intent = Intent::DEMO_ACTION;
        return result;
    }


    // --------------------------------------------------------
    // 9. ROBOT_INTRO：机器人介绍
    // --------------------------------------------------------

    if (contains_any(input, {
        "你是谁",
        "你叫什么",
        "介绍你自己",
        "介绍一下你",
        "介绍一下自己",
        "你能做什么",
        "你会做什么",
        "你有什么功能",
        "你有什么能力",
        "你的功能",
        "你的能力",
        "你是什么机器人",
        "这是什么机器人",
        "介绍一下机器人",
        "机器人能做什么",
        "机器人有什么功能",
        "介绍一下g1",
        "g1能做什么",
        "宇树g1是什么"
    }))
    {
        result.intent = Intent::ROBOT_INTRO;
        return result;
    }


    // --------------------------------------------------------
    // 10. GENERAL_QA：其他问题交给大模型
    // --------------------------------------------------------

    result.intent = Intent::GENERAL_QA;

    return result;
}


// ============================================================
// 意图名称转换
// ============================================================

const char *IntentRecognizer::to_string(Intent intent) noexcept
{
    switch (intent)
    {
        case Intent::GREETING:
            return "GREETING";

        case Intent::LAB_INTRO:
            return "LAB_INTRO";

        case Intent::ROBOT_INTRO:
            return "ROBOT_INTRO";

        case Intent::DEMO_ACTION:
            return "DEMO_ACTION";

        case Intent::ROBOT_STATUS:
            return "ROBOT_STATUS";

        case Intent::GOODBYE:
            return "GOODBYE";

        case Intent::GENERAL_QA:
            return "GENERAL_QA";

        default:
            return "UNKNOWN";
    }
}

}  // namespace g1_voice_router
