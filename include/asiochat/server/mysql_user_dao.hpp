#pragma once

#include "asiochat/server/mysql_connection_pool.hpp"

#include <optional>
#include <string>

namespace asiochat::server {

struct UserRecord {
    std::string username;
    std::string password_hash;
};

class MySqlUserDao {
public:
    explicit MySqlUserDao(const MySqlConfig& config);

    void ensure_schema();
    bool insert_user(const std::string& username, const std::string& password_hash);
    std::optional<UserRecord> find_user(const std::string& username);

private:
    MySqlConnectionPool pool_;
};

}  // namespace asiochat::server
