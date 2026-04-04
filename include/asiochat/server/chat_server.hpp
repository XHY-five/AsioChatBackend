#pragma once

#include "asiochat/server/chat_room.hpp"
#include "asiochat/server/offline_message_store.hpp"
#include "asiochat/server/online_status_store.hpp"
#include "asiochat/server/room_ai_agent_service.hpp"
#include "asiochat/server/user_store.hpp"

#include <boost/asio.hpp>

#include <chrono>
#include <cstdint>
#include <memory>

namespace asiochat::server {

class ChatServer {
public:
    explicit ChatServer(std::uint16_t port,
                        std::unique_ptr<OfflineMessageStore> offline_message_store,
                        std::unique_ptr<OnlineStatusStore> online_status_store,
                        std::unique_ptr<UserStore> user_store,
                        std::chrono::seconds idle_timeout,
                        std::chrono::seconds idle_check_interval);

    void run();

private:
    void accept_next();

    boost::asio::io_context io_context_;
    boost::asio::ip::tcp::acceptor acceptor_;

    std::shared_ptr<ChatRoom> room_;
    std::shared_ptr<RoomAiAgentService> room_ai_agent_service_;

    std::chrono::seconds idle_timeout_;
    std::chrono::seconds idle_check_interval_;

    std::unique_ptr<OfflineMessageStore> offline_message_store_;
    std::unique_ptr<OnlineStatusStore> online_status_store_;
    std::unique_ptr<UserStore> user_store_;
};

}  // namespace asiochat::server
