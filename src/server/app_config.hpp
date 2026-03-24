#pragma once

#include <filesystem>
#include <string>

#include "server/offline_message_store.hpp"
#include "server/online_status_store.hpp"

namespace asiochat::server {

struct AppConfig {
    unsigned short server_port{5555};
    unsigned int worker_threads{0};
    unsigned int business_threads{2};
    std::string offline_store_backend{"mysql"};
    std::filesystem::path offline_file_path{"data/offline_messages.json"};
    std::string online_store_backend{"redis"};
    MySqlConfig mysql;
    RedisConfig redis;
};

AppConfig load_app_config(const std::filesystem::path& config_path);

}  // namespace asiochat::server
