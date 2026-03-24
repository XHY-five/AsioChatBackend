#pragma once

#include "server/app_config.hpp"
#include "server/offline_message_store.hpp"
#include "server/online_status_store.hpp"

#include <memory>

namespace asiochat::server {

std::shared_ptr<OfflineMessageStore> create_offline_message_store(const AppConfig& config);
std::shared_ptr<OnlineStatusStore> create_online_status_store(const AppConfig& config);

}  // namespace asiochat::server
