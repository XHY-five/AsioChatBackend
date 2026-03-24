#pragma once

#include "common/protocol.hpp"
#include "server/offline_message_store.hpp"
#include "server/online_status_store.hpp"

#include <boost/asio.hpp>

#include <array>
#include <chrono>
#include <deque>
#include <memory>
#include <string>
#include <vector>

namespace asiochat::server {

class ChatRoom;

class ChatSession : public std::enable_shared_from_this<ChatSession> {
public:
    ChatSession(boost::asio::ip::tcp::socket socket,
                ChatRoom& room,
                OfflineMessageStore& offline_message_store,
                OnlineStatusStore& online_status_store);

    void start();
    void deliver(std::string message);
    void stop(bool graceful = false);
    std::string name() const;

private:
    void close_socket();
    void read_header();
    void read_body(std::uint32_t body_length);
    void handle_payload(const std::string& payload);
    void handle_command(const asiochat::protocol::Command& command);
    void write_next();
    void refresh_activity();
    void schedule_idle_check();
    void submit_store_task(std::function<void()> task);
    void mark_online_async();
    void mark_offline_async(std::string user_name);
    void refresh_online_status_async();
    void load_offline_messages_async();
    void store_offline_message_async(OfflineMessage message);

    boost::asio::ip::tcp::socket socket_;
    boost::asio::strand<boost::asio::any_io_executor> strand_;
    boost::asio::steady_timer idle_timer_;
    ChatRoom& room_;
    OfflineMessageStore& offline_message_store_;
    OnlineStatusStore& online_status_store_;
    std::array<unsigned char, asiochat::protocol::kHeaderLength> read_header_buffer_{};
    std::vector<char> read_body_buffer_;
    std::deque<std::string> write_queue_;
    std::string user_name_;
    std::string current_room_;
    std::chrono::steady_clock::time_point last_activity_;
    std::chrono::steady_clock::time_point last_status_refresh_{};
    bool joined_{false};
    bool stopped_{false};
    bool close_after_write_{false};
};

}  // namespace asiochat::server
