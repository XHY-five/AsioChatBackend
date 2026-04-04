#pragma once

#include "asiochat/server/redis_config.hpp"

#include <string>
#include <string_view>
#include <unordered_map>
#include <mutex>

namespace asiochat::server
{
    class RedisConnectionPool;

    class OnlineStatusStore
    {
    public:
        virtual ~OnlineStatusStore() = default;

        virtual void mark_online(std::string_view user) = 0;
        virtual void refresh(std::string_view user) = 0;
        virtual void mark_offline(std::string_view user) = 0;
        virtual bool is_online(std::string_view user) const = 0;
    };

    class MemoryOnlineStatusStore : public OnlineStatusStore
    {
    public:
        void mark_online(std::string_view user) override;
        void refresh(std::string_view user) override;
        void mark_offline(std::string_view user) override;
        bool is_online(std::string_view user) const override;

    private:
        mutable std::mutex mutex_;
        std::unordered_map<std::string, bool> online_users_;
    };

    class RedisOnlineStatusStore : public OnlineStatusStore
    {
    public:
        explicit RedisOnlineStatusStore(const RedisConfig &config);
        ~RedisOnlineStatusStore() override;

        void mark_online(std::string_view user) override;
        void refresh(std::string_view user) override;
        void mark_offline(std::string_view user) override;
        bool is_online(std::string_view user) const override;

    private:
        static std::string make_online_key(std::string_view user);

        RedisConfig config_;
        std::unique_ptr<RedisConnectionPool> pool_;
    };

}