#pragma once

#include <string>
#include <string_view>

namespace asiochat::server
{

    class UserStore
    {
    public:
        virtual ~UserStore() = default;

        virtual bool register_user(std::string_view username, std::string_view password) = 0;
        virtual bool validate_user(std::string_view username, std::string_view password) = 0;
        virtual bool user_exists(std::string_view username) = 0;
    };

} // namespace asiochat::server
