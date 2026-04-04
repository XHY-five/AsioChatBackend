#include "asiochat/server/mysql_user_dao.hpp"

#include <mysql/jdbc.h>

#include <memory>
#include <utility>

namespace asiochat::server {

namespace {

constexpr std::size_t kDefaultPoolSize = 2;

}  // namespace

MySqlUserDao::MySqlUserDao(const MySqlConfig& config)
    : pool_(config, kDefaultPoolSize) {
}

void MySqlUserDao::ensure_schema() {
    auto connection = pool_.acquire();

    try {
        std::unique_ptr<sql::Statement> statement(connection->createStatement());

        statement->execute(
            "CREATE TABLE IF NOT EXISTS users ("
            "id BIGINT PRIMARY KEY AUTO_INCREMENT, "
            "username VARCHAR(64) NOT NULL UNIQUE, "
            "password_hash VARCHAR(128) NOT NULL, "
            "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
            ")");

        pool_.release(std::move(connection));
    } catch (...) {
        pool_.release(std::move(connection));
        throw;
    }
}

bool MySqlUserDao::insert_user(const std::string& username, const std::string& password_hash) {
    auto connection = pool_.acquire();

    try {
        std::unique_ptr<sql::PreparedStatement> statement(
            connection->prepareStatement(
                "INSERT INTO users(username, password_hash) VALUES (?, ?)"));

        statement->setString(1, username);
        statement->setString(2, password_hash);
        statement->execute();

        pool_.release(std::move(connection));
        return true;
    } catch (...) {
        pool_.release(std::move(connection));
        return false;
    }
}

std::optional<UserRecord> MySqlUserDao::find_user(const std::string& username) {
    auto connection = pool_.acquire();

    try {
        std::unique_ptr<sql::PreparedStatement> statement(
            connection->prepareStatement(
                "SELECT username, password_hash FROM users WHERE username = ? LIMIT 1"));

        statement->setString(1, username);

        std::unique_ptr<sql::ResultSet> result(statement->executeQuery());
        if (!result->next()) {
            pool_.release(std::move(connection));
            return std::nullopt;
        }

        UserRecord record;
        record.username = result->getString("username");
        record.password_hash = result->getString("password_hash");

        pool_.release(std::move(connection));
        return record;
    } catch (...) {
        pool_.release(std::move(connection));
        throw;
    }
}

}  // namespace asiochat::server
