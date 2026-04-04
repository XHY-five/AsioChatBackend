#pragma once

#include <string>

namespace asiochat::server
{

    struct RedisConfig
    {
        std::string host{"127.0.0.1"};
        int port{6379};
        std::string password;
        int db{0};
        int online_ttl_seconds{30};
    };

} // namespace asiochat::server
