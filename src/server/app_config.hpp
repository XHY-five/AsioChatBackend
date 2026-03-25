#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>

#include "server/offline_message_store.hpp"
#include "server/online_status_store.hpp"

namespace asiochat::server {

struct AppConfig {
    struct RoomAiAgentConfig {
        bool enabled{false};
        std::string bot_name;
        std::string persona;
        std::string welcome_message;
        std::string reply_template;
        std::string provider{"local"};
        std::string model;
        std::string api_key;
        std::string api_key_env;
        std::string base_url;
        std::string endpoint;
        std::string curl_executable{"curl"};
        unsigned int timeout_seconds{30};
        unsigned int max_tokens{300};
        unsigned int history_messages{6};
        double temperature{0.7};
    };

    unsigned short server_port{5555};
    unsigned int worker_threads{0};
    unsigned int business_threads{2};
    std::string offline_store_backend{"mysql"};
    std::filesystem::path offline_file_path{"data/offline_messages.json"};
    std::string online_store_backend{"redis"};
    MySqlConfig mysql;
    RedisConfig redis;
    std::optional<RoomAiAgentConfig> default_room_ai_agent;
    std::unordered_map<std::string, RoomAiAgentConfig> room_ai_agents;
};

AppConfig load_app_config(const std::filesystem::path& config_path);

}  // namespace asiochat::server
