#include "asiochat/server/app_config.hpp"
#include <fstream>
#include <sstream>

namespace asiochat::server
{
    namespace
    {
        std::string trim(const std::string &text)
        {
            const auto first = text.find_first_not_of(" \t\r\n");
            if (first == std::string::npos)
            {
                return {};
            }

            const auto last = text.find_last_not_of(" \t\r\n");
            return text.substr(first, last - first + 1);
        }
    }

    bool AppConfig::load(const std::string &file_path)
    {
        values_.clear();

        std::ifstream input(file_path);

        if (!input.is_open())
        {
            return false;
        }

        std::string line;
        while (std::getline(input, line))
        {
            line = trim(line);

            if (line.empty() || line[0] == '#')
            {
                continue;
            }

            const auto pos = line.find('=');
            if (pos == std::string::npos)
            {
                continue;
            }

            const std::string key = trim(line.substr(0, pos));
            const std::string value = trim(line.substr(pos + 1));

            if (!key.empty())
            {
                values_[key] = value;
            }
        }

        const auto host_it = values_.find("mysql.host");
        if (host_it != values_.end())
        {
            mysql_config_.host = host_it->second;
        }

        const auto port_it = values_.find("mysql.port");
        if (port_it != values_.end())
        {
            mysql_config_.port = static_cast<unsigned int>(std::stoul(port_it->second));
        }

        const auto user_it = values_.find("mysql.user");
        if (user_it != values_.end())
        {
            mysql_config_.user = user_it->second;
        }

        const auto password_it = values_.find("mysql.password");
        if (password_it != values_.end())
        {
            mysql_config_.password = password_it->second;
        }

        const auto database_it = values_.find("mysql.database");
        if (database_it != values_.end())
        {
            mysql_config_.database = database_it->second;
        }

        auto load_room_ai_config = [this](const string &prefix, RoomAiAgentConfig &cfg)
        {
            auto read_string = [this, &prefix](const string &key, string &out)
            {
                const auto it = values_.find(prefix + key);
                if (it != values_.end())
                    out = it->second;
            };
            auto read_bool = [this, &prefix](const std::string &key, bool &out)
            {
                const auto it = values_.find(prefix + key);
                if (it != values_.end())
                    out = (it->second == "true" || it->second == "1");
            };
            auto read_size = [this, &prefix](const std::string &key, std::size_t &out)
            {
                const auto it = values_.find(prefix + key);
                if (it != values_.end())
                    out = static_cast<std::size_t>(std::stoul(it->second));
            };
            auto read_double = [this, &prefix](const std::string &key, double &out)
            {
                const auto it = values_.find(prefix + key);
                if (it != values_.end())
                    out = std::stod(it->second);
            };
            auto read_int = [this, &prefix](const std::string &key, int &out)
            {
                const auto it = values_.find(prefix + key);
                if (it != values_.end())
                    out = std::stoi(it->second);
            };

            read_bool("enabled", cfg.enabled);
            read_string("bot_name", cfg.bot_name);
            read_string("welcome_message", cfg.welcome_message);
            read_string("provider", cfg.provider);
            read_string("model", cfg.model);
            read_string("base_url", cfg.base_url);
            read_string("endpoint", cfg.endpoint);
            read_string("api_key", cfg.api_key);
            read_string("api_key_env", cfg.api_key_env);
            read_string("persona", cfg.persona);
            read_string("reply_template", cfg.reply_template);
            read_size("history_messages", cfg.history_messages);
            read_double("temperature", cfg.temperature);
            read_int("max_tokens", cfg.max_tokens);
            read_int("timeout_seconds", cfg.timeout_seconds);
            read_string("curl_executable", cfg.curl_executable);
        };

        RoomAiAgentConfig default_cfg;
        load_room_ai_config("ai_agents.default.", default_cfg);
        if (default_cfg.enabled)
        {
            default_room_ai_agent = default_cfg;
        }

        room_ai_agents.clear();
        for (const auto &[key, value] : values_)
        {
            const std::string prefix = "ai_agents.rooms.";
            if (key.rfind(prefix, 0) != 0)
                continue;

            const auto rest = key.substr(prefix.size());
            const auto dot = rest.find('.');
            if (dot == std::string::npos)
                continue;

            const std::string room_name = rest.substr(0, dot);
            RoomAiAgentConfig cfg;
            if (const auto inherited = default_room_ai_agent; inherited.has_value())
            {
                cfg = *inherited;
            }
            load_room_ai_config(prefix + room_name + ".", cfg);
            if (cfg.enabled)
            {
                room_ai_agents[room_name] = cfg;
            }
        }

        if (const auto it = values_.find("redis.host"); it != values_.end())
        {
            redis_config_.host = it->second;
        }
        if (const auto it = values_.find("redis.port"); it != values_.end())
        {
            redis_config_.port = std::stoi(it->second);
        }
        if (const auto it = values_.find("redis.password"); it != values_.end())
        {
            redis_config_.password = it->second;
        }
        if (const auto it = values_.find("redis.db"); it != values_.end())
        {
            redis_config_.db = std::stoi(it->second);
        }
        if (const auto it = values_.find("redis.online_ttl_seconds"); it != values_.end())
        {
            redis_config_.online_ttl_seconds = std::stoi(it->second);
        }
        return true;
    }

    std::uint16_t AppConfig::server_port() const
    {
        const auto it = values_.find("server.port");
        if (it == values_.end())
        {
            return 5555;
        }

        return static_cast<std::uint16_t>(std::stoi(it->second));
    }

    unsigned int AppConfig::business_threads() const
    {
        const auto it = values_.find("executor.business_threads");
        if (it == values_.end())
        {
            return 2;
        }

        return static_cast<unsigned int>(std::stoul(it->second));
    }

    int AppConfig::idle_timeout_seconds() const
    {
        const auto it = values_.find("server.idle_timeout_seconds");
        if (it == values_.end())
        {
            return 120;
        }

        return std::stoi(it->second);
    }

    int AppConfig::idle_check_interval_seconds() const
    {
        const auto it = values_.find("server.idle_check_interval_seconds");
        if (it == values_.end())
        {
            return 10;
        }

        return std::stoi(it->second);
    }

    const std::string &AppConfig::offline_message_file() const
    {
        static const std::string kDefaultFile = "offline_messages.txt";

        const auto it = values_.find("storage.offline_message_file");
        if (it == values_.end())
        {
            return kDefaultFile;
        }

        return it->second;
    }

    const std::string &AppConfig::offline_message_store_type() const
    {
        static const std::string kDefaultType = "file";

        const auto it = values_.find("storage.offline_message_store");

        if (it == values_.end())
        {
            return kDefaultType;
        }

        return it->second;
    }

    const std::string &AppConfig::online_status_store_type() const
    {
        static const std::string kDefaultType = "memory";

        const auto it = values_.find("storage.online_status_store");
        if (it == values_.end())
        {
            return kDefaultType;
        }

        return it->second;
    }

    const MySqlConfig &AppConfig::mysql_config() const
    {
        return mysql_config_;
    }

    const std::string &AppConfig::user_store_type() const
    {
        static const std::string kDefaultType = "mysql";

        const auto it = values_.find("storage.user_store");
        if (it == values_.end())
        {
            return kDefaultType;
        }

        return it->second;
    }
    
    const RedisConfig& AppConfig::redis_config() const
    {
        return redis_config_;
    }
}