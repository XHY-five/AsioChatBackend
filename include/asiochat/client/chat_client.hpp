#pragma once

#include <boost/asio.hpp>

#include <array>
#include <deque>
#include <memory>
#include <string>
#include <vector>

namespace asiochat::client {

class ChatClient : public std::enable_shared_from_this<ChatClient> {
public:
    ChatClient(boost::asio::io_context& io_context,
               const boost::asio::ip::tcp::resolver::results_type& endpoints);

    void start();
    void write(std::string line);
    void close();

private:
    void read_header();
    void read_body(std::uint32_t body_length);
    void write_next();

    boost::asio::io_context& io_context_;
    boost::asio::ip::tcp::socket socket_;
    boost::asio::strand<boost::asio::any_io_executor> strand_;
    boost::asio::ip::tcp::resolver::results_type endpoints_;
    std::array<unsigned char, 4> read_header_buffer_{};
    std::vector<char> read_body_buffer_;
    std::deque<std::string> write_queue_;
    bool connected_{false};
    bool closed_{false};
};

}  // namespace asiochat::client
