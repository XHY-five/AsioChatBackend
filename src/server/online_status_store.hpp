#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace asiochat::server {

struct RedisConfig {
    std::string host{"127.0.0.1"};
    int port{6379};
    int ttl_seconds{120};
    std::string key_prefix{"asiochat:online:"};
    std::string password;
    std::string redis_cli_executable{"redis-cli"};
};

class OnlineStatusStore {
public:
    virtual ~OnlineStatusStore() = default;

    virtual void mark_online(std::string_view user) = 0;
    virtual void refresh(std::string_view user) = 0;
    virtual void mark_offline(std::string_view user) = 0;
};

class NullOnlineStatusStore : public OnlineStatusStore {
public:
    void mark_online(std::string_view user) override;
    void refresh(std::string_view user) override;
    void mark_offline(std::string_view user) override;
};

class RedisOnlineStatusStore : public OnlineStatusStore {
public:
    explicit RedisOnlineStatusStore(RedisConfig config);

    void mark_online(std::string_view user) override;
    void refresh(std::string_view user) override;
    void mark_offline(std::string_view user) override;

private:
    std::string key_for(std::string_view user) const;
    void run_redis(const std::vector<std::string>& args) const;

    RedisConfig config_;
};

}  // namespace asiochat::server
