#pragma once

#include "asiochat/server/offline_message_store.hpp"
#include "asiochat/server/online_status_store.hpp"
#include "asiochat/server/user_store.hpp"

#include <memory>
#include <string>

namespace asiochat::server
{
    class StoreFactory
    {
    public:
        static std::unique_ptr<OfflineMessageStore> create_offline_message_store(
            const std::string &type,
            const std::string &offline_message_file,
            const MySqlConfig &mysql_config);

        static std::unique_ptr<OnlineStatusStore> create_online_status_store(
            const std::string &type);

        static std::unique_ptr<OnlineStatusStore> create_online_status_store(
            const std::string &type,
            const RedisConfig &redis_config);

        static std::unique_ptr<UserStore> create_user_store(
            const std::string &type,
            const MySqlConfig &mysql_config);
    };
} // namespace asiochat::server
