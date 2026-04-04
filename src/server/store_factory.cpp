#include "asiochat/server/mysql_user_store.hpp"
#include "asiochat/server/store_factory.hpp"

#include <stdexcept>

namespace asiochat::server
{
    std::unique_ptr<OfflineMessageStore> StoreFactory::create_offline_message_store(
        const std::string &type,
        const std::string &offline_message_file,
        const MySqlConfig &mysql_config)
    {
        if (type == "none")
        {
            return std::make_unique<NullOfflineMessageStore>();
        }

        if (type == "file")
        {
            return std::make_unique<FileOfflineMessageStore>(offline_message_file);
        }

        if (type == "mysql")
        {
            return std::make_unique<MySqlOfflineMessageStore>(mysql_config);
        }
        throw std::runtime_error("Unsupported offline message store type: " + type);
    }

    std::unique_ptr<OnlineStatusStore> StoreFactory::create_online_status_store(const std::string &type)
    {
        if (type == "memory")
        {
            return std::make_unique<MemoryOnlineStatusStore>();
        }
        throw std::runtime_error("Unsupported online status store type: " + type);
    }

    std::unique_ptr<OnlineStatusStore> StoreFactory::create_online_status_store(const std::string &type,
                                                                                const RedisConfig &redis_config)
    {
        if (type == "memory")
        {
            return std::make_unique<MemoryOnlineStatusStore>();
        }

        if (type == "redis")
        {
            return std::make_unique<RedisOnlineStatusStore>(redis_config);
        }

        throw std::runtime_error("Unsupported online status store type: " + type);
    }
    std::unique_ptr<UserStore> StoreFactory::create_user_store(
        const std::string &type,
        const MySqlConfig &mysql_config)
    {
        if (type == "mysql")
        {
            return std::unique_ptr<UserStore>(new MySqlUserStore(mysql_config));
        }

        throw std::runtime_error("Unsupported user store type: " + type);
    }

}