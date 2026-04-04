#pragma once

#include "asiochat/server/mysql_user_dao.hpp"
#include "asiochat/server/user_store.hpp"

namespace asiochat::server {

class MySqlUserStore : public UserStore {
public:
    explicit MySqlUserStore(const MySqlConfig& config);

    bool register_user(std::string_view username, std::string_view password) override;
    bool validate_user(std::string_view username, std::string_view password) override;
    bool user_exists(std::string_view username) override;

private:
    static std::string hash_password(std::string_view password);

    MySqlUserDao dao_;
};

}  // namespace asiochat::server
