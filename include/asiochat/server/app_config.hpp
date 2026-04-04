#pragma once

#include "asiochat/server/singleton.hpp"
#include "asiochat/server/offline_message_store.hpp"
#include "asiochat/server/redis_config.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <optional>

using namespace std;

namespace asiochat::server
{

    class AppConfig : public Singleton<AppConfig>
    {

    public:
        struct RoomAiAgentConfig
        {
            /* data */
            bool enabled{false};
            std::string bot_name{"room-bot"};
            std::string welcome_message;
            std::string provider{"local"};
            std::string model;
            std::string base_url;
            std::string endpoint;
            string api_key;
            string api_key_env;
            string persona{"You are a helpful room assistant."};
            string reply_template{"[{room}] {persona} 鐢ㄦ埛 {user} 鍒氭墠璇达細{message}"};
            size_t history_messages{8};
            double temperature{0.7};
            int max_tokens{256};
            int timeout_seconds{20};
            std::string curl_executable{"curl"};
        };

        bool load(const std::string &file_path);

        std::uint16_t server_port() const;
        unsigned int business_threads() const;
        int idle_timeout_seconds() const;
        int idle_check_interval_seconds() const;
        const std::string &offline_message_file() const;
        const std::string &offline_message_store_type() const;
        const std::string &online_status_store_type() const;
        const MySqlConfig &mysql_config() const;
        const std::string &user_store_type() const;
        const RedisConfig& redis_config() const;

        optional<RoomAiAgentConfig> default_room_ai_agent;
        unordered_map<std::string, RoomAiAgentConfig> room_ai_agents;
        
    private:
        friend class Singleton<AppConfig>;

        AppConfig() = default;

        std::unordered_map<std::string, std::string> values_;

        MySqlConfig mysql_config_;

        RedisConfig redis_config_;

       
    };

} // namespace asiochat::server
