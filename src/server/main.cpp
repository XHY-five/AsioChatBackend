#include "asiochat/server/business_executor.hpp"
#include "asiochat/server/chat_server.hpp"
#include "asiochat/server/app_config.hpp"
#include "asiochat/server/store_factory.hpp"

#include <iostream>

int main()
{
    auto &config = asiochat::server::AppConfig::instance();
    if (!config.load("app.cfg"))
    {
        std::cout << "Failed to load app.cfg, using default configuration.\n";
    }

    asiochat::server::BusinessExecutor::instance().initialize(config.business_threads());

    auto offline_store = asiochat::server::StoreFactory::create_offline_message_store(
        config.offline_message_store_type(),
        config.offline_message_file(),
        config.mysql_config());

    auto online_store = asiochat::server::StoreFactory::create_online_status_store(
        config.online_status_store_type(),
        config.redis_config());

    auto user_store = asiochat::server::StoreFactory::create_user_store(
        config.user_store_type(),
        config.mysql_config());
    asiochat::server::ChatServer server(config.server_port(),
                                        std::move(offline_store),
                                        std::move(online_store),
                                        std::move(user_store),
                                        std::chrono::seconds(config.idle_timeout_seconds()),
                                        std::chrono::seconds(config.idle_check_interval_seconds()));
    server.run();

    asiochat::server::BusinessExecutor::instance().shutdown();
    return 0;
}
