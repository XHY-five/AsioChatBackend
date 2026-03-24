#pragma once

#include "server/chat_room.hpp"

#include <boost/asio.hpp>

#include <memory>

namespace asiochat::server {

class OfflineMessageStore;
class OnlineStatusStore;

class ChatServer {
public:
    ChatServer(boost::asio::io_context& io_context,
               unsigned short port,
               std::shared_ptr<OfflineMessageStore> offline_message_store,
               std::shared_ptr<OnlineStatusStore> online_status_store);

private:
    void accept_next();

    boost::asio::ip::tcp::acceptor acceptor_;
    ChatRoom room_;
    std::shared_ptr<OfflineMessageStore> offline_message_store_;
    std::shared_ptr<OnlineStatusStore> online_status_store_;
};

}  // namespace asiochat::server
