#include "server/room_ai_agent_service.hpp"

#include "server/business_executor.hpp"

#include <boost/json.hpp>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <process.h>
#define asiochat_popen _popen
#define asiochat_pclose _pclose
#else
#include <unistd.h>
#define asiochat_popen popen
#define asiochat_pclose pclose
#endif

namespace asiochat::server {
namespace {

using boost::json::array;
using boost::json::object;
using boost::json::parse;
using boost::json::serialize;
using boost::json::value;

std::string to_lower(std::string_view input) {
    std::string lowered(input);
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return lowered;
}

bool contains_any_keyword(std::string_view text, const std::vector<std::string>& keywords) {
    const std::string lowered = to_lower(text);
    for (const auto& keyword : keywords) {
        if (lowered.find(keyword) != std::string::npos) {
            return true;
        }
    }
    return false;
}

std::string normalize_provider(std::string provider) {
    if (provider.empty()) {
        return "local";
    }
    return to_lower(provider);
}

std::string safe_fallback_reply(std::string_view room, std::string_view from_user, std::string_view message) {
    std::ostringstream stream;
    stream << "[" << room << "] AI reply to " << from_user << ": " << message;
    return stream.str();
}

std::string resolve_api_key(const AppConfig::RoomAiAgentConfig& config) {
    if (!config.api_key.empty()) {
        return config.api_key;
    }

    std::string env_name = config.api_key_env;
    if (env_name.empty()) {
        const std::string provider = normalize_provider(config.provider);
        if (provider == "openai") {
            env_name = "OPENAI_API_KEY";
        } else if (provider == "deepseek") {
            env_name = "DEEPSEEK_API_KEY";
        }
    }

    if (env_name.empty()) {
        return {};
    }

    if (const char* value = std::getenv(env_name.c_str()); value != nullptr) {
        return std::string(value);
    }
    return {};
}

std::string default_base_url(std::string_view provider) {
    const std::string normalized = normalize_provider(std::string(provider));
    if (normalized == "openai") {
        return "https://api.openai.com";
    }
    if (normalized == "deepseek") {
        return "https://api.deepseek.com";
    }
    if (normalized == "ollama") {
        return "http://127.0.0.1:11434";
    }
    return {};
}

std::string default_endpoint(std::string_view provider) {
    const std::string normalized = normalize_provider(std::string(provider));
    if (normalized == "deepseek") {
        return "/chat/completions";
    }
    return "/v1/chat/completions";
}

std::string trim_trailing_slash(std::string input) {
    while (!input.empty() && input.back() == '/') {
        input.pop_back();
    }
    return input;
}

std::string join_url(const AppConfig::RoomAiAgentConfig& config) {
    const std::string base = trim_trailing_slash(config.base_url.empty() ? default_base_url(config.provider) : config.base_url);
    const std::string endpoint = config.endpoint.empty() ? default_endpoint(config.provider) : config.endpoint;
    if (base.empty()) {
        return endpoint;
    }
    if (!endpoint.empty() && endpoint.front() == '/') {
        return base + endpoint;
    }
    return base + "/" + endpoint;
}

std::filesystem::path make_temp_file_path(std::string_view stem, std::string_view extension) {
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto tid = std::hash<std::thread::id>{}(std::this_thread::get_id());
#ifdef _WIN32
    const auto pid = _getpid();
#else
    const auto pid = getpid();
#endif
    std::ostringstream name;
    name << stem << '_' << pid << '_' << tid << '_' << now << extension;
    return std::filesystem::temp_directory_path() / name.str();
}

std::string escape_for_curl_config(std::string text) {
    std::string escaped;
    escaped.reserve(text.size());
    for (char ch : text) {
        if (ch == '\\' || ch == '"') {
            escaped.push_back('\\');
        }
        escaped.push_back(ch);
    }
    return escaped;
}

std::string curl_path_string(const std::filesystem::path& path) {
    return path.generic_string();
}

std::string quote_for_shell(std::string text) {
#ifdef _WIN32
    std::string escaped;
    escaped.reserve(text.size());
    for (char ch : text) {
        if (ch == '"') {
            escaped += "\"\"";
        } else {
            escaped.push_back(ch);
        }
    }
    return "\"" + escaped + "\"";
#else
    std::string escaped;
    escaped.reserve(text.size() + 2);
    escaped.push_back('\'');
    for (char ch : text) {
        if (ch == '\'') {
            escaped += "'\\''";
        } else {
            escaped.push_back(ch);
        }
    }
    escaped.push_back('\'');
    return escaped;
#endif
}

struct TempFile {
    std::filesystem::path path;
    ~TempFile() {
        if (!path.empty()) {
            std::error_code ec;
            std::filesystem::remove(path, ec);
        }
    }
};

std::string run_command_capture(std::string command) {
    FILE* pipe = asiochat_popen(command.c_str(), "r");
    if (pipe == nullptr) {
        throw std::runtime_error("Failed to start external command.");
    }

    std::string output;
    char buffer[512];
    while (std::fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
    }

    const int exit_code = asiochat_pclose(pipe);
    if (exit_code != 0) {
        throw std::runtime_error(output.empty() ? "External command failed." : output);
    }
    return output;
}

std::string extract_text_from_response(const std::string& response_text) {
    boost::system::error_code ec;
    value parsed = parse(response_text, ec);
    if (ec || !parsed.is_object()) {
        throw std::runtime_error("Provider response is not valid JSON.");
    }

    const auto& root = parsed.as_object();
    if (const auto* error = root.if_contains("error"); error != nullptr && error->is_object()) {
        const auto& error_obj = error->as_object();
        std::string error_message;
        if (const auto* message = error_obj.if_contains("message"); message != nullptr && message->is_string()) {
            error_message = std::string(message->as_string().c_str());
        }
        throw std::runtime_error(error_message.empty() ? "Provider returned an error." : error_message);
    }

    const auto* choices_value = root.if_contains("choices");
    if (choices_value == nullptr || !choices_value->is_array() || choices_value->as_array().empty()) {
        throw std::runtime_error("Provider response does not contain choices.");
    }

    const auto& choice = choices_value->as_array().front();
    if (!choice.is_object()) {
        throw std::runtime_error("Provider response choice is invalid.");
    }

    const auto& choice_obj = choice.as_object();
    const auto* message_value = choice_obj.if_contains("message");
    if (message_value == nullptr || !message_value->is_object()) {
        throw std::runtime_error("Provider response does not contain a message.");
    }

    const auto& message_obj = message_value->as_object();
    const auto* content_value = message_obj.if_contains("content");
    if (content_value == nullptr) {
        throw std::runtime_error("Provider response message has no content.");
    }

    if (content_value->is_string()) {
        return std::string(content_value->as_string().c_str());
    }

    if (content_value->is_array()) {
        std::ostringstream stream;
        for (const auto& part : content_value->as_array()) {
            if (!part.is_object()) {
                continue;
            }
            const auto& part_obj = part.as_object();
            if (const auto* text = part_obj.if_contains("text"); text != nullptr && text->is_string()) {
                stream << std::string(text->as_string().c_str());
            }
        }
        const std::string combined = stream.str();
        if (!combined.empty()) {
            return combined;
        }
    }

    throw std::runtime_error("Provider response content format is unsupported.");
}

std::string build_prompt_with_context(const AppConfig::RoomAiAgentConfig& config,
                                      const std::vector<std::string>& recent_messages,
                                      std::string_view room,
                                      std::string_view from_user,
                                      std::string_view message) {
    std::ostringstream prompt;
    prompt << "Room: " << room << "\n";
    prompt << "User: " << from_user << "\n";
    prompt << "Latest message: " << message << "\n";
    if (!recent_messages.empty()) {
        prompt << "Recent context:\n";
        for (const auto& item : recent_messages) {
            prompt << "- " << item << "\n";
        }
    }
    prompt << "Reply as " << (config.bot_name.empty() ? "the room bot" : config.bot_name)
           << " in a concise, helpful way.";
    return prompt.str();
}

std::string call_openai_compatible_api(const AppConfig::RoomAiAgentConfig& config,
                                       const std::vector<std::string>& recent_messages,
                                       std::string_view room,
                                       std::string_view from_user,
                                       std::string_view message) {
    const std::string url = join_url(config);
    if (url.empty()) {
        throw std::runtime_error("Provider URL is not configured.");
    }
    if (config.model.empty()) {
        throw std::runtime_error("Provider model is not configured.");
    }

    object body;
    body["model"] = config.model;
    body["temperature"] = config.temperature;
    body["max_tokens"] = static_cast<std::int64_t>(config.max_tokens);

    array messages;
    object system_message;
    system_message["role"] = "system";
    system_message["content"] = config.persona.empty()
        ? std::string("You are a helpful room assistant.")
        : config.persona;
    messages.emplace_back(system_message);

    object user_message;
    user_message["role"] = "user";
    user_message["content"] = build_prompt_with_context(config, recent_messages, room, from_user, message);
    messages.emplace_back(user_message);
    body["messages"] = messages;

    TempFile request_file{make_temp_file_path("asiochat_ai_request", ".json")};
    TempFile curl_config_file{make_temp_file_path("asiochat_ai_curl", ".cfg")};

    {
        std::ofstream request_out(request_file.path, std::ios::binary);
        if (!request_out) {
            throw std::runtime_error("Failed to create provider request file.");
        }
        request_out << serialize(body);
    }

    {
        std::ofstream cfg_out(curl_config_file.path, std::ios::binary);
        if (!cfg_out) {
            throw std::runtime_error("Failed to create curl config file.");
        }

        cfg_out << "silent\n";
        cfg_out << "show-error\n";
        cfg_out << "fail-with-body\n";
        cfg_out << "request = \"POST\"\n";
        cfg_out << "url = \"" << escape_for_curl_config(url) << "\"\n";
        cfg_out << "header = \"Content-Type: application/json\"\n";
        if (const std::string api_key = resolve_api_key(config); !api_key.empty()) {
            cfg_out << "header = \"Authorization: Bearer " << escape_for_curl_config(api_key) << "\"\n";
        }
        cfg_out << "max-time = " << config.timeout_seconds << "\n";
        cfg_out << "data-binary = \"@" << escape_for_curl_config(curl_path_string(request_file.path)) << "\"\n";
    }

    std::ostringstream command;
#ifdef _WIN32
    command << "cmd /d /s /c \""
            << quote_for_shell(config.curl_executable)
            << " -K "
            << quote_for_shell(curl_path_string(curl_config_file.path))
            << " 2>&1\"";
#else
    command << quote_for_shell(config.curl_executable)
            << " -K "
            << quote_for_shell(curl_path_string(curl_config_file.path))
            << " 2>&1";
#endif

    return extract_text_from_response(run_command_capture(command.str()));
}

}  // namespace

RoomAiAgentService::RoomAiAgentService(const AppConfig& config)
    : default_room_config_(config.default_room_ai_agent) {
    for (const auto& [room_name, room_config] : config.room_ai_agents) {
        if (!room_config.enabled) {
            continue;
        }
        rooms_.emplace(room_name, RoomState{room_config, {}});
    }
}

RoomAiAgentService::RoomState* RoomAiAgentService::ensure_room_state_(std::string_view room) {
    if (auto it = rooms_.find(std::string(room)); it != rooms_.end()) {
        return &it->second;
    }
    if (!default_room_config_.has_value() || !default_room_config_->enabled) {
        return nullptr;
    }

    auto inherited = *default_room_config_;
    if (inherited.bot_name.empty()) {
        inherited.bot_name = "room-bot";
    }
    auto [it, inserted] = rooms_.emplace(std::string(room), RoomState{std::move(inherited), {}});
    (void)inserted;
    return &it->second;
}

void RoomAiAgentService::request_reply(std::string room,
                                       std::string from_user,
                                       std::string message,
                                       ReplyHandler handler) {
    if (!handler) {
        return;
    }

    RoomState room_state_snapshot;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        RoomState* room_state = ensure_room_state_(room);
        if (room_state == nullptr) {
            return;
        }

        room_state->recent_messages.push_back(from_user + ": " + message);
        const std::size_t max_history = std::max<std::size_t>(1, room_state->config.history_messages);
        while (room_state->recent_messages.size() > max_history) {
            room_state->recent_messages.erase(room_state->recent_messages.begin());
        }
        room_state_snapshot = *room_state;
    }

    auto task = [this, room = std::move(room), from_user = std::move(from_user), message = std::move(message),
                 room_state_snapshot = std::move(room_state_snapshot), handler = std::move(handler)]() mutable {
        try {
            std::string reply = build_reply_text(room_state_snapshot, room, from_user, message);
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (auto it = rooms_.find(room); it != rooms_.end()) {
                    it->second.recent_messages.push_back(it->second.config.bot_name + ": " + reply);
                    const std::size_t max_history = std::max<std::size_t>(1, it->second.config.history_messages);
                    while (it->second.recent_messages.size() > max_history) {
                        it->second.recent_messages.erase(it->second.recent_messages.begin());
                    }
                }
            }
            handler(std::move(room), room_state_snapshot.config.bot_name, std::move(reply));
        } catch (const std::exception& ex) {
            std::string fallback = build_local_reply_text(room_state_snapshot, room, from_user, message);
            fallback += "\n[provider-fallback] " + std::string(ex.what());
            handler(std::move(room), room_state_snapshot.config.bot_name, std::move(fallback));
        } catch (...) {
            handler(room, room_state_snapshot.config.bot_name, safe_fallback_reply(room, from_user, message));
        }
    };

    if (BusinessExecutor::instance().running()) {
        BusinessExecutor::instance().submit(std::move(task));
        return;
    }

    task();
}

std::string RoomAiAgentService::build_reply_text(const RoomState& room_state,
                                                 std::string_view room,
                                                 std::string_view from_user,
                                                 std::string_view message) {
    const std::string provider = normalize_provider(room_state.config.provider);
    if (provider == "local") {
        return build_local_reply_text(room_state, room, from_user, message);
    }
    return request_provider_reply(room_state, room, from_user, message);
}

std::string RoomAiAgentService::build_local_reply_text(const RoomState& room_state,
                                                       std::string_view room,
                                                       std::string_view from_user,
                                                       std::string_view message) {
    if (message.empty()) {
        return {};
    }

    std::string guidance;
    if (contains_any_keyword(message, {"hello", "hi", u8"你好", u8"嗨"})) {
        guidance = u8"你好，{user}。很高兴在这个房间见到你。";
    } else if (contains_any_keyword(message, {"help", u8"怎么", u8"如何", "why", "what"})) {
        guidance = u8"我理解你的问题了，我们可以先把目标、现象和限制条件补全。";
    } else if (contains_any_keyword(message, {"bug", "error", u8"失败", u8"异常"})) {
        guidance = u8"这个问题值得先拆一下现象、复现步骤和日志。";
    } else {
        guidance = u8"我看到了你刚刚在房间里的消息，会结合这个房间的设定继续跟进。";
    }

    std::ostringstream context_stream;
    if (!room_state.recent_messages.empty()) {
        context_stream << u8" 最近上下文：";
        for (std::size_t i = 0; i < room_state.recent_messages.size(); ++i) {
            if (i != 0) {
                context_stream << " | ";
            }
            context_stream << room_state.recent_messages[i];
        }
        context_stream << u8"。";
    }

    std::string reply = room_state.config.reply_template.empty()
        ? u8"[{room}] {persona} 用户 {user} 刚才说：{message}"
        : room_state.config.reply_template;
    reply += " " + guidance + context_stream.str();
    return apply_template(reply, room, from_user, message, room_state.config.persona);
}

std::string RoomAiAgentService::request_provider_reply(const RoomState& room_state,
                                                       std::string_view room,
                                                       std::string_view from_user,
                                                       std::string_view message) {
    const std::string provider = normalize_provider(room_state.config.provider);
    if (provider == "openai" || provider == "deepseek" || provider == "ollama") {
        return call_openai_compatible_api(room_state.config, room_state.recent_messages, room, from_user, message);
    }
    throw std::runtime_error("Unsupported AI provider: " + room_state.config.provider);
}

std::string RoomAiAgentService::apply_template(std::string text,
                                               std::string_view room,
                                               std::string_view from_user,
                                               std::string_view message,
                                               std::string_view persona) {
    replace_all(text, "{room}", room);
    replace_all(text, "{user}", from_user);
    replace_all(text, "{message}", message);
    replace_all(text, "{persona}", persona);
    return text;
}

void RoomAiAgentService::replace_all(std::string& text, std::string_view token, std::string_view replacement) {
    std::size_t position = 0;
    while ((position = text.find(token, position)) != std::string::npos) {
        text.replace(position, token.size(), replacement);
        position += replacement.size();
    }
}

}  // namespace asiochat::server


