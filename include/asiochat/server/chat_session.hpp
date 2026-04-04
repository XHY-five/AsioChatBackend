#pragma once

#include "asiochat/common/protocol.hpp"
#include "asiochat/server/offline_message_store.hpp"
#include "asiochat/server/online_status_store.hpp"
#include "asiochat/server/room_ai_agent_service.hpp"
#include "asiochat/server/user_store.hpp"

#include <boost/asio.hpp>

#include <array>
#include <chrono>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace asiochat::server
{

    class ChatRoom;

    class ChatSession : public std::enable_shared_from_this<ChatSession>
    {
    public:
        explicit ChatSession(boost::asio::ip::tcp::socket socket,
                             std::shared_ptr<ChatRoom> room,
                             std::shared_ptr<RoomAiAgentService> room_ai_agent_service,
                             OfflineMessageStore &offline_message_store,
                             OnlineStatusStore &online_status_store,
                             UserStore &user_store,
                             std::chrono::seconds idle_timeout,
                             std::chrono::seconds idle_check_interval);

        void start();
        void deliver(const std::string &message);

        const std::string &name() const { return user_name_; }

    private:
        void read_header();
        void read_body(std::uint32_t body_length);
        void handle_payload(const std::string &payload);
        void write_next();
        void refresh_activity();
        void schedule_idle_check();
        void close_session();
        void notify_idle_timeout_and_close();

        void submit_store_task(std::function<void()> task);
        void load_offline_message_async();
        void store_offline_message_async(OfflineMessage message);

        void broadcast_room_system_message(const std::string &room_name, const std::string &text);
        void send_json(const asiochat::protocol::Message &message);
        void send_room_ai_welcome_if_needed(const std::string &room_name);
        void request_room_ai_reply(std::string room, std::string from_user, std::string message);

        boost::asio::ip::tcp::socket socket_;
        boost::asio::strand<boost::asio::any_io_executor> strand_;
        boost::asio::steady_timer idle_timer_;

        std::array<unsigned char, asiochat::protocol::kHeaderLength> read_header_buffer_{};
        std::vector<char> read_body_buffer_;
        std::deque<std::string> write_queue_;

        std::shared_ptr<ChatRoom> room_;
        std::shared_ptr<RoomAiAgentService> room_ai_agent_service_;

        OfflineMessageStore &offline_message_store_;
        OnlineStatusStore &online_status_store_;
        UserStore &user_store_;

        std::string user_name_;
        std::string current_room_{"lobby"};

        std::chrono::steady_clock::time_point last_activity_;
        bool logged_in_{false};
        bool stopped_{false};
        bool closing_after_write_{false};

        std::chrono::seconds idle_timeout_;
        std::chrono::seconds idle_check_interval_;
    };

} // namespace asiochat::server
