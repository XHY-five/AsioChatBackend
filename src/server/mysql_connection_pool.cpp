#include "asiochat/server/mysql_connection_pool.hpp"

#include <sstream>
#include <stdexcept>
#include <utility>

namespace asiochat::server
{
    MySqlConnectionPool::MySqlConnectionPool(const MySqlConfig &config, std::size_t pool_size)
        : config_(config)
    {
        for (std::size_t i = 0; i < pool_size; ++i)
        {
            pool_.push(create_connection());
        }
    }

    std::unique_ptr<sql::Connection> MySqlConnectionPool::acquire()
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (pool_.empty())
        {
            return create_connection();
        }

        auto con = std::move(pool_.front());
        pool_.pop();
        return con;
    }

    void MySqlConnectionPool::release(std::unique_ptr<sql::Connection> connection)
    {
        if (!connection)
        {
            return;
        }

        std::lock_guard<std::mutex> lock(mutex_);

        pool_.push(std::move(connection));
    }

    std::unique_ptr<sql::Connection> MySqlConnectionPool::create_connection() const
    {
        sql::mysql::MySQL_Driver *drive = sql::mysql::get_mysql_driver_instance();

        std::ostringstream url;
        url << "tcp://" << config_.host << ':' << config_.port;

        std::unique_ptr<sql::Connection> connection(
            drive->connect(url.str(), config_.user, config_.password));

        connection->setSchema(config_.database);
        return connection;
    }
}