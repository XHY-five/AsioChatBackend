#include "asiochat/server/chat_session.hpp"

#include "asiochat/server/business_executor.hpp"
#include "asiochat/server/chat_room.hpp"

#include <boost/asio/dispatch.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>

#include <cctype>
#include <iostream>
#include <string>
#include <thread>
#include <utility>

namespace asiochat::server
{
    namespace
    {

        std::string ascii_lower(std::string_view input)
        {
            std::string lowered;
            lowered.reserve(input.size());
            for (unsigned char ch : input)
            {
                lowered.push_back(static_cast<char>(std::tolower(ch)));
            }
            return lowered;
        }

        bool is_room_ai_mention_target(std::string_view target)
        {
            const std::string lowered = ascii_lower(target);
            return lowered == "ai" || lowered == u8"小d";
        }

        std::string format_room_ai_mention_message(std::string_view target, std::string_view message)
        {
            std::string formatted = "@";
            formatted += std::string(target);
            if (!message.empty())
            {
                formatted.push_back(' ');
                formatted += std::string(message);
            }
            return formatted;
        }

    } // namespace

    ChatSession::ChatSession(boost::asio::ip::tcp::socket socket,
                             std::shared_ptr<ChatRoom> room,
                             std::shared_ptr<RoomAiAgentService> room_ai_agent_service,
                             OfflineMessageStore &offline_message_store,
                             OnlineStatusStore &online_status_store,
                             UserStore &user_store,
                             std::chrono::seconds idle_timeout,
                             std::chrono::seconds idle_check_interval)
        : socket_(std::move(socket)),
          strand_(boost::asio::make_strand(socket_.get_executor())),
          idle_timer_(socket_.get_executor()),
          room_(std::move(room)),
          room_ai_agent_service_(std::move(room_ai_agent_service)),
          offline_message_store_(offline_message_store),
          online_status_store_(online_status_store),
          user_store_(user_store),
          last_activity_(std::chrono::steady_clock::now()),
          idle_timeout_(idle_timeout),
          idle_check_interval_(idle_check_interval)
    {
    }

    void ChatSession::start()
    {
        schedule_idle_check();
        read_header();
    }

    void ChatSession::deliver(const std::string &message)
    {
        auto self = shared_from_this();

        boost::asio::dispatch(
            strand_,
            [this, self, message]()
            {
                if (stopped_)
                {
                    return;
                }

                const bool writing = !write_queue_.empty();
                write_queue_.push_back(asiochat::protocol::make_frame(message));

                if (!writing)
                {
                    write_next();
                }
            });
    }

    void ChatSession::read_header()
    {
        auto self = shared_from_this();

        boost::asio::async_read(
            socket_,
            boost::asio::buffer(read_header_buffer_),
            boost::asio::bind_executor(
                strand_,
                [this, self](const boost::system::error_code &ec, std::size_t)
                {
                    if (ec)
                    {
                        close_session();
                        return;
                    }

                    refresh_activity();

                    std::uint32_t body_length = 0;
                    if (!asiochat::protocol::decode_header(read_header_buffer_, body_length))
                    {
                        close_session();
                        return;
                    }

                    read_body(body_length);
                }));
    }

    void ChatSession::read_body(std::uint32_t body_length)
    {
        read_body_buffer_.assign(body_length, '\0');
        auto self = shared_from_this();

        boost::asio::async_read(
            socket_,
            boost::asio::buffer(read_body_buffer_.data(), read_body_buffer_.size()),
            boost::asio::bind_executor(
                strand_,
                [this, self](const boost::system::error_code &ec, std::size_t)
                {
                    if (ec)
                    {
                        close_session();
                        return;
                    }

                    refresh_activity();

                    std::string payload(read_body_buffer_.begin(), read_body_buffer_.end());
                    handle_payload(payload);

                    if (!stopped_)
                    {
                        read_header();
                    }
                }));
    }

    void ChatSession::handle_payload(const std::string &payload)
    {
        const auto parsed = asiochat::protocol::parse_message(payload);
        if (!parsed.has_value())
        {
            asiochat::protocol::Message reply;
            reply.type = "system";
            reply.message = "Failed to parse JSON payload.";
            send_json(reply);
            return;
        }

        const auto &msg = *parsed;

        if (msg.type == "register")
        {
            if (msg.user.empty() || msg.password.empty())
            {
                asiochat::protocol::Message reply;
                reply.type = "register_failed";
                reply.message = "Usage: /register <name> <password>";
                send_json(reply);
                return;
            }

            if (!user_store_.register_user(msg.user, msg.password))
            {
                asiochat::protocol::Message reply;
                reply.type = "register_failed";
                reply.message = "Register failed. Username may already exist.";
                send_json(reply);
                return;
            }

            asiochat::protocol::Message reply;
            reply.type = "register_ok";
            reply.user = msg.user;
            reply.message = "Register succeeded.";
            send_json(reply);
            return;
        }

        if (msg.type == "login")
        {
            if (msg.user.empty() || msg.password.empty())
            {
                asiochat::protocol::Message reply;
                reply.type = "login_failed";
                reply.message = "Usage: /login <name> <password>";
                send_json(reply);
                return;
            }

            if (logged_in_)
            {
                asiochat::protocol::Message reply;
                reply.type = "login_failed";
                reply.message = "You are already logged in as " + user_name_ + ".";
                send_json(reply);
                return;
            }

            if (!user_store_.user_exists(msg.user))
            {
                asiochat::protocol::Message reply;
                reply.type = "login_failed";
                reply.message = "User does not exist.";
                send_json(reply);
                return;
            }

            if (!user_store_.validate_user(msg.user, msg.password))
            {
                asiochat::protocol::Message reply;
                reply.type = "login_failed";
                reply.message = "Invalid username or password.";
                send_json(reply);
                return;
            }

            if (!room_)
            {
                asiochat::protocol::Message reply;
                reply.type = "login_failed";
                reply.message = "Room service is unavailable.";
                send_json(reply);
                return;
            }

            std::string assigned_name;
            std::string assigned_room;
            const bool kept_original_name = room_->login(shared_from_this(), msg.user, assigned_name, assigned_room);

            user_name_ = assigned_name;
            current_room_ = assigned_room;
            logged_in_ = true;
            online_status_store_.mark_online(user_name_);

            asiochat::protocol::Message reply;
            reply.type = "login_ok";
            reply.user = user_name_;
            reply.room = current_room_;
            reply.message = kept_original_name ? "Login succeeded."
                                               : "Requested username was occupied. Logged in with adjusted name.";
            send_json(reply);

            broadcast_room_system_message(current_room_, user_name_ + " joined the room.");
            send_room_ai_welcome_if_needed(current_room_);
            load_offline_message_async();
            return;
        }

        if (msg.type == "join_room")
        {
            if (!logged_in_)
            {
                asiochat::protocol::Message reply;
                reply.type = "system";
                reply.message = "Please login first.";
                send_json(reply);
                return;
            }

            if (msg.room.empty())
            {
                asiochat::protocol::Message reply;
                reply.type = "system";
                reply.message = "Usage: /join <room>";
                send_json(reply);
                return;
            }

            if (!room_)
            {
                asiochat::protocol::Message reply;
                reply.type = "system";
                reply.message = "Room service is unavailable.";
                send_json(reply);
                return;
            }

            std::string previous_room;
            std::string assigned_room;
            if (!room_->change_room(this, msg.room, previous_room, assigned_room))
            {
                asiochat::protocol::Message reply;
                reply.type = "system";
                reply.message = "Unable to switch rooms right now.";
                send_json(reply);
                return;
            }

            if (previous_room == assigned_room)
            {
                asiochat::protocol::Message reply;
                reply.type = "system";
                reply.message = "You are already in room " + assigned_room + ".";
                send_json(reply);
                return;
            }

            room_->broadcast_to_room(previous_room,
                                     asiochat::protocol::serialize_message(
                                         asiochat::protocol::Message{"system", "", "", "", user_name_ + " left the room."}),
                                     this);

            current_room_ = assigned_room;

            asiochat::protocol::Message reply;
            reply.type = "system";
            reply.message = "Switched to room " + current_room_ + ".";
            send_json(reply);

            broadcast_room_system_message(current_room_, user_name_ + " joined the room.");
            send_room_ai_welcome_if_needed(current_room_);
            return;
        }

        if (msg.type == "chat")
        {
            if (!logged_in_)
            {
                asiochat::protocol::Message reply;
                reply.type = "system";
                reply.message = "Please login first.";
                send_json(reply);
                return;
            }

            if (!room_ || msg.message.empty())
            {
                return;
            }

            asiochat::protocol::Message event;
            event.type = "chat";
            event.user = user_name_;
            event.room = current_room_;
            event.message = msg.message;

            room_->broadcast_to_room(current_room_, asiochat::protocol::serialize_message(event));
            return;
        }

        if (msg.type == "private_message")
        {
            if (!logged_in_)
            {
                asiochat::protocol::Message reply;
                reply.type = "system";
                reply.message = "Please login first.";
                send_json(reply);
                return;
            }

            if (msg.target.empty() || msg.message.empty())
            {
                asiochat::protocol::Message reply;
                reply.type = "system";
                reply.message = "Usage: /pm <user> <message>";
                send_json(reply);
                return;
            }

            if (!room_)
            {
                asiochat::protocol::Message reply;
                reply.type = "system";
                reply.message = "Room service is unavailable.";
                send_json(reply);
                return;
            }

            if (room_->is_ai_agent_room_enabled(current_room_) && is_room_ai_mention_target(msg.target))
            {
                asiochat::protocol::Message mention_event;
                mention_event.type = "chat";
                mention_event.user = user_name_;
                mention_event.room = current_room_;
                mention_event.message = format_room_ai_mention_message(msg.target, msg.message);

                room_->broadcast_to_room(current_room_, asiochat::protocol::serialize_message(mention_event));
                request_room_ai_reply(current_room_, user_name_, msg.message);
                return;
            }

            if (msg.target == user_name_)
            {
                asiochat::protocol::Message reply;
                reply.type = "system";
                reply.message = "You cannot private-message yourself.";
                send_json(reply);
                return;
            }

            if (!user_store_.user_exists(msg.target))
            {
                asiochat::protocol::Message reply;
                reply.type = "system";
                reply.message = "User does not exist.";
                send_json(reply);
                return;
            }

            asiochat::protocol::Message event;
            event.type = "private_message";
            event.user = user_name_;
            event.target = msg.target;
            event.message = msg.message;

            if (!room_->deliver_private(msg.target, asiochat::protocol::serialize_message(event)))
            {
                store_offline_message_async({user_name_, msg.target, msg.message});

                asiochat::protocol::Message reply;
                reply.type = "system";
                reply.message = "Target user is offline. The private message has been queued for later delivery.";
                send_json(reply);
                return;
            }

            if (msg.target != user_name_)
            {
                send_json(event);
            }
            return;
        }

        if (msg.type == "list_users")
        {
            if (!logged_in_)
            {
                asiochat::protocol::Message reply;
                reply.type = "system";
                reply.message = "Please login first.";
                send_json(reply);
                return;
            }

            asiochat::protocol::Message reply;
            reply.type = "system";
            reply.message = "Users in room " + current_room_ + ": ";

            const auto users = room_ ? room_->room_users(current_room_) : std::vector<std::string>{};
            for (std::size_t i = 0; i < users.size(); ++i)
            {
                if (i != 0)
                    reply.message += ", ";
                reply.message += users[i];
                if (users[i] == user_name_)
                    reply.message += " (you)";
            }
            send_json(reply);
            return;
        }

        if (msg.type == "list_rooms")
        {
            asiochat::protocol::Message reply;
            reply.type = "system";
            reply.message = "Rooms: ";

            const auto rooms = room_ ? room_->room_names() : std::vector<std::string>{};
            for (std::size_t i = 0; i < rooms.size(); ++i)
            {
                if (i != 0)
                    reply.message += ", ";
                reply.message += rooms[i];
            }
            send_json(reply);
            return;
        }

        if (msg.type == "heartbeat")
        {
            if (logged_in_)
            {
                online_status_store_.refresh(user_name_);
            }

            asiochat::protocol::Message reply;
            reply.type = "pong";
            send_json(reply);
            return;
        }

        if (msg.type == "quit")
        {
            close_session();
            return;
        }

        asiochat::protocol::Message reply;
        reply.type = "system";
        reply.message = "Unknown message type: " + msg.type;
        send_json(reply);
    }

    void ChatSession::write_next()
    {
        if (stopped_ || write_queue_.empty())
        {
            return;
        }

        auto self = shared_from_this();

        boost::asio::async_write(
            socket_,
            boost::asio::buffer(write_queue_.front()),
            boost::asio::bind_executor(
                strand_,
                [this, self](const boost::system::error_code &ec, std::size_t)
                {
                    if (ec)
                    {
                        close_session();
                        return;
                    }

                    write_queue_.pop_front();
                    if (closing_after_write_ && write_queue_.empty())
                    {
                        close_session();
                        return;
                    }

                    if (!write_queue_.empty())
                    {
                        write_next();
                    }
                }));
    }

    void ChatSession::refresh_activity()
    {
        last_activity_ = std::chrono::steady_clock::now();
    }

    void ChatSession::schedule_idle_check()
    {
        auto self = shared_from_this();

        idle_timer_.expires_after(idle_check_interval_);
        idle_timer_.async_wait(
            boost::asio::bind_executor(
                strand_,
                [this, self](const boost::system::error_code &ec)
                {
                    if (ec || stopped_)
                    {
                        return;
                    }

                    const auto now = std::chrono::steady_clock::now();
                    if (now - last_activity_ >= idle_timeout_)
                    {
                        notify_idle_timeout_and_close();
                        return;
                    }

                    schedule_idle_check();
                }));
    }

    void ChatSession::close_session()
    {
        if (stopped_)
        {
            return;
        }
        stopped_ = true;
        closing_after_write_ = false;

        const bool should_announce = logged_in_;
        const std::string user_name = user_name_;
        const std::string room_name = current_room_;

        if (logged_in_)
        {
            online_status_store_.mark_offline(user_name_);
            logged_in_ = false;
        }

        boost::system::error_code ignored;
        idle_timer_.cancel();
        socket_.close(ignored);

        if (room_)
        {
            std::string departed_name;
            std::string departed_room;
            room_->leave(this, departed_name, departed_room);
        }

        if (should_announce && room_)
        {
            broadcast_room_system_message(room_name, user_name + " left the room.");
        }
    }

    void ChatSession::notify_idle_timeout_and_close()
    {
        if (stopped_ || closing_after_write_)
        {
            return;
        }

        idle_timer_.cancel();
        closing_after_write_ = true;

        asiochat::protocol::Message reply;
        reply.type = "system";
        reply.message = "idle timeout";

        const bool writing = !write_queue_.empty();
        write_queue_.push_back(asiochat::protocol::make_frame(asiochat::protocol::serialize_message(reply)));
        if (!writing)
        {
            write_next();
        }
    }

    void ChatSession::submit_store_task(std::function<void()> task)
    {
        if (!BusinessExecutor::instance().running())
        {
            task();
            return;
        }

        BusinessExecutor::instance().submit(std::move(task));
    }

    void ChatSession::load_offline_message_async()
    {
        auto weak_self = weak_from_this();
        auto *store = &offline_message_store_;
        const std::string user_name = user_name_;

        submit_store_task([weak_self, store, user_name]()
                          {
        try {
            const auto messages = store->take_private_messages_for(user_name);

            if (auto self = weak_self.lock()) {
                for (const auto& message : messages) {
                    asiochat::protocol::Message reply;
                    reply.type = "private_message";
                    reply.user = message.from;
                    reply.target = message.to;
                    reply.message = message.message;
                    self->send_json(reply);
                }
            }
        } catch (const std::exception& ex) {
            if (auto self = weak_self.lock()) {
                asiochat::protocol::Message reply;
                reply.type = "system";
                reply.message = std::string("Failed to load offline messages: ") + ex.what();
                self->send_json(reply);
            }
        } });
    }

    void ChatSession::store_offline_message_async(OfflineMessage message)
    {
        auto weak_self = weak_from_this();
        auto *store = &offline_message_store_;

        submit_store_task([weak_self, store, message = std::move(message)]()
                          {
        try {
            store->save_private_message(message);
        } catch (const std::exception& ex) {
            if (auto self = weak_self.lock()) {
                asiochat::protocol::Message reply;
                reply.type = "system";
                reply.message = std::string("Failed to store offline message: ") + ex.what();
                self->send_json(reply);
            }
        } });
    }

    void ChatSession::broadcast_room_system_message(const std::string &room_name, const std::string &text)
    {
        if (!room_)
        {
            return;
        }

        asiochat::protocol::Message message;
        message.type = "system";
        message.message = text;

        room_->broadcast_to_room(room_name, asiochat::protocol::serialize_message(message));
    }

    void ChatSession::send_json(const asiochat::protocol::Message &message)
    {
        deliver(asiochat::protocol::serialize_message(message));
    }

    void ChatSession::send_room_ai_welcome_if_needed(const std::string &room_name)
    {
        if (!room_ || !room_->is_ai_agent_room_enabled(room_name))
        {
            return;
        }

        const auto bot_name = room_->ai_agent_name_for_room(room_name);
        const auto welcome = room_->ai_agent_welcome_for_room(room_name);
        if (!bot_name.has_value() || !welcome.has_value())
        {
            return;
        }

        asiochat::protocol::Message message;
        message.type = "chat";
        message.user = *bot_name;
        message.room = room_name;
        message.message = *welcome;
        room_->broadcast_to_room(room_name, asiochat::protocol::serialize_message(message));
    }

    void ChatSession::request_room_ai_reply(std::string room, std::string from_user, std::string message)
    {
        if (!room_ai_agent_service_ || !room_)
        {
            return;
        }

        auto weak_self = weak_from_this();
        auto chat_room = room_;

        room_ai_agent_service_->request_reply(
            std::move(room),
            std::move(from_user),
            std::move(message),
            [weak_self, chat_room = std::move(chat_room)](std::string target_room,
                                                          std::string bot_name,
                                                          std::string reply_message) mutable
            {
                if (reply_message.empty())
                {
                    return;
                }

                if (auto self = weak_self.lock())
                {
                    boost::asio::post(
                        self->strand_,
                        [self,
                         chat_room = std::move(chat_room),
                         target_room = std::move(target_room),
                         bot_name = std::move(bot_name),
                         reply_message = std::move(reply_message)]() mutable
                        {
                            if (self->stopped_ || !chat_room)
                            {
                                return;
                            }

                            asiochat::protocol::Message ai_event;
                            ai_event.type = "chat";
                            ai_event.user = std::move(bot_name);
                            ai_event.room = std::move(target_room);
                            ai_event.message = std::move(reply_message);

                            chat_room->broadcast_to_room(
                                ai_event.room,
                                asiochat::protocol::serialize_message(ai_event));
                        });
                }
            });
    }

} // namespace asiochat::server
