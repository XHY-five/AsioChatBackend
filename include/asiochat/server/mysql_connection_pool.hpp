#pragma once

#include "asiochat/server/mysql_config.hpp"

#include <memory>
#include <mutex>
#include <queue>
#include <string>

#include <mysql/jdbc.h>

namespace asiochat::server {

class MySqlConnectionPool {
public:
    MySqlConnectionPool(const MySqlConfig& config, std::size_t pool_size);

    std::unique_ptr<sql::Connection> acquire();
    void release(std::unique_ptr<sql::Connection> connection);

private:
    std::unique_ptr<sql::Connection> create_connection() const;

    MySqlConfig config_;
    std::mutex mutex_;
    std::queue<std::unique_ptr<sql::Connection>> pool_;
};

}  // namespace asiochat::server
