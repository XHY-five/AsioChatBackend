#pragma once

#include "server/app_config.hpp"
#include "server/chat_room.hpp"

#include <boost/asio.hpp>

#include <memory>

namespace asiochat::server {

class OfflineMessageStore;
class OnlineStatusStore;
class RoomAiAgentService;

class ChatServer {
public:
    ChatServer(boost::asio::io_context& io_context,
               const AppConfig& config,
               std::shared_ptr<OfflineMessageStore> offline_message_store,
               std::shared_ptr<OnlineStatusStore> online_status_store);

private:
    void accept_next();

    boost::asio::ip::tcp::acceptor acceptor_;
    std::shared_ptr<ChatRoom> room_;
    std::shared_ptr<RoomAiAgentService> room_ai_agent_service_;
    std::shared_ptr<OfflineMessageStore> offline_message_store_;
    std::shared_ptr<OnlineStatusStore> online_status_store_;
};

}  // namespace asiochat::server
