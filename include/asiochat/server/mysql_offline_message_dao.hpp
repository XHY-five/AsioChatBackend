#pragma once

#include "asiochat/server/mysql_connection_pool.hpp"
#include "asiochat/server/offline_message_store.hpp"

#include <string>
#include <vector>

namespace asiochat::server {

class MySqlOfflineMessageDao {
public:
    explicit MySqlOfflineMessageDao(const MySqlConfig& config);

    void ensure_schema();
    void save_private_message(const OfflineMessage& message);
    std::vector<OfflineMessage> take_private_messages_for(const std::string& user);

private:
    MySqlConnectionPool pool_;
};

}  // namespace asiochat::server
