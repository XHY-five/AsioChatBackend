#pragma once

#include "asiochat/server/redis_config.hpp"

#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <queue>

#include <hiredis.h>

namespace asiochat::server {

class RedisConnectionPool {
public:
    RedisConnectionPool(const RedisConfig& config, std::size_t pool_size);
    ~RedisConnectionPool();

    redisContext* acquire();
    void release(redisContext* connection);
    void close();

private:
    redisContext* create_connection() const;

    bool stopped_{false};
    RedisConfig config_;
    std::mutex mutex_;
    std::condition_variable condition_;
    std::queue<redisContext*> pool_;
};

}  // namespace asiochat::server
