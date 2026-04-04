#pragma once

#include <string>

namespace asiochat::server {

struct MySqlConfig {
    std::string host{"127.0.0.1"};
    unsigned int port{3306};
    std::string user{"root"};
    std::string password;
    std::string database{"asiochat"};
};

}  // namespace asiochat::server
