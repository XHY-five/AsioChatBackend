#include "asiochat/server/mysql_user_store.hpp"

#include <functional>
#include <string>

namespace asiochat::server {

MySqlUserStore::MySqlUserStore(const MySqlConfig& config)
    : dao_(config) {
    dao_.ensure_schema();
}

bool MySqlUserStore::register_user(std::string_view username, std::string_view password) {
    if (username.empty() || password.empty()) {
        return false;
    }

    return dao_.insert_user(std::string(username), hash_password(password));
}

bool MySqlUserStore::validate_user(std::string_view username, std::string_view password) {
    const auto record = dao_.find_user(std::string(username));
    if (!record.has_value()) {
        return false;
    }

    return record->password_hash == hash_password(password);
}

bool MySqlUserStore::user_exists(std::string_view username) {
    return dao_.find_user(std::string(username)).has_value();
}

std::string MySqlUserStore::hash_password(std::string_view password) {
    return std::to_string(std::hash<std::string_view>{}(password));
}

}  // namespace asiochat::server
