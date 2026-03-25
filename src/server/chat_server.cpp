#include "server/chat_server.hpp"

#include "server/chat_session.hpp"
#include "server/offline_message_store.hpp"
#include "server/online_status_store.hpp"
#include "server/room_ai_agent_service.hpp"

namespace asiochat::server {

using boost::asio::ip::tcp;

ChatServer::ChatServer(boost::asio::io_context& io_context,
                       const AppConfig& config,
                       std::shared_ptr<OfflineMessageStore> offline_message_store,
                       std::shared_ptr<OnlineStatusStore> online_status_store)
    : acceptor_(io_context, tcp::endpoint(tcp::v4(), config.server_port)),
      room_(std::make_shared<ChatRoom>(config)),
      room_ai_agent_service_(std::make_shared<RoomAiAgentService>(config)),
      offline_message_store_(std::move(offline_message_store)),
      online_status_store_(std::move(online_status_store)) {
    accept_next();
}

void ChatServer::accept_next() {
    acceptor_.async_accept([this](const boost::system::error_code& ec, tcp::socket socket) {
        if (!ec) {
            std::make_shared<ChatSession>(
                std::move(socket),
                room_,
                room_ai_agent_service_,
                *offline_message_store_,
                *online_status_store_)->start();
        }

        accept_next();
    });
}

}  // namespace asiochat::server

