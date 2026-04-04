#include "asiochat/server/online_status_store.hpp"
#include "asiochat/server/redis_connection_pool.hpp"

#include <hiredis.h>
#include <stdexcept>

namespace asiochat::server
{
    void MemoryOnlineStatusStore::mark_online(std::string_view user)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        online_users_[std::string(user)] = true;
    }

    void MemoryOnlineStatusStore::refresh(std::string_view user)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        online_users_[std::string(user)] = true;
    }

    void MemoryOnlineStatusStore::mark_offline(std::string_view user)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        online_users_.erase(std::string(user));
    }

    bool MemoryOnlineStatusStore::is_online(std::string_view user) const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto it = online_users_.find(std::string(user));
        return it != online_users_.end() && it->second;
    }
    //redis
    RedisOnlineStatusStore::RedisOnlineStatusStore(const RedisConfig &config)
        : config_(config),
          pool_(std::make_unique<RedisConnectionPool>(config, 4))
    {
    }

    RedisOnlineStatusStore::~RedisOnlineStatusStore() = default;

    std::string RedisOnlineStatusStore::make_online_key(std::string_view user)
    {
        return "online:user:" + std::string(user);
    }

    void RedisOnlineStatusStore::mark_online(std::string_view user)
    {
        redisContext *connection = pool_->acquire();
        if (connection == nullptr)
        {
            throw std::runtime_error("Failed to acquire Redis connection.");
        }

        const std::string key = make_online_key(user);
        redisReply *reply = static_cast<redisReply *>(
            redisCommand(connection, "SET %s 1 EX %d", key.c_str(), config_.online_ttl_seconds));

        if (reply == nullptr || reply->type == REDIS_REPLY_ERROR)
        {
            if (reply != nullptr)
            {
                freeReplyObject(reply);
            }
            pool_->release(connection);
            throw std::runtime_error("Redis SET EX failed.");
        }

        freeReplyObject(reply);
        pool_->release(connection);
    }

    void RedisOnlineStatusStore::refresh(std::string_view user)
    {
        mark_online(user);
    }

    void RedisOnlineStatusStore::mark_offline(std::string_view user)
    {
        redisContext *connection = pool_->acquire();
        if (connection == nullptr)
        {
            throw std::runtime_error("Failed to acquire Redis connection.");
        }

        const std::string key = make_online_key(user);
        redisReply *reply = static_cast<redisReply *>(
            redisCommand(connection, "DEL %s", key.c_str()));

        if (reply != nullptr)
        {
            freeReplyObject(reply);
        }

        pool_->release(connection);
    }

    bool RedisOnlineStatusStore::is_online(std::string_view user) const
    {
        redisContext *connection = pool_->acquire();
        if (connection == nullptr)
        {
            return false;
        }

        const std::string key = make_online_key(user);
        redisReply *reply = static_cast<redisReply *>(
            redisCommand(connection, "EXISTS %s", key.c_str()));

        const bool online =
            reply != nullptr &&
            reply->type == REDIS_REPLY_INTEGER &&
            reply->integer > 0;

        if (reply != nullptr)
        {
            freeReplyObject(reply);
        }

        pool_->release(connection);
        return online;
    }
}