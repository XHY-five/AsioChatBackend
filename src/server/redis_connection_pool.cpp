#include "asiochat/server/redis_connection_pool.hpp"

#include <stdexcept>

namespace asiochat::server {

RedisConnectionPool::RedisConnectionPool(const RedisConfig& config, std::size_t pool_size)
    : config_(config) {
    for (std::size_t i = 0; i < pool_size; ++i) {
        if (auto* connection = create_connection(); connection != nullptr) {
            pool_.push(connection);
        }
    }

    if (pool_.empty()) {
        throw std::runtime_error("Failed to create Redis connections.");
    }
}

RedisConnectionPool::~RedisConnectionPool() {
    close();
}

redisContext* RedisConnectionPool::create_connection() const {
    redisContext* context = redisConnect(config_.host.c_str(), config_.port);
    if (context == nullptr || context->err != 0) {
        if (context != nullptr) {
            redisFree(context);
        }
        return nullptr;
    }

    if (!config_.password.empty()) {
        redisReply* reply = static_cast<redisReply*>(
            redisCommand(context, "AUTH %s", config_.password.c_str()));
        if (reply == nullptr || reply->type == REDIS_REPLY_ERROR) {
            if (reply != nullptr) {
                freeReplyObject(reply);
            }
            redisFree(context);
            return nullptr;
        }
        freeReplyObject(reply);
    }

    if (config_.db != 0) {
        redisReply* reply = static_cast<redisReply*>(
            redisCommand(context, "SELECT %d", config_.db));
        if (reply == nullptr || reply->type == REDIS_REPLY_ERROR) {
            if (reply != nullptr) {
                freeReplyObject(reply);
            }
            redisFree(context);
            return nullptr;
        }
        freeReplyObject(reply);
    }

    return context;
}

redisContext* RedisConnectionPool::acquire() {
    std::unique_lock<std::mutex> lock(mutex_);
    condition_.wait(lock, [this] { return stopped_ || !pool_.empty(); });

    if (stopped_) {
        return nullptr;
    }

    redisContext* connection = pool_.front();
    pool_.pop();
    return connection;
}

void RedisConnectionPool::release(redisContext* connection) {
    if (connection == nullptr) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (stopped_) {
        redisFree(connection);
        return;
    }

    pool_.push(connection);
    condition_.notify_one();
}

void RedisConnectionPool::close() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (stopped_) {
        return;
    }

    stopped_ = true;
    while (!pool_.empty()) {
        redisContext* connection = pool_.front();
        pool_.pop();
        if (connection != nullptr) {
            redisFree(connection);
        }
    }
    condition_.notify_all();
}

}  // namespace asiochat::server
