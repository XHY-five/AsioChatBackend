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

    return config;
}

}  // namespace asiochat::server
