#include "server/chat_server.hpp"

#include "server/chat_session.hpp"
#include "server/offline_message_store.hpp"
#include "server/online_status_store.hpp"

namespace asiochat::server {

using boost::asio::ip::tcp;

ChatServer::ChatServer(boost::asio::io_context& io_context,
                       unsigned short port,
                       std::shared_ptr<OfflineMessageStore> offline_message_store,
                       std::shared_ptr<OnlineStatusStore> online_status_store)
    : acceptor_(io_context, tcp::endpoint(tcp::v4(), port)),
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
                *offline_message_store_,
                *online_status_store_)->start();
        }

        accept_next();
    });
}

}  // namespace asiochat::server
