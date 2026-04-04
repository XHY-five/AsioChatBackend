#include "asiochat/client/chat_client.hpp"

#include "asiochat/common/protocol.hpp"

#include <boost/asio/connect.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>

#include <iostream>
#include <string_view>

#ifdef _WIN32
#include <windows.h>
#endif

namespace asiochat::client {

namespace {

#ifdef _WIN32
std::string wide_to_utf8(std::wstring_view text) {
    if (text.empty()) {
        return {};
    }

    const int size = WideCharToMultiByte(
        CP_UTF8,
        0,
        text.data(),
        static_cast<int>(text.size()),
        nullptr,
        0,
        nullptr,
        nullptr);
    if (size <= 0) {
        return {};
    }

    std::string utf8(size, '\0');
    WideCharToMultiByte(
        CP_UTF8,
        0,
        text.data(),
        static_cast<int>(text.size()),
        utf8.data(),
        size,
        nullptr,
        nullptr);
    return utf8;
}

std::string trim_trailing_newlines(std::wstring text) {
    while (!text.empty() && (text.back() == L'\r' || text.back() == L'\n')) {
        text.pop_back();
    }
    return wide_to_utf8(text);
}
#endif

std::string format_error_message(const boost::system::error_code& ec) {
#ifdef _WIN32
    LPWSTR buffer = nullptr;
    const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    const DWORD length = FormatMessageW(
        flags,
        nullptr,
        static_cast<DWORD>(ec.value()),
        0,
        reinterpret_cast<LPWSTR>(&buffer),
        0,
        nullptr);
    if (length != 0 && buffer != nullptr) {
        std::wstring message(buffer, length);
        LocalFree(buffer);
        const std::string utf8 = trim_trailing_newlines(std::move(message));
        if (!utf8.empty()) {
            return utf8;
        }
    }
#endif

    return ec.message();
}

std::string format_server_message(const std::string& payload) {
    const auto parsed = asiochat::protocol::parse_message(payload);
    if (!parsed.has_value()) {
        return payload;
    }

    const auto& msg = *parsed;

    if (msg.type == "register_ok") {
        return "[register] success: " + msg.user;
    }

    if (msg.type == "register_failed") {
        return "[register] failed: " + msg.message;
    }

    if (msg.type == "login_ok") {
        return "[login] success: " + msg.user + " @ " + msg.room;
    }

    if (msg.type == "login_failed") {
        return "[login] failed: " + msg.message;
    }

    if (msg.type == "system") {
        return "[system] " + msg.message;
    }

    if (msg.type == "chat") {
        if (!msg.room.empty()) {
            return "[" + msg.room + "] " + msg.user + ": " + msg.message;
        }
        return "[chat] " + msg.user + ": " + msg.message;
    }

    if (msg.type == "private_message") {
        return "[pm] " + msg.user + " -> " + msg.target + ": " + msg.message;
    }

    if (msg.type == "pong") {
        return "[pong]";
    }

    return payload;
}

}  // namespace

using boost::asio::ip::tcp;

ChatClient::ChatClient(boost::asio::io_context& io_context,
                       const tcp::resolver::results_type& endpoints)
    : io_context_(io_context),
      socket_(io_context),
      strand_(boost::asio::make_strand(io_context)),
      endpoints_(endpoints) {
}

void ChatClient::start() {
    auto self = shared_from_this();
    boost::asio::async_connect(
        socket_,
        endpoints_,
        boost::asio::bind_executor(
            strand_,
            [this, self](const boost::system::error_code& ec, const tcp::endpoint&) {
                if (ec) {
                    std::cerr << "Connect failed: " << format_error_message(ec) << '\n';
                    close();
                    return;
                }

                connected_ = true;
                std::cout << "Connected. Commands: /register <user> <password>, /login <user> <password>, "
                             "/join <room>, /pm <user|ai|灏廳> <text>, /users, /rooms, /ping, /quit.\n";
                if (!write_queue_.empty()) {
                    write_next();
                }
                read_header();
            }));
}

void ChatClient::write(std::string line) {
    auto self = shared_from_this();
    boost::asio::dispatch(strand_, [this, self, line = std::move(line)]() mutable {
        const bool writing = !write_queue_.empty();
        write_queue_.push_back(asiochat::protocol::make_frame(line));
        if (connected_ && !writing) {
            write_next();
        }
    });
}

void ChatClient::close() {
    auto self = shared_from_this();
    boost::asio::dispatch(strand_, [this, self]() {
        if (closed_) {
            return;
        }

        closed_ = true;
        boost::system::error_code ignored;
        socket_.shutdown(tcp::socket::shutdown_both, ignored);
        socket_.close(ignored);
        io_context_.stop();
    });
}

void ChatClient::read_header() {
    auto self = shared_from_this();
    boost::asio::async_read(
        socket_,
        boost::asio::buffer(read_header_buffer_),
        boost::asio::bind_executor(
            strand_,
            [this, self](const boost::system::error_code& ec, std::size_t) {
                if (ec) {
                    if (!closed_) {
                        std::cerr << "Disconnected: " << format_error_message(ec) << '\n';
                    }
                    close();
                    return;
                }

                std::uint32_t body_length = 0;
                if (!asiochat::protocol::decode_header(read_header_buffer_, body_length)) {
                    std::cerr << "Disconnected: invalid frame length.\n";
                    close();
                    return;
                }

                read_body(body_length);
            }));
}

void ChatClient::read_body(std::uint32_t body_length) {
    read_body_buffer_.assign(body_length, '\0');

    auto self = shared_from_this();
    boost::asio::async_read(
        socket_,
        boost::asio::buffer(read_body_buffer_.data(), read_body_buffer_.size()),
        boost::asio::bind_executor(
            strand_,
            [this, self](const boost::system::error_code& ec, std::size_t) {
                if (ec) {
                    if (!closed_) {
                        std::cerr << "Disconnected: " << format_error_message(ec) << '\n';
                    }
                    close();
                    return;
                }

                const std::string payload(read_body_buffer_.begin(), read_body_buffer_.end());
                std::cout << format_server_message(payload) << '\n';
                read_header();
            }));
}

void ChatClient::write_next() {
    auto self = shared_from_this();
    boost::asio::async_write(
        socket_,
        boost::asio::buffer(write_queue_.front()),
        boost::asio::bind_executor(
            strand_,
            [this, self](const boost::system::error_code& ec, std::size_t) {
                if (ec) {
                    std::cerr << "Write failed: " << format_error_message(ec) << '\n';
                    close();
                    return;
                }

                write_queue_.pop_front();
                if (!write_queue_.empty()) {
                    write_next();
                }
            }));
}

}  // namespace asiochat::client
