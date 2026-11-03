#include <curl/curl.h>
#include <algorithm>
#include <ament_index_cpp/get_package_share_directory.hpp>
#include <nlohmann/json.hpp>

#include <atomic>
#include <cctype>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <iterator>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>

#include "g1_voice_router/action_command_mapper.hpp"
#include "g1_voice_router/asr_text_normalizer.hpp"
#include "g1_voice_router/fact_risk_guard.hpp"
#include "g1_voice_router/intent_recognizer.hpp"
#include "g1_voice_router/interaction_manager.hpp"
#include "g1_voice_router/knowledge_base.hpp"
#include "g1_voice_router/robot_state_evaluator.hpp"
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"

using Json = nlohmann::json;

class G1VoiceRouter : public rclcpp::Node
{
public:
  G1VoiceRouter()
  : Node("g1_voice_router")
  {
    ollama_url_ = declare_parameter<std::string>(
      "ollama_url",
      "http://192.168.123.99:11434/api/chat");

    model_name_ = declare_parameter<std::string>(
      "model",
      "qwen3-vl:4b-instruct");

    timeout_seconds_ = declare_parameter<int>(
      "request_timeout_seconds", 60);

    if (timeout_seconds_ < 5 || timeout_seconds_ > 300) {
      RCLCPP_WARN(
        get_logger(),
        "Invalid request timeout; using 60 seconds");
      timeout_seconds_ = 60;
    }

    std::string default_knowledge_file;

    try {
      default_knowledge_file =
        ament_index_cpp::get_package_share_directory(
          "g1_voice_router") +
        "/config/lab_knowledge.json";
    } catch (const std::exception & error) {
      RCLCPP_ERROR(
        get_logger(),
        "Failed to locate g1_voice_router share directory: %s",
        error.what());
    }

    knowledge_file_ = declare_parameter<std::string>(
      "knowledge_file",
      default_knowledge_file);

    std::string knowledge_error;

    if (!knowledge_base_.load(knowledge_file_, knowledge_error)) {
      RCLCPP_ERROR(
        get_logger(),
        "KnowledgeBase load failed: %s",
        knowledge_error.c_str());
    } else {
      RCLCPP_INFO(
        get_logger(),
        "KnowledgeBase loaded: %s "
        "(FAQ=%zu, directions=%zu, projects=%zu, equipment=%zu)",
        knowledge_file_.c_str(),
        knowledge_base_.faq_count(),
        knowledge_base_.direction_count(),
        knowledge_base_.project_count(),
        knowledge_base_.equipment_count());
    }

    system_prompt_ = declare_parameter<std::string>(
      "system_prompt",
      "你是运行在宇树G1人形机器人中的中文语音助手。"
      "用户输入通常来自语音识别，可能存在少量错字。"
      "请使用简体中文，回答自然、简洁，适合直接语音播报，"
      "通常回答一到三句话。不要使用Markdown、表格或代码块。"
      "你绝对不能编造以下内容：产品价格、人员数量、科研成果、"
      "项目进展、设备品牌型号规格、机器人实时状态。"
      "如果不确定或没有可靠依据，必须诚实说明你无法确认，"
      "不要给出任何猜测的具体数字、名称或型号。");

    // 保留最近的对话上下文，第一条始终是系统提示词。
    history_.push_back(Json{
      {"role", "system"},
      {"content", system_prompt_}
    });

    // 输出仍然使用已通过测试的 TTS 接口。
    publisher_ =
      create_publisher<std_msgs::msg::String>(
        "/g1/tts", 10);

    // 发布固定白名单动作请求给 g1_action_executor。
    action_publisher_ =
      create_publisher<std_msgs::msg::String>(
        "/g1/action_request", 10);


    // 输入仍然使用已通过测试的 ASR 桥接接口。
    subscription_ =
      create_subscription<std_msgs::msg::String>(
        "/g1/asr_text",
        10,
        [this](const std_msgs::msg::String::SharedPtr msg) {
          enqueue_user_text(msg->data);
        });

    action_timeout_seconds_ = declare_parameter<int>(
      "action_timeout_seconds", 45);

    // P0-20：Action Executor 结果订阅（std_msgs/String）。
    action_result_subscription_ =
      create_subscription<std_msgs::msg::String>(
        "/g1/action_result",
        10,
        [this](const std_msgs::msg::String::SharedPtr msg) {
          on_action_result(msg);
        });

    // RobotStateProvider 聚合状态（P0-18）。
    // Provider 不在线时本节点照常工作，ROBOT_STATUS 仍走安全兜底。
    robot_state_subscription_ =
      create_subscription<std_msgs::msg::String>(
        "/g1/robot_state",
        10,
        [this](const std_msgs::msg::String::SharedPtr msg) {
          std::lock_guard<std::mutex> lock(robot_state_mutex_);
          robot_state_json_ = msg->data;
          robot_state_recv_time_ = std::chrono::steady_clock::now();
          robot_state_received_ = true;
        });

    // 先建立订阅和发布接口，再启动工作线程。
    worker_ = std::thread(
      &G1VoiceRouter::worker_loop, this);

    RCLCPP_INFO(
      get_logger(),
      "G1 voice router started");

    RCLCPP_INFO(
      get_logger(),
      "Ollama URL: %s",
      ollama_url_.c_str());

    RCLCPP_INFO(
      get_logger(),
      "Model: %s",
      model_name_.c_str());

    RCLCPP_INFO(
      get_logger(),
      "Listening on /g1/asr_text, publishing to /g1/tts");
  }

  ~G1VoiceRouter() override
  {
    stop_worker();
  }

  // 退出时停止接收新任务，并等待正在进行的请求结束。
  void stop_worker()
  {
    // P0-20：先置停机标志（中断进行中的 HTTP 与动作等待），
    // 再停止管理器并唤醒 worker。
    shutdown_requested_.store(true);
    action_result_cv_.notify_all();

    interaction_manager_.stop();

    if (worker_.joinable()) {
      worker_.join();
    }
  }

private:
  // P0-20：HTTP 进度回调，停机时返回 1 中断传输。
  static int progress_callback(
    void * client,
    curl_off_t dltotal,
    curl_off_t dlnow,
    curl_off_t ultotal,
    curl_off_t ulnow)
  {
    (void)dltotal;
    (void)dlnow;
    (void)ultotal;
    (void)ulnow;

    auto * self = static_cast<G1VoiceRouter *>(client);

    if (self != nullptr && self->shutdown_requested_.load()) {
      return 1;
    }

    return 0;
  }

  static std::size_t write_callback(
    char * data,
    std::size_t size,
    std::size_t count,
    void * user_data)
  {
    const std::size_t bytes = size * count;

    auto * response =
      static_cast<std::string *>(user_data);

    response->append(data, bytes);
    return bytes;
  }

  static std::string trim(std::string text)
  {
    const auto not_space = [](unsigned char c) {
      return std::isspace(c) == 0;
    };

    const auto first = std::find_if(
      text.begin(), text.end(), not_space);

    const auto last = std::find_if(
      text.rbegin(), text.rend(), not_space).base();

    if (first >= last) {
      return {};
    }

    return std::string(first, last);
  }


  static std::string normalize_intent_text(std::string text)
  {
    text = trim(std::move(text));
  
    static const std::string punctuation[] = {
      "。", "！", "？", "，", "、", "；", "：",
      ".", ",", "!", "?", ";", ":",
      "\"", "'", "“", "”", " ",
      "\t", "\r", "\n"
    };
  
    for (const auto & mark : punctuation) {
      std::size_t pos = 0;
      while ((pos = text.find(mark, pos)) != std::string::npos) {
        text.erase(pos, mark.size());
      }
    }
  
    std::transform(
      text.begin(), text.end(), text.begin(),
      [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
      });
  
    return text;
  }
  
  static bool match_greeting_reply(
    const std::string & raw_text,
    std::string & reply)
  {
    const std::string text = normalize_intent_text(raw_text);
  
    if (text == "你好" ||
        text == "您好" ||
        text == "你好啊" ||
        text == "嗨" ||
        text == "嗨嗨" ||
        text == "哈喽" ||
        text == "hello" ||
        text == "hi") {
      reply = "你好！很高兴见到你。";
      return true;
    }
  
    if (text == "早上好" || text == "早安" || text == "上午好") {
      reply = "早上好！今天也一起加油。";
      return true;
    }
  
    if (text == "下午好") {
      reply = "下午好！很高兴见到你。";
      return true;
    }
  
    if (text == "晚上好") {
      reply = "晚上好！今天辛苦了。";
      return true;
    }
  
    return false;
  }
  

  static long steady_now_ms()
  {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count();
  }

  // P0-19：ASR 回调（ROS 线程）只做准入判定，不执行业务。
  // 过滤/去重/队列满在这里被明确记录，业务统一由 worker 串行处理。
  void enqueue_user_text(const std::string & raw_text)
  {
    const std::string raw = trim(raw_text);

    // P0-21：先做有限的专有名词归一化，再做过滤/去重/入队。
    // 归一化结果用于业务，原始文本只用于日志摘要与追溯。
    const auto normalization = asr_normalizer_.normalize(raw);
    const std::string text = normalization.normalized_text;

    const long now = steady_now_ms();

    const auto admission = interaction_manager_.submit(text, now);

    if (!admission.accepted) {
      RCLCPP_INFO(
        get_logger(),
        "event=%s reason=%s queue_depth=%zu text_len=%zu",
        g1_voice_router::to_string(admission.status),
        admission.reason.c_str(),
        admission.queue_depth,
        text.size());

      // 队列满：忙碌提示交给 worker 按请求顺序发布，
      // 不在 ROS 回调线程直接发 /g1/tts（避免插队）。
      if (admission.reason == "QUEUE_FULL") {
        std::lock_guard<std::mutex> lock(notice_mutex_);

        if (notices_.size() >= 2) {
          RCLCPP_WARN(
            get_logger(),
            "Busy notice queue full; dropping oldest notice");
          notices_.pop_front();
        }

        notices_.push_back(
          "我现在要回答的问题有点多，请你稍等一下再问一次。");
      }

      return;
    }

    RCLCPP_INFO(
      get_logger(),
      "request_id=%lu event=QUEUED queue_depth=%zu text_len=%zu",
      static_cast<unsigned long>(admission.request_id),
      admission.queue_depth,
      text.size());

    if (normalization.changed) {
      RCLCPP_INFO(
        get_logger(),
        "request_id=%lu event=ASR_NORMALIZED rule=%s raw_len=%zu "
        "raw_digest=%s normalized=%s",
        static_cast<unsigned long>(admission.request_id),
        normalization.rule_id.c_str(),
        normalization.raw_text.size(),
        g1_voice_router::AsrTextNormalizer::digest(
          normalization.raw_text).c_str(),
        normalization.normalized_text.c_str());
    } else {
      RCLCPP_INFO(
        get_logger(),
        "request_id=%lu event=ASR_NORMALIZATION_SKIPPED reason=%s",
        static_cast<unsigned long>(admission.request_id),
        normalization.reason.c_str());
    }
  }

  // P0-19：worker 串行处理一条请求（本地回答与 Ollama 回答统一在此发布，
  // 保证 /g1/tts 的提交顺序严格等于请求接收顺序）。
  void process_request(
    const g1_voice_router::InteractionRequest & request)
  {
    const std::string text = request.text;

    const auto intent_result =
      intent_recognizer_.recognize(text);

    RCLCPP_INFO(
      get_logger(),
      "request_id=%lu event=PROCESSING intent=%s text_len=%zu",
      static_cast<unsigned long>(request.request_id),
      g1_voice_router::IntentRecognizer::to_string(
        intent_result.intent),
      text.size());

    // 本地分支会提前 return；用 RAII 保证无论走哪条路径都能落到终态，
    // 避免请求停留在 PROCESSING 造成统计失真。
    struct CompletionGuard
    {
      g1_voice_router::InteractionManager & manager;
      std::uint64_t request_id;

      ~CompletionGuard()
      {
        const long now = G1VoiceRouter::steady_now_ms();

        if (!manager.mark_status(
              request_id,
              g1_voice_router::InteractionStatus::COMPLETED,
              now))
        {
          manager.mark_status(
            request_id,
            g1_voice_router::InteractionStatus::REPLY_READY,
            now);
          manager.mark_status(
            request_id,
            g1_voice_router::InteractionStatus::COMPLETED,
            now);
        }
      }
    } completion_guard{interaction_manager_, request.request_id};



      // 明确问候语走固定规则，不经过 Ollama。
      // 动作请求和 TTS 回复分别发布，由两个节点并行处理。
      std::string greeting_reply;
    
      if (intent_result.intent == g1_voice_router::Intent::GREETING &&
        match_greeting_reply(text, greeting_reply)) {
        std_msgs::msg::String action_msg;
        action_msg.data = "wave";
        action_publisher_->publish(action_msg);
    
        RCLCPP_INFO(
          get_logger(),
          "Greeting intent detected: [%s], published action [wave]",
          text.c_str());
    
        publish_reply(greeting_reply);
        return;
      }
    

    // --------------------------------------------------------
    // DEMO_ACTION：语音动作命令（P0-20）
    // 固定白名单、串行执行、等待 Action Result；
    // 不进入 Ollama，不执行白名单外动作。
    // --------------------------------------------------------

    if (intent_result.intent ==
        g1_voice_router::Intent::DEMO_ACTION)
    {
      handle_action_request(request, text);
      return;
    }

    // --------------------------------------------------------
    // LAB_INTRO：本地知识库
    // 有资料时直接回答；资料缺失时返回安全兜底。
    // 不进入 Ollama 消息队列。
    // --------------------------------------------------------

    if (intent_result.intent ==
        g1_voice_router::Intent::LAB_INTRO)
    {
      const auto result = knowledge_base_.query(text);

      publish_reply(result.answer);

      RCLCPP_INFO(
        get_logger(),
        "LAB_INTRO local knowledge base completed: "
        "found=%s category=%s matched_id=%s "
        "match_type=%s match_score=%d",
        result.found ? "true" : "false",
        result.category.c_str(),
        result.matched_id.c_str(),
        result.match_type.c_str(),
        result.match_score);

      return;
    }


    // --------------------------------------------------------
    // ROBOT_INTRO：机器人自我介绍
    // 本地固定回答，不调用 Ollama
    // --------------------------------------------------------

    if (intent_result.intent ==
        g1_voice_router::Intent::ROBOT_INTRO)
    {
      const std::string reply =
        "我是宇树 G1 EDU 人形机器人。"
        "我可以通过语音识别接收你的问题，"
        "连接外部大语言模型进行中文对话，"
        "还可以执行挥手、举手和比心等预设手臂动作。"
        "很高兴为你介绍我们的机器人系统。";

      publish_reply(reply);

      RCLCPP_INFO(
        get_logger(),
        "ROBOT_INTRO local handling completed");

      return;
    }


    // --------------------------------------------------------
    // GOODBYE：告别
    // 本地固定回答，不调用 Ollama
    // --------------------------------------------------------

    if (intent_result.intent ==
        g1_voice_router::Intent::GOODBYE)
    {
      const std::string reply =
        "感谢你的参观！"
        "很高兴和你交流，祝你今天愉快，再见！";

      publish_reply(reply);

      RCLCPP_INFO(
        get_logger(),
        "GOODBYE local handling completed");

      return;
    }


    // --------------------------------------------------------
    // ROBOT_STATUS：本地安全兜底（P0-17.1）。
    // RobotStateProvider 尚未开发，系统暂无真实状态数据来源。
    // 该意图不进入 Ollama 队列，直接发布明确的状态不可用提示。
    // 注意：FactRiskGuard 的 REALTIME_STATUS 规则完整保留，
    // 用于拦截 IntentRecognizer 误分类为 GENERAL_QA 的
    // 状态问题（第二道防线），两者互不替代。
    // --------------------------------------------------------

    if (intent_result.intent ==
        g1_voice_router::Intent::ROBOT_STATUS)
    {
      // P0-18：优先使用 RobotStateProvider 的真实状态数据；
      // Provider 未启动、缓存过期、JSON 解析失败时一律安全兜底。
      // 本分支任何路径都不会进入 Ollama 队列。
      std::string cached_json;
      bool cache_fresh = false;

      {
        std::lock_guard<std::mutex> lock(robot_state_mutex_);

        if (robot_state_received_) {
          const auto age_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::steady_clock::now() -
              robot_state_recv_time_).count();

          cache_fresh =
            age_ms <= g1_voice_router::kRobotStateCacheTimeoutMs;
          cached_json = robot_state_json_;
        }
      }

      g1_voice_router::AggregatedRobotState cached_state;
      const bool state_ready =
        cache_fresh &&
        g1_voice_router::RobotStateEvaluator::parse_robot_state_json(
          cached_json, cached_state);

      const std::string reply =
        g1_voice_router::RobotStateEvaluator::generate_status_answer(
          state_ready ? &cached_state : nullptr,
          text);

      publish_reply(reply);

      RCLCPP_INFO(
        get_logger(),
        "ROBOT_STATUS local handling completed: state_ready=%s",
        state_ready ? "true" : "false");

      return;
    }

    // --------------------------------------------------------
    // 事实风险保护层（P0-15）：
    // 在文本进入 Ollama 队列之前执行检查。
    // - PRICE / REALTIME_STATUS：直接安全兜底，不调用大模型。
    // - LAB_FACT：优先使用本地知识库可靠答案；
    //   无可靠资料时明确兜底，不交给大模型猜测。
    // - NORMAL：普通通用知识，照常进入 Ollama 队列。
    // --------------------------------------------------------

    const auto fact_risk =
      fact_risk_guard_.evaluate(text, &knowledge_base_);

    RCLCPP_INFO(
      get_logger(),
      "FactRiskGuard: risk=%s blocked=%s use_local=%s "
      "kb_match_type=%s kb_matched_id=%s kb_match_score=%d "
      "reason=%s | text: %s",
      g1_voice_router::to_string(fact_risk.risk),
      fact_risk.blocked ? "true" : "false",
      fact_risk.use_local_answer ? "true" : "false",
      fact_risk.kb_match_type.c_str(),
      fact_risk.kb_matched_id.c_str(),
      fact_risk.kb_match_score,
      fact_risk.reason.c_str(),
      text.c_str());

    if (fact_risk.blocked) {
      publish_reply(fact_risk.fallback_answer);

      RCLCPP_WARN(
        get_logger(),
        "Fact risk blocked before Ollama queue; "
        "risk=%s reason=%s",
        g1_voice_router::to_string(fact_risk.risk),
        fact_risk.reason.c_str());

      return;
    }

    if (fact_risk.use_local_answer) {
      publish_reply(fact_risk.local_answer);

      RCLCPP_INFO(
        get_logger(),
        "Fact risk handled by local knowledge base: "
        "matched_id=%s match_type=%s",
        fact_risk.kb_matched_id.c_str(),
        fact_risk.kb_match_type.c_str());

      return;
    }

    // --------------------------------------------------------
    // GENERAL_QA：调用 Ollama（在 worker 线程内串行执行，
    // 保证不同请求之间不会并发调用模型）
    // --------------------------------------------------------

    interaction_manager_.mark_status(
      request.request_id,
      g1_voice_router::InteractionStatus::WAITING_OLLAMA,
      steady_now_ms());

    RCLCPP_INFO(
      get_logger(),
      "request_id=%lu event=WAITING_OLLAMA",
      static_cast<unsigned long>(request.request_id));

    std::string reply;
    const bool ollama_ok = query_ollama(text, reply);

    if (!rclcpp::ok()) {
      interaction_manager_.mark_status(
        request.request_id,
        g1_voice_router::InteractionStatus::FAILED,
        steady_now_ms(),
        "NODE_SHUTDOWN");
      return;
    }

    if (ollama_ok) {
      interaction_manager_.mark_status(
        request.request_id,
        g1_voice_router::InteractionStatus::REPLY_READY,
        steady_now_ms());

      publish_reply(reply);

      interaction_manager_.mark_status(
        request.request_id,
        g1_voice_router::InteractionStatus::COMPLETED,
        steady_now_ms());

      RCLCPP_INFO(
        get_logger(),
        "request_id=%lu event=COMPLETED total_ms=%ld",
        static_cast<unsigned long>(request.request_id),
        steady_now_ms() - request.received_ms);

      return;
    }

    // 失败路径：标记 FAILED、发布兜底回复，然后继续处理后续请求。
    interaction_manager_.mark_status(
      request.request_id,
      g1_voice_router::InteractionStatus::FAILED,
      steady_now_ms(),
      "OLLAMA_ERROR");

    publish_reply("抱歉，我暂时无法连接到对话模型。");

    RCLCPP_WARN(
      get_logger(),
      "request_id=%lu event=FAILED reason=OLLAMA_ERROR fallback=published total_ms=%ld",
      static_cast<unsigned long>(request.request_id),
      steady_now_ms() - request.received_ms);
  }

  bool query_ollama(
    const std::string & user_text,
    std::string & answer)
  {
    Json messages = Json::array();

    for (const auto & message : history_) {
      messages.push_back(message);
    }

    messages.push_back(Json{
      {"role", "user"},
      {"content", user_text}
    });

    const Json request_json = {
      {"model", model_name_},
      {"messages", messages},
      {"stream", false},
      {"options", {
        {"temperature", 0.2},
        {"num_predict", 160}
      }}
    };

    const std::string request_body = request_json.dump();
    std::string response_body;

    CURL * curl = curl_easy_init();

    if (curl == nullptr) {
      RCLCPP_ERROR(
        get_logger(),
        "Failed to create CURL request");
      return false;
    }

    curl_slist * headers = nullptr;
    headers = curl_slist_append(
      headers, "Content-Type: application/json");

    if (headers == nullptr) {
      curl_easy_cleanup(curl);

      RCLCPP_ERROR(
        get_logger(),
        "Failed to create HTTP headers");
      return false;
    }

    curl_easy_setopt(
      curl, CURLOPT_URL, ollama_url_.c_str());

    curl_easy_setopt(
      curl, CURLOPT_HTTPHEADER, headers);

    curl_easy_setopt(
      curl, CURLOPT_POST, 1L);

    curl_easy_setopt(
      curl, CURLOPT_POSTFIELDS, request_body.c_str());

    curl_easy_setopt(
      curl, CURLOPT_POSTFIELDSIZE,
      static_cast<long>(request_body.size()));

    curl_easy_setopt(
      curl, CURLOPT_WRITEFUNCTION,
      &G1VoiceRouter::write_callback);

    curl_easy_setopt(
      curl, CURLOPT_WRITEDATA, &response_body);

    curl_easy_setopt(
      curl, CURLOPT_CONNECTTIMEOUT_MS, 3000L);

    curl_easy_setopt(
      curl, CURLOPT_TIMEOUT_MS,
      static_cast<long>(timeout_seconds_) * 1000L);

    // 在后台线程中执行同步 HTTP 请求。
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

    // P0-20：停机时可快速中断进行中的请求（避免拖过 systemd 停止期限）。
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(
      curl, CURLOPT_XFERINFOFUNCTION,
      &G1VoiceRouter::progress_callback);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, this);

    const CURLcode result = curl_easy_perform(curl);

    long http_status = 0;
    curl_easy_getinfo(
      curl, CURLINFO_RESPONSE_CODE, &http_status);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (result != CURLE_OK) {
      RCLCPP_ERROR(
        get_logger(),
        "Ollama HTTP request failed: %s",
        curl_easy_strerror(result));
      return false;
    }

    if (http_status < 200 || http_status >= 300) {
      const std::string detail =
        response_body.substr(0, 300);

      RCLCPP_ERROR(
        get_logger(),
        "Ollama HTTP status %ld; response: %s",
        http_status, detail.c_str());
      return false;
    }

    try {
      const Json response =
        Json::parse(response_body);

      if (!response.contains("message") ||
          !response["message"].is_object() ||
          !response["message"].contains("content") ||
          !response["message"]["content"].is_string()) {
        RCLCPP_ERROR(
          get_logger(),
          "Ollama response has no message.content field");
        return false;
      }

      answer = trim(
        response["message"]["content"].get<std::string>());

      if (answer.empty()) {
        RCLCPP_ERROR(
          get_logger(),
          "Ollama returned an empty answer");
        return false;
      }

      // 只有请求成功时才保存这轮对话。
      history_.push_back(Json{
        {"role", "user"},
        {"content", user_text}
      });

      history_.push_back(Json{
        {"role", "assistant"},
        {"content", answer}
      });

      // 保留系统提示词和最近最多六轮对话。
      while (history_.size() > 13) {
        history_.erase(std::next(history_.begin()));
        history_.erase(std::next(history_.begin()));
      }

      return true;
    } catch (const Json::exception & error) {
      RCLCPP_ERROR(
        get_logger(),
        "Failed to parse Ollama JSON: %s",
        error.what());
      return false;
    }
  }

  void on_action_result(const std_msgs::msg::String::SharedPtr msg)
  {
    {
      std::lock_guard<std::mutex> lock(action_result_mutex_);
      action_result_text_ = msg->data;
      action_result_ms_ = steady_now_ms();
      ++action_result_seq_;
    }

    action_result_cv_.notify_all();
  }

  static bool parse_action_result(
    const std::string & raw,
    const std::string & expected_name,
    std::string & outcome)
  {
    const std::size_t first = raw.find(':');

    if (first == std::string::npos) {
      return false;
    }

    const std::string kind = raw.substr(0, first);

    if (kind == "rejected") {
      outcome = "REJECTED";
      return true;
    }

    const std::size_t second = raw.find(':', first + 1);
    const std::string name =
      (second == std::string::npos)
      ? raw.substr(first + 1)
      : raw.substr(first + 1, second - first - 1);

    if (name != expected_name) {
      return false;
    }

    if (kind == "success") {
      outcome = "SUCCESS";
      return true;
    }

    if (kind == "failed") {
      outcome = "FAILED";
      return true;
    }

    if (kind == "timeout") {
      outcome = "TIMEOUT";
      return true;
    }

    return false;
  }

  // 等待与本次动作请求匹配的 Action Result。
  // 返回 SUCCESS / FAILED / TIMEOUT / REJECTED；超时未收到返回 TIMEOUT。
  std::string wait_action_result(
    const std::string & expected_name,
    long timeout_ms)
  {
    std::unique_lock<std::mutex> lock(action_result_mutex_);

    const std::uint64_t start_seq = action_result_seq_;
    const long deadline = steady_now_ms() + timeout_ms;

    while (true) {
      if (stop_requested()) {
        return "TIMEOUT";
      }

      const long now = steady_now_ms();
      const long remaining = deadline - now;

      if (remaining <= 0) {
        return "TIMEOUT";
      }

      action_result_cv_.wait_for(
        lock,
        std::chrono::milliseconds(remaining));

      if (action_result_seq_ != start_seq) {
        std::string outcome;

        if (parse_action_result(
              action_result_text_, expected_name, outcome))
        {
          return outcome;
        }
      }
    }
  }

  bool stop_requested() const
  {
    return shutdown_requested_.load() || !rclcpp::ok();
  }

  static const char * spoken_action_name(const std::string & name)
  {
    if (name == "wave") {
      return "挥手";
    }
    if (name == "wave_low") {
      return "轻轻挥手";
    }
    if (name == "hands_up") {
      return "举手";
    }
    if (name == "clap") {
      return "拍手";
    }
    if (name == "heart") {
      return "比心";
    }
    if (name == "victory") {
      return "比耶";
    }
    return "这个";
  }

  void publish_reply(const std::string & reply)
  {
    if (!rclcpp::ok()) {
      return;
    }

    std_msgs::msg::String output;
    output.data = reply;

    publisher_->publish(output);

    RCLCPP_INFO(
      get_logger(),
      "Reply published to /g1/tts (%zu bytes)",
      reply.size());
  }

  void drain_notices()
  {
    std::deque<std::string> pending;

    {
      std::lock_guard<std::mutex> lock(notice_mutex_);
      pending.swap(notices_);
    }

    for (const auto & notice : pending) {
      publish_reply(notice);
    }
  }

  // P0-20：语音动作命令处理（固定白名单，串行执行，等待执行结果）。
  void handle_action_request(
    const g1_voice_router::InteractionRequest & request,
    const std::string & text)
  {
    const auto action =
      action_mapper_.map(g1_voice_router::normalize_text(text));

    if (!action.matched) {
      std::string reply;

      if (action.match_type == "NEGATED") {
        reply = "好的，那我不做动作。";
      } else if (action.match_type == "QUESTION") {
        reply = std::string(
          g1_voice_router::ActionCommandMapper::capability_hint()) +
          "如果你想了解动作原理，也可以直接问我。";
      } else if (action.match_type == "DISABLED") {
        reply = "这个动作不在我的可执行列表里，我不能执行。";
      } else {
        reply = g1_voice_router::ActionCommandMapper::capability_hint();
      }

      publish_reply(reply);

      RCLCPP_INFO(
        get_logger(),
        "request_id=%lu event=ACTION_NOT_EXECUTED reason=%s",
        static_cast<unsigned long>(request.request_id),
        action.reject_reason.c_str());

      return;
    }

    if (stop_requested()) {
      return;
    }

    interaction_manager_.mark_status(
      request.request_id,
      g1_voice_router::InteractionStatus::ACTION_WAITING,
      steady_now_ms());

    std_msgs::msg::String action_msg;
    action_msg.data = action.action_name;
    action_publisher_->publish(action_msg);

    RCLCPP_INFO(
      get_logger(),
      "request_id=%lu event=ACTION_SENT action=%s id=%d",
      static_cast<unsigned long>(request.request_id),
      action.action_name.c_str(),
      action.action_id);

    const long timeout_ms =
      static_cast<long>(action_timeout_seconds_) * 1000L;

    const std::string outcome =
      wait_action_result(action.action_name, timeout_ms);

    std::string reply;

    if (outcome == "SUCCESS") {
      reply = std::string("好的，") +
        spoken_action_name(action.action_name) + "动作完成了。";
    } else if (outcome == "FAILED") {
      reply = "这个动作暂时没有执行成功。";
    } else if (outcome == "REJECTED") {
      reply = "动作执行器暂时不能执行这个动作，可能是当前状态不允许。";
    } else {
      reply = "动作执行结果暂时没有收到，我不会自动重复执行。";
    }

    interaction_manager_.mark_status(
      request.request_id,
      (outcome == "SUCCESS")
        ? g1_voice_router::InteractionStatus::REPLY_READY
        : g1_voice_router::InteractionStatus::FAILED,
      steady_now_ms(),
      outcome);

    publish_reply(reply);

    RCLCPP_INFO(
      get_logger(),
      "request_id=%lu event=ACTION_FINISHED outcome=%s action=%s",
      static_cast<unsigned long>(request.request_id),
      outcome.c_str(),
      action.action_name.c_str());
  }

  void worker_loop()
  {
    g1_voice_router::InteractionRequest request;

    while (interaction_manager_.wait_pop(request)) {
      try {
        process_request(request);
      } catch (const std::exception & error) {
        RCLCPP_ERROR(
          get_logger(),
          "request_id=%lu event=EXCEPTION what=%s",
          static_cast<unsigned long>(request.request_id),
          error.what());

        interaction_manager_.mark_status(
          request.request_id,
          g1_voice_router::InteractionStatus::FAILED,
          steady_now_ms(),
          "EXCEPTION");
      }

      // 忙碌提示必须排在所有已接受请求之后：
      // 只有当队列中已无待处理的已接受请求时才发布。
      if (interaction_manager_.pending_count() == 0) {
        drain_notices();
      }
    }

    RCLCPP_INFO(
      get_logger(),
      "Worker loop exited");
  }

  std::string ollama_url_;
  std::string model_name_;
  std::string system_prompt_;
  int timeout_seconds_{60};

  std::deque<Json> history_;

  std::string knowledge_file_;

  g1_voice_router::KnowledgeBase knowledge_base_;

  g1_voice_router::FactRiskGuard fact_risk_guard_;

  g1_voice_router::IntentRecognizer intent_recognizer_;

  // P0-20：动作命令映射（固定白名单）。
  g1_voice_router::ActionCommandMapper action_mapper_;

  // P0-21：ASR 专有名词归一化（有限词典，保留原始文本）。
  g1_voice_router::AsrTextNormalizer asr_normalizer_;

  // 动作结果（由 ROS 回调写入，worker 线程等待）。
  std::mutex action_result_mutex_;
  std::condition_variable action_result_cv_;
  std::uint64_t action_result_seq_{0};
  std::string action_result_text_;
  long action_result_ms_{0};

  // 队列满时的忙碌提示：由 worker 按顺序发布，不直接从回调线程插队。
  std::mutex notice_mutex_;
  std::deque<std::string> notices_;

  // 停机标志（用于中断进行中的 HTTP 请求）。
  std::atomic<bool> shutdown_requested_{false};

  // RobotStateProvider 聚合状态缓存（P0-18）。
  std::mutex robot_state_mutex_;
  std::string robot_state_json_;
  std::chrono::steady_clock::time_point robot_state_recv_time_;
  bool robot_state_received_{false};

  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr
    robot_state_subscription_;

  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr
    subscription_;

  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr
    publisher_;

  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr
    action_publisher_;

  // P0-19：交互任务管理（有界队列 + 去重 + 状态机）。
  g1_voice_router::InteractionManager interaction_manager_{
    g1_voice_router::InteractionManager::kDefaultMaxPending,
    g1_voice_router::InteractionManager::kDefaultDedupeWindowMs};

  std::thread worker_;

  int action_timeout_seconds_{45};

  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr
    action_result_subscription_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);

  const CURLcode curl_status =
    curl_global_init(CURL_GLOBAL_DEFAULT);

  if (curl_status != CURLE_OK) {
    RCLCPP_ERROR(
      rclcpp::get_logger("g1_voice_router"),
      "curl_global_init failed");

    rclcpp::shutdown();
    return 1;
  }

  try {
    auto node = std::make_shared<G1VoiceRouter>();

    rclcpp::spin(node);

    node->stop_worker();
    node.reset();
  } catch (const std::exception & error) {
    RCLCPP_ERROR(
      rclcpp::get_logger("g1_voice_router"),
      "Fatal error: %s",
      error.what());
  }

  if (rclcpp::ok()) {
    rclcpp::shutdown();
  }

  curl_global_cleanup();
  return 0;
}
