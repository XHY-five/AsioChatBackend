#include "server/store_factory.hpp"

#include <stdexcept>

namespace asiochat::server {
namespace {

std::string normalize_backend(std::string backend) {
    for (char& ch : backend) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return backend;
}

}  // namespace

std::shared_ptr<OfflineMessageStore> create_offline_message_store(const AppConfig& config) {
    const std::string backend = normalize_backend(config.offline_store_backend);
    if (backend == "mysql") {
        return std::make_shared<MySqlOfflineMessageStore>(config.mysql);
    }
    if (backend == "file") {
        return std::make_shared<FileOfflineMessageStore>(config.offline_file_path);
    }
    if (backend == "none" || backend == "null") {
        return std::make_shared<NullOfflineMessageStore>();
    }

    throw std::runtime_error("Unsupported offline store backend: " + config.offline_store_backend);
}

std::shared_ptr<OnlineStatusStore> create_online_status_store(const AppConfig& config) {
    const std::string backend = normalize_backend(config.online_store_backend);
    if (backend == "redis") {
        return std::make_shared<RedisOnlineStatusStore>(config.redis);
    }
    if (backend == "none" || backend == "null") {
        return std::make_shared<NullOnlineStatusStore>();
    }

    throw std::runtime_error("Unsupported online status store backend: " + config.online_store_backend);
}

}  // namespace asiochat::server
