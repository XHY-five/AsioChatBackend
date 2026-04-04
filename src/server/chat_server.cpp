#include "asiochat/server/chat_server.hpp"

#include "asiochat/server/app_config.hpp"
#include "asiochat/server/chat_session.hpp"

#include <iostream>

namespace asiochat::server {

using boost::asio::ip::tcp;

ChatServer::ChatServer(std::uint16_t port,
                       std::unique_ptr<OfflineMessageStore> offline_message_store,
                       std::unique_ptr<OnlineStatusStore> online_status_store,
                       std::unique_ptr<UserStore> user_store,
                       std::chrono::seconds idle_timeout,
                       std::chrono::seconds idle_check_interval)
    : io_context_(),
      acceptor_(io_context_, tcp::endpoint(tcp::v4(), port)),
      room_(std::make_shared<ChatRoom>(AppConfig::instance())),
      room_ai_agent_service_(std::make_shared<RoomAiAgentService>(AppConfig::instance())),
      idle_timeout_(idle_timeout),
      idle_check_interval_(idle_check_interval),
      offline_message_store_(std::move(offline_message_store)),
      online_status_store_(std::move(online_status_store)),
      user_store_(std::move(user_store)) {
}

void ChatServer::run() {
    std::cout << "ChatServer is running on port "
              << acceptor_.local_endpoint().port()
              << "\n";

    accept_next();
    io_context_.run();
}

void ChatServer::accept_next() {
    acceptor_.async_accept(
        [this](const boost::system::error_code& ec, tcp::socket socket) {
            if (!ec) {
                std::cout << "A new client connected from "
                          << socket.remote_endpoint().address().to_string()
                          << ':'
                          << socket.remote_endpoint().port()
                          << "\n";

                auto session = std::make_shared<ChatSession>(
                    std::move(socket),
                    room_,
                    room_ai_agent_service_,
                    *offline_message_store_,
                    *online_status_store_,
                    *user_store_,
                    idle_timeout_,
                    idle_check_interval_);
                session->start();
            }

            accept_next();
        });
}

}  // namespace asiochat::server
