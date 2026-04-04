#include "asiochat/server/mysql_offline_message_dao.hpp"

#include <jdbc/cppconn/exception.h>
#include <jdbc/cppconn/prepared_statement.h>
#include <jdbc/cppconn/resultset.h>
#include <jdbc/cppconn/statement.h>

#include <utility>

namespace asiochat::server {
namespace {

constexpr std::size_t kDefaultPoolSize = 2;

}  // namespace

MySqlOfflineMessageDao::MySqlOfflineMessageDao(const MySqlConfig& config)
    : pool_(config, kDefaultPoolSize) {
}

void MySqlOfflineMessageDao::ensure_schema() {
    auto connection = pool_.acquire();

    try {
        std::unique_ptr<sql::Statement> statement(connection->createStatement());

        statement->execute(
            "CREATE TABLE IF NOT EXISTS offline_messages ("
            "id BIGINT PRIMARY KEY AUTO_INCREMENT, "
            "sender VARCHAR(64) NOT NULL, "
            "recipient VARCHAR(64) NOT NULL, "
            "content TEXT NOT NULL, "
            "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
            ")");

        pool_.release(std::move(connection));
    } catch (...) {
        pool_.release(std::move(connection));
        throw;
    }
}

void MySqlOfflineMessageDao::save_private_message(const OfflineMessage& message) {
    auto connection = pool_.acquire();

    try {
        std::unique_ptr<sql::PreparedStatement> statement(
            connection->prepareStatement(
                "INSERT INTO offline_messages(sender, recipient, content) VALUES (?, ?, ?)"));

        statement->setString(1, message.from);
        statement->setString(2, message.to);
        statement->setString(3, message.message);
        statement->execute();

        pool_.release(std::move(connection));
    } catch (...) {
        pool_.release(std::move(connection));
        throw;
    }
}

std::vector<OfflineMessage> MySqlOfflineMessageDao::take_private_messages_for(const std::string& user) {
    auto connection = pool_.acquire();

    try {
        std::vector<OfflineMessage> messages;

        {
            std::unique_ptr<sql::PreparedStatement> query(
                connection->prepareStatement(
                    "SELECT sender, recipient, content "
                    "FROM offline_messages "
                    "WHERE recipient = ? "
                    "ORDER BY id ASC"));

            query->setString(1, user);

            std::unique_ptr<sql::ResultSet> result(query->executeQuery());
            while (result->next()) {
                messages.push_back(OfflineMessage{
                    result->getString("sender"),
                    result->getString("recipient"),
                    result->getString("content")});
            }
        }

        {
            std::unique_ptr<sql::PreparedStatement> remove(
                connection->prepareStatement(
                    "DELETE FROM offline_messages WHERE recipient = ?"));

            remove->setString(1, user);
            remove->execute();
        }

        pool_.release(std::move(connection));
        return messages;
    } catch (...) {
        pool_.release(std::move(connection));
        throw;
    }
}

}  // namespace asiochat::server
