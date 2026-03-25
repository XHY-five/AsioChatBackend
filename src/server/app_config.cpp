#include "server/app_config.hpp"

#include <boost/json.hpp>

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace asiochat::server {
namespace {

std::string read_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Failed to open config file: " + path.string());
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    std::string content = buffer.str();
    if (content.size() >= 3 &&
        static_cast<unsigned char>(content[0]) == 0xEF &&
        static_cast<unsigned char>(content[1]) == 0xBB &&
        static_cast<unsigned char>(content[2]) == 0xBF) {
        content.erase(0, 3);
    }
    return content;
}

std::string get_string(const boost::json::object& obj, std::string_view key, std::string default_value = {}) {
    if (const auto* value = obj.if_contains(key); value != nullptr && value->is_string()) {
        return std::string(value->as_string().c_str());
    }
    return default_value;
}

int get_int(const boost::json::object& obj, std::string_view key, int default_value) {
    if (const auto* value = obj.if_contains(key); value != nullptr && value->is_int64()) {
        return static_cast<int>(value->as_int64());
    }
    return default_value;
}

unsigned int get_uint(const boost::json::object& obj, std::string_view key, unsigned int default_value) {
    if (const auto* value = obj.if_contains(key); value != nullptr && value->is_int64()) {
        return static_cast<unsigned int>(value->as_int64());
    }
    return default_value;
}

bool get_bool(const boost::json::object& obj, std::string_view key, bool default_value) {
    if (const auto* value = obj.if_contains(key); value != nullptr && value->is_bool()) {
        return value->as_bool();
    }
    return default_value;
}

double get_double(const boost::json::object& obj, std::string_view key, double default_value) {
    if (const auto* value = obj.if_contains(key); value != nullptr) {
        if (value->is_double()) {
            return value->as_double();
        }
        if (value->is_int64()) {
            return static_cast<double>(value->as_int64());
        }
    }
    return default_value;
}

AppConfig::RoomAiAgentConfig parse_room_ai_agent_config(const boost::json::object& room_obj,
                                                        const std::string& default_bot_name) {
    AppConfig::RoomAiAgentConfig room_config;
    room_config.enabled = get_bool(room_obj, "enabled", room_config.enabled);
    room_config.bot_name = get_string(room_obj, "bot_name", default_bot_name);
    room_config.persona = get_string(room_obj, "persona", "You are a helpful room assistant.");
    room_config.welcome_message = get_string(room_obj, "welcome_message");
    room_config.reply_template = get_string(room_obj, "reply_template", "[{room}] {persona} User {user} said: {message}");
    room_config.provider = get_string(room_obj, "provider", room_config.provider);
    room_config.model = get_string(room_obj, "model", room_config.model);
    room_config.api_key = get_string(room_obj, "api_key", room_config.api_key);
    room_config.api_key_env = get_string(room_obj, "api_key_env", room_config.api_key_env);
    room_config.base_url = get_string(room_obj, "base_url", room_config.base_url);
    room_config.endpoint = get_string(room_obj, "endpoint", room_config.endpoint);
    room_config.curl_executable = get_string(room_obj, "curl_executable", room_config.curl_executable);
    room_config.timeout_seconds = get_uint(room_obj, "timeout_seconds", room_config.timeout_seconds);
    room_config.max_tokens = get_uint(room_obj, "max_tokens", room_config.max_tokens);
    room_config.history_messages = get_uint(room_obj, "history_messages", room_config.history_messages);
    room_config.temperature = get_double(room_obj, "temperature", room_config.temperature);
    return room_config;
}

}  // namespace

AppConfig load_app_config(const std::filesystem::path& config_path) {
    const std::string content = read_file(config_path);

    boost::system::error_code ec;
    boost::json::value parsed = boost::json::parse(content, ec);
    if (ec || !parsed.is_object()) {
        throw std::runtime_error("Invalid config file format: " + config_path.string());
    }

    const auto& root = parsed.as_object();
    AppConfig config;

    if (const auto* server = root.if_contains("server"); server != nullptr && server->is_object()) {
        const auto& server_obj = server->as_object();
        config.server_port = static_cast<unsigned short>(get_int(server_obj, "port", config.server_port));
        config.worker_threads = get_uint(server_obj, "worker_threads", config.worker_threads);
        config.business_threads = get_uint(server_obj, "business_threads", config.business_threads);
    }

    if (const auto* offline_store = root.if_contains("offline_store"); offline_store != nullptr && offline_store->is_object()) {
        const auto& offline_store_obj = offline_store->as_object();
        config.offline_store_backend = get_string(offline_store_obj, "backend", config.offline_store_backend);
        config.offline_file_path = get_string(offline_store_obj, "file_path", config.offline_file_path.string());
    }

    if (const auto* mysql = root.if_contains("mysql"); mysql != nullptr && mysql->is_object()) {
        const auto& mysql_obj = mysql->as_object();
        config.mysql.host = get_string(mysql_obj, "host", config.mysql.host);
        config.mysql.port = static_cast<unsigned int>(get_int(mysql_obj, "port", static_cast<int>(config.mysql.port)));
        config.mysql.user = get_string(mysql_obj, "user", config.mysql.user);
        config.mysql.password = get_string(mysql_obj, "password", config.mysql.password);
        config.mysql.database = get_string(mysql_obj, "database", config.mysql.database);
        config.mysql.mysql_executable = get_string(mysql_obj, "mysql_executable", config.mysql.mysql_executable);
    }

    if (const auto* online_store = root.if_contains("online_store"); online_store != nullptr && online_store->is_object()) {
        const auto& online_store_obj = online_store->as_object();
        config.online_store_backend = get_string(online_store_obj, "backend", config.online_store_backend);
    }

    if (const auto* redis = root.if_contains("redis"); redis != nullptr && redis->is_object()) {
        const auto& redis_obj = redis->as_object();
        config.redis.host = get_string(redis_obj, "host", config.redis.host);
        config.redis.port = get_int(redis_obj, "port", config.redis.port);
        config.redis.password = get_string(redis_obj, "password", config.redis.password);
        config.redis.ttl_seconds = get_int(redis_obj, "ttl_seconds", config.redis.ttl_seconds);
        config.redis.key_prefix = get_string(redis_obj, "key_prefix", config.redis.key_prefix);
        config.redis.redis_cli_executable = get_string(redis_obj, "redis_cli_executable", config.redis.redis_cli_executable);
    }

    if (const auto* ai_agents = root.if_contains("ai_agents"); ai_agents != nullptr && ai_agents->is_object()) {
        const auto& ai_agents_obj = ai_agents->as_object();

        if (const auto* default_agent = ai_agents_obj.if_contains("default"); default_agent != nullptr && default_agent->is_object()) {
            config.default_room_ai_agent = parse_room_ai_agent_config(default_agent->as_object(), "room-bot");
        }

        if (const auto* rooms = ai_agents_obj.if_contains("rooms"); rooms != nullptr && rooms->is_object()) {
            for (const auto& [room_name, room_value] : rooms->as_object()) {
                if (!room_value.is_object()) {
                    continue;
                }

                const std::string room_name_string(room_name.data(), room_name.size());
                config.room_ai_agents.emplace(
                    room_name_string,
                    parse_room_ai_agent_config(room_value.as_object(), "ai-" + room_name_string));
            }
        }
    }

    return config;
}

}  // namespace asiochat::server
