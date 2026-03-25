#include "server/chat_session.hpp"

#include "server/business_executor.hpp"
#include "server/chat_room.hpp"
#include "server/room_ai_agent_service.hpp"

#include <boost/asio/dispatch.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>

namespace asiochat::server {
namespace {

using namespace std::chrono_literals;
constexpr auto kIdleTimeout = 60s;
constexpr auto kIdleCheckInterval = 15s;
constexpr auto kStatusRefreshInterval = 5s;

std::string ascii_lower(std::string_view input) {
    std::string lowered;
    lowered.reserve(input.size());
    for (unsigned char ch : input) {
        lowered.push_back(static_cast<char>(std::tolower(ch)));
    }
    return lowered;
}

bool is_room_ai_mention_target(std::string_view target) {
    const std::string lowered = ascii_lower(target);
    return lowered == "ai" || lowered == u8"小d";
}

std::string format_room_ai_mention_message(std::string_view target, std::string_view message) {
    std::string formatted = "@";
    formatted += std::string(target);
    if (!message.empty()) {
        formatted.push_back(' ');
        formatted += std::string(message);
    }
    return formatted;
}

void deliver_room_ai_welcome(const std::shared_ptr<ChatRoom>& room, std::string_view target_room) {
    if (!room) {
        return;
    }

    const auto welcome = room->ai_agent_welcome_for_room(target_room);
    if (!welcome.has_value()) {
        return;
    }

    room->broadcast_to_room(
        target_room,
        asiochat::protocol::make_chat_event(
            room->ai_agent_name_for_room(target_room).value_or("room-bot"),
            target_room,
            *welcome));
}

}  // namespace

using boost::asio::ip::tcp;

ChatSession::ChatSession(tcp::socket socket,
                         std::shared_ptr<ChatRoom> room,
                         std::shared_ptr<RoomAiAgentService> room_ai_agent_service,
                         OfflineMessageStore& offline_message_store,
                         OnlineStatusStore& online_status_store)
    : socket_(std::move(socket)),
      strand_(boost::asio::make_strand(socket_.get_executor())),
      idle_timer_(socket_.get_executor()),
      room_(std::move(room)),
      room_ai_agent_service_(std::move(room_ai_agent_service)),
      offline_message_store_(offline_message_store),
      online_status_store_(online_status_store),
      last_activity_(std::chrono::steady_clock::now()),
      last_status_refresh_(std::chrono::steady_clock::time_point::min()) {}

void ChatSession::start() {
    deliver(asiochat::protocol::make_system_event(
        "Welcome. Commands: /login <name>, /join <room>, /msg <text>, /pm <user> <text>, /users, /rooms, /ping, /quit."));
    schedule_idle_check();
    read_header();
}

void ChatSession::deliver(std::string message) {
    auto self = shared_from_this();
    boost::asio::dispatch(strand_, [this, self, message = std::move(message)]() mutable {
        const bool writing = !write_queue_.empty();
        write_queue_.push_back(asiochat::protocol::make_frame(message));
        if (!writing) {
            write_next();
        }
    });
}

void ChatSession::stop(bool graceful) {
    auto self = shared_from_this();
    boost::asio::dispatch(strand_, [this, self, graceful]() {
        if (stopped_) {
            return;
        }

        stopped_ = true;
        idle_timer_.cancel();

        if (joined_ && room_) {
            mark_offline_async(user_name_);

            std::string departed_name;
            std::string departed_room;
            room_->leave(this, departed_name, departed_room);
            if (!departed_name.empty() && !departed_room.empty()) {
                room_->broadcast_to_room(
                    departed_room,
                    asiochat::protocol::make_presence_event(
                        departed_name,
                        departed_room,
                        "left",
                        room_->online_count(departed_room)));
            }
        }

        if (graceful && !write_queue_.empty()) {
            close_after_write_ = true;
            return;
        }

        close_socket();
    });
}

std::string ChatSession::name() const {
    return user_name_;
}

void ChatSession::close_socket() {
    boost::system::error_code ignored;
    socket_.shutdown(tcp::socket::shutdown_both, ignored);
    socket_.close(ignored);
}

void ChatSession::read_header() {
    auto self = shared_from_this();
    boost::asio::async_read(
        socket_,
        boost::asio::buffer(read_header_buffer_),
        boost::asio::bind_executor(
            strand_,
            [this, self](const boost::system::error_code& ec, std::size_t /*bytes_transferred*/) {
                if (ec) {
                    stop();
                    return;
                }

                refresh_activity();

                std::uint32_t body_length = 0;
                if (!asiochat::protocol::decode_header(read_header_buffer_, body_length)) {
                    deliver(asiochat::protocol::make_system_event("Invalid frame length."));
                    stop();
                    return;
                }

                read_body(body_length);
            }));
}

void ChatSession::read_body(std::uint32_t body_length) {
    read_body_buffer_.assign(body_length, '\0');

    auto self = shared_from_this();
    boost::asio::async_read(
        socket_,
        boost::asio::buffer(read_body_buffer_.data(), read_body_buffer_.size()),
        boost::asio::bind_executor(
            strand_,
            [this, self](const boost::system::error_code& ec, std::size_t /*bytes_transferred*/) {
                if (ec) {
                    stop();
                    return;
                }

                refresh_activity();
                handle_payload(std::string(read_body_buffer_.begin(), read_body_buffer_.end()));

                if (!stopped_) {
                    read_header();
                }
            }));
}

void ChatSession::handle_payload(const std::string& payload) {
    const auto command = asiochat::protocol::parse_client_line(payload);
    if (!command.has_value()) {
        return;
    }

    handle_command(*command);
}

void ChatSession::handle_command(const asiochat::protocol::Command& command) {
    switch (command.type) {
    case asiochat::protocol::CommandType::login: {
        if (joined_) {
            deliver(asiochat::protocol::make_system_event("You are already logged in as " + user_name_ + "."));
            return;
        }
        if (command.user.empty()) {
            deliver(asiochat::protocol::make_system_event("Username cannot be empty."));
            return;
        }
        if (!room_) {
            deliver(asiochat::protocol::make_system_event("Room service is unavailable."));
            stop();
            return;
        }

        std::string assigned_name;
        std::string assigned_room;
        const bool kept_original_name = room_->login(shared_from_this(), command.user, assigned_name, assigned_room);
        user_name_ = assigned_name;
        current_room_ = assigned_room;
        joined_ = true;

        mark_online_async();

        if (kept_original_name) {
            deliver(asiochat::protocol::make_system_event("Logged in as " + user_name_ + " in room " + current_room_ + "."));
        } else {
            deliver(asiochat::protocol::make_system_event(
                "Requested username was occupied. You are logged in as " + user_name_ + " in room " + current_room_ + "."));
        }

        room_->broadcast_to_room(
            current_room_,
            asiochat::protocol::make_presence_event(user_name_, current_room_, "joined", room_->online_count(current_room_)));
        deliver(asiochat::protocol::make_users_event(current_room_, room_->room_users(current_room_)));
        deliver_room_ai_welcome(room_, current_room_);
        load_offline_messages_async();
        return;
    }
    case asiochat::protocol::CommandType::join_room: {
        if (!joined_) {
            deliver(asiochat::protocol::make_system_event("Please log in first with /login <name>."));
            return;
        }
        if (command.room.empty()) {
            deliver(asiochat::protocol::make_system_event("Room name cannot be empty."));
            return;
        }
        if (!room_) {
            deliver(asiochat::protocol::make_system_event("Room service is unavailable."));
            stop();
            return;
        }

        std::string previous_room;
        std::string assigned_room;
        if (!room_->change_room(this, command.room, previous_room, assigned_room)) {
            deliver(asiochat::protocol::make_system_event("Unable to switch rooms right now."));
            return;
        }
        if (previous_room == assigned_room) {
            deliver(asiochat::protocol::make_system_event("You are already in room " + assigned_room + "."));
            return;
        }

        room_->broadcast_to_room(
            previous_room,
            asiochat::protocol::make_presence_event(user_name_, previous_room, "left", room_->online_count(previous_room)),
            this);
        current_room_ = assigned_room;
        deliver(asiochat::protocol::make_system_event("Switched to room " + current_room_ + "."));
        room_->broadcast_to_room(
            current_room_,
            asiochat::protocol::make_presence_event(user_name_, current_room_, "joined", room_->online_count(current_room_)));
        deliver(asiochat::protocol::make_users_event(current_room_, room_->room_users(current_room_)));
        deliver_room_ai_welcome(room_, current_room_);
        return;
    }
    case asiochat::protocol::CommandType::message: {
        if (!joined_) {
            deliver(asiochat::protocol::make_system_event("Please log in first with /login <name>."));
            return;
        }
        if (command.message.empty() || !room_) {
            return;
        }

        room_->broadcast_to_room(
            current_room_,
            asiochat::protocol::make_chat_event(user_name_, current_room_, command.message));
        return;
    }
    case asiochat::protocol::CommandType::private_message: {
        if (!joined_) {
            deliver(asiochat::protocol::make_system_event("Please log in first with /login <name>."));
            return;
        }
        if (command.target.empty() || command.message.empty()) {
            deliver(asiochat::protocol::make_system_event("Usage: /pm <user> <message>."));
            return;
        }
        if (!room_) {
            deliver(asiochat::protocol::make_system_event("Room service is unavailable."));
            stop();
            return;
        }

        if (room_->is_ai_agent_room_enabled(current_room_) && is_room_ai_mention_target(command.target)) {
            room_->broadcast_to_room(
                current_room_,
                asiochat::protocol::make_chat_event(
                    user_name_,
                    current_room_,
                    format_room_ai_mention_message(command.target, command.message)));
            request_room_ai_reply(current_room_, user_name_, command.message);
            return;
        }

        const std::string event = asiochat::protocol::make_private_event(user_name_, command.target, command.message);
        if (!room_->deliver_private(command.target, event)) {
            store_offline_message_async({user_name_, command.target, command.message});
            deliver(asiochat::protocol::make_system_event(
                "Target user is offline. The private message has been queued for later delivery."));
            return;
        }

        if (command.target != user_name_) {
            deliver(event);
        }
        return;
    }
    case asiochat::protocol::CommandType::list_users: {
        if (!joined_) {
            deliver(asiochat::protocol::make_system_event("Please log in first with /login <name>."));
            return;
        }
        if (room_) {
            deliver(asiochat::protocol::make_users_event(current_room_, room_->room_users(current_room_)));
        }
        return;
    }
    case asiochat::protocol::CommandType::list_rooms: {
        if (room_) {
            deliver(asiochat::protocol::make_rooms_event(room_->room_names()));
        }
        return;
    }
    case asiochat::protocol::CommandType::heartbeat:
        if (joined_) {
            refresh_online_status_async();
        }
        deliver(asiochat::protocol::make_pong_event());
        return;
    case asiochat::protocol::CommandType::quit:
        stop(true);
        return;
    case asiochat::protocol::CommandType::invalid:
        deliver(asiochat::protocol::make_system_event("Unsupported command."));
        return;
    }
}

void ChatSession::write_next() {
    auto self = shared_from_this();
    boost::asio::async_write(
        socket_,
        boost::asio::buffer(write_queue_.front()),
        boost::asio::bind_executor(
            strand_,
            [this, self](const boost::system::error_code& ec, std::size_t /*bytes_transferred*/) {
                if (ec) {
                    stop();
                    return;
                }

                write_queue_.pop_front();
                if (!write_queue_.empty()) {
                    write_next();
                    return;
                }

                if (close_after_write_) {
                    close_socket();
                }
            }));
}

void ChatSession::refresh_activity() {
    last_activity_ = std::chrono::steady_clock::now();
    if (joined_) {
        refresh_online_status_async();
    }
}

void ChatSession::schedule_idle_check() {
    auto self = shared_from_this();
    idle_timer_.expires_after(kIdleCheckInterval);
    idle_timer_.async_wait(boost::asio::bind_executor(
        strand_,
        [this, self](const boost::system::error_code& ec) {
            if (ec || stopped_) {
                return;
            }

            const auto now = std::chrono::steady_clock::now();
            if (now - last_activity_ >= kIdleTimeout) {
                deliver(asiochat::protocol::make_system_event("Connection closed due to heartbeat timeout."));
                stop();
                return;
            }

            schedule_idle_check();
        }));
}

void ChatSession::submit_store_task(std::function<void()> task) {
    if (!BusinessExecutor::instance().running()) {
        task();
        return;
    }
    BusinessExecutor::instance().submit(std::move(task));
}

void ChatSession::mark_online_async() {
    const std::string user_name = user_name_;
    auto* online_store = &online_status_store_;
    last_status_refresh_ = std::chrono::steady_clock::now();
    submit_store_task([online_store, user_name]() {
        online_store->mark_online(user_name);
    });
}

void ChatSession::mark_offline_async(std::string user_name) {
    auto* online_store = &online_status_store_;
    submit_store_task([online_store, user_name = std::move(user_name)]() {
        online_store->mark_offline(user_name);
    });
}

void ChatSession::refresh_online_status_async() {
    const auto now = std::chrono::steady_clock::now();
    if (now - last_status_refresh_ < kStatusRefreshInterval) {
        return;
    }

    last_status_refresh_ = now;
    const std::string user_name = user_name_;
    auto* online_store = &online_status_store_;
    submit_store_task([online_store, user_name]() {
        online_store->refresh(user_name);
    });
}

void ChatSession::load_offline_messages_async() {
    auto weak_self = weak_from_this();
    const std::string user_name = user_name_;
    auto* offline_store = &offline_message_store_;
    submit_store_task([weak_self, user_name, offline_store]() {
        try {
            const auto offline_messages = offline_store->take_private_messages_for(user_name);
            if (offline_messages.empty()) {
                return;
            }

            if (auto self = weak_self.lock()) {
                boost::asio::post(
                    self->strand_,
                    [self, offline_messages]() {
                        if (self->stopped_) {
                            return;
                        }

                        self->deliver(asiochat::protocol::make_system_event(
                            "You have " + std::to_string(offline_messages.size()) + " offline private message(s)."));
                        for (const auto& message : offline_messages) {
                            self->deliver(asiochat::protocol::make_private_event(message.from, message.to, message.message));
                        }
                    });
            }
        } catch (const std::exception& ex) {
            if (auto self = weak_self.lock()) {
                boost::asio::post(self->strand_, [self, error = std::string(ex.what())]() {
                    if (!self->stopped_) {
                        self->deliver(asiochat::protocol::make_system_event(
                            "Failed to load offline messages: " + error));
                    }
                });
            }
        }
    });
}

void ChatSession::store_offline_message_async(OfflineMessage message) {
    auto weak_self = weak_from_this();
    auto* offline_store = &offline_message_store_;
    submit_store_task([weak_self, offline_store, message = std::move(message)]() {
        try {
            offline_store->save_private_message(message);
        } catch (const std::exception& ex) {
            if (auto self = weak_self.lock()) {
                boost::asio::post(self->strand_, [self, error = std::string(ex.what())]() {
                    if (!self->stopped_) {
                        self->deliver(asiochat::protocol::make_system_event(
                            "Failed to persist offline private message: " + error));
                    }
                });
            }
        }
    });
}

void ChatSession::request_room_ai_reply(std::string room, std::string from_user, std::string message) {
    if (!room_ai_agent_service_ || !room_) {
        return;
    }

    auto weak_self = weak_from_this();
    auto chat_room = room_;
    room_ai_agent_service_->request_reply(
        std::move(room),
        std::move(from_user),
        std::move(message),
        [weak_self, chat_room = std::move(chat_room)](std::string target_room, std::string bot_name, std::string reply_message) mutable {
            if (reply_message.empty()) {
                return;
            }

            if (auto self = weak_self.lock()) {
                boost::asio::post(
                    self->strand_,
                    [self, chat_room = std::move(chat_room), target_room = std::move(target_room), bot_name = std::move(bot_name),
                     reply_message = std::move(reply_message)]() mutable {
                        if (self->stopped_ || !chat_room) {
                            return;
                        }

                        try {
                            chat_room->broadcast_to_room(
                                target_room,
                                asiochat::protocol::make_chat_event(bot_name, target_room, reply_message));
                        } catch (...) {
                            if (!self->stopped_) {
                                self->deliver(asiochat::protocol::make_system_event("AI reply was dropped because it could not be serialized."));
                            }
                        }
                    });
            }
        });
}

}  // namespace asiochat::server


