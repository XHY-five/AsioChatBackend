#include "client/chat_client.hpp"

#include "common/protocol.hpp"

#include <boost/asio/connect.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>

#include <iostream>

namespace asiochat::client {

using boost::asio::ip::tcp;

ChatClient::ChatClient(boost::asio::io_context& io_context,
                       const tcp::resolver::results_type& endpoints)
    : io_context_(io_context),
      socket_(io_context),
      strand_(boost::asio::make_strand(io_context)),
      endpoints_(endpoints) {}

void ChatClient::start() {
    auto self = shared_from_this();
    boost::asio::async_connect(
        socket_,
        endpoints_,
        boost::asio::bind_executor(
            strand_,
            [this, self](const boost::system::error_code& ec, const tcp::endpoint&) {
                if (ec) {
                    std::cerr << "Connect failed: " << ec.message() << '\n';
                    close();
                    return;
                }

                connected_ = true;
                std::cout << "Connected. Use /login <name>, /join <room>, /pm <user> <text>, /users, /rooms, /ping, /quit.\n";
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
            [this, self](const boost::system::error_code& ec, std::size_t /*bytes_transferred*/) {
                if (ec) {
                    if (!closed_) {
                        std::cerr << "Disconnected: " << ec.message() << '\n';
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
            [this, self](const boost::system::error_code& ec, std::size_t /*bytes_transferred*/) {
                if (ec) {
                    if (!closed_) {
                        std::cerr << "Disconnected: " << ec.message() << '\n';
                    }
                    close();
                    return;
                }

                const std::string payload(read_body_buffer_.begin(), read_body_buffer_.end());
                std::cout << asiochat::protocol::format_server_line(payload) << '\n';
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
            [this, self](const boost::system::error_code& ec, std::size_t /*bytes_transferred*/) {
                if (ec) {
                    std::cerr << "Write failed: " << ec.message() << '\n';
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
