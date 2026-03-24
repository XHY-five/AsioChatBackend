#include "server/online_status_store.hpp"

#include <cstdio>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace asiochat::server {

void NullOnlineStatusStore::mark_online(std::string_view /*user*/) {}

void NullOnlineStatusStore::refresh(std::string_view /*user*/) {}

void NullOnlineStatusStore::mark_offline(std::string_view /*user*/) {}

RedisOnlineStatusStore::RedisOnlineStatusStore(RedisConfig config)
    : config_(std::move(config)) {}

void RedisOnlineStatusStore::mark_online(std::string_view user) {
    run_redis({"SET", key_for(user), "1", "EX", std::to_string(config_.ttl_seconds)});
}

void RedisOnlineStatusStore::refresh(std::string_view user) {
    run_redis({"EXPIRE", key_for(user), std::to_string(config_.ttl_seconds)});
}

void RedisOnlineStatusStore::mark_offline(std::string_view user) {
    run_redis({"DEL", key_for(user)});
}

std::string RedisOnlineStatusStore::key_for(std::string_view user) const {
    return config_.key_prefix + std::string(user);
}

void RedisOnlineStatusStore::run_redis(const std::vector<std::string>& args) const {
    std::ostringstream command;
    command << "powershell -NoProfile -Command \"& '" << config_.redis_cli_executable << "'"
            << " -h " << config_.host
            << " -p " << config_.port;

    if (!config_.password.empty()) {
        command << " -a " << config_.password;
    }

    for (const auto& arg : args) {
        command << ' ' << arg;
    }

    command << "\" 2>&1";

    FILE* pipe = _popen(command.str().c_str(), "r");
    if (pipe == nullptr) {
        return;
    }

    std::string output;
    char buffer[256];
    while (std::fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
    }

    const int exit_code = _pclose(pipe);
    if (exit_code != 0) {
        throw std::runtime_error("redis-cli command failed: " + output);
    }
}

}  // namespace asiochat::server
