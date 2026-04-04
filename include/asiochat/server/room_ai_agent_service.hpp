#pragma once

#include "asiochat/server/app_config.hpp"

#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace asiochat::server {

class RoomAiAgentService {
public:
    using ReplyHandler = std::function<void(std::string room,
                                            std::string bot_name,
                                            std::string reply_message)>;

    explicit RoomAiAgentService(const AppConfig& config);

    void request_reply(std::string room,
                       std::string from_user,
                       std::string message,
                       ReplyHandler handler);

private:
    struct RoomState {
        AppConfig::RoomAiAgentConfig config;
        std::vector<std::string> recent_messages;
    };

    RoomState* ensure_room_state_(std::string_view room);

    static std::string build_reply_text(const RoomState& room_state,
                                        std::string_view room,
                                        std::string_view from_user,
                                        std::string_view message);

    static std::string build_local_reply_text(const RoomState& room_state,
                                              std::string_view room,
                                              std::string_view from_user,
                                              std::string_view message);

    static std::string request_provider_reply(const RoomState& room_state,
                                              std::string_view room,
                                              std::string_view from_user,
                                              std::string_view message);

    static std::string apply_template(std::string text,
                                      std::string_view room,
                                      std::string_view from_user,
                                      std::string_view message,
                                      std::string_view persona);

    static void replace_all(std::string& text, std::string_view token, std::string_view replacement);

    std::optional<AppConfig::RoomAiAgentConfig> default_room_config_;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, RoomState> rooms_;
};

}  // namespace asiochat::server
