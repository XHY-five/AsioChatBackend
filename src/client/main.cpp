#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <WinSock2.h>
#include <windows.h>
#endif

#include "asiochat/client/chat_client.hpp"
#include "asiochat/common/protocol.hpp"

#include <boost/asio.hpp>

#include <cstdlib>
#include <exception>
#include <iostream>
#include <optional>
#include <string>
#include <thread>

namespace {

void configure_console_utf8() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}

std::optional<asiochat::protocol::Message> build_client_message(const std::string& line) {
    using asiochat::protocol::Message;

    if (line.empty()) {
        return std::nullopt;
    }

    if (line.rfind("/register ", 0) == 0) {
        const auto rest = line.substr(10);
        const auto pos = rest.find(' ');
        if (pos == std::string::npos) {
            return std::nullopt;
        }

        Message msg;
        msg.type = "register";
        msg.user = rest.substr(0, pos);
        msg.password = rest.substr(pos + 1);
        return msg;
    }

    if (line.rfind("/login ", 0) == 0) {
        const auto rest = line.substr(7);
        const auto pos = rest.find(' ');
        if (pos == std::string::npos) {
            return std::nullopt;
        }

        Message msg;
        msg.type = "login";
        msg.user = rest.substr(0, pos);
        msg.password = rest.substr(pos + 1);
        return msg;
    }

    if (line.rfind("/join ", 0) == 0) {
        Message msg;
        msg.type = "join_room";
        msg.room = line.substr(6);
        return msg;
    }

    if (line.rfind("/pm ", 0) == 0) {
        const auto rest = line.substr(4);
        const auto pos = rest.find(' ');
        if (pos == std::string::npos) {
            return std::nullopt;
        }

        Message msg;
        msg.type = "private_message";
        msg.target = rest.substr(0, pos);
        msg.message = rest.substr(pos + 1);
        return msg;
    }

    if (line == "/users") {
        Message msg;
        msg.type = "list_users";
        return msg;
    }

    if (line == "/rooms") {
        Message msg;
        msg.type = "list_rooms";
        return msg;
    }

    if (line == "/ping") {
        Message msg;
        msg.type = "heartbeat";
        return msg;
    }

    if (line == "/quit") {
        Message msg;
        msg.type = "quit";
        return msg;
    }

    Message msg;
    msg.type = "chat";
    msg.message = line;
    return msg;
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        configure_console_utf8();

        const std::string host = argc > 1 ? argv[1] : "127.0.0.1";
        const std::string port = argc > 2 ? argv[2] : "5555";

        boost::asio::io_context io_context;
        boost::asio::ip::tcp::resolver resolver(io_context);
        auto client = std::make_shared<asiochat::client::ChatClient>(
            io_context,
            resolver.resolve(host, port));

        client->start();

        std::thread network_thread([&io_context]() { io_context.run(); });

        bool requested_quit = false;
        std::string line;
        while (std::getline(std::cin, line)) {
            const auto message = build_client_message(line);
            if (!message.has_value()) {
                std::cout << "Invalid input format.\n";
                continue;
            }

            client->write(asiochat::protocol::serialize_message(*message));
            if (message->type == "quit") {
                requested_quit = true;
                break;
            }
        }

        if (!requested_quit) {
            client->close();
        }

        network_thread.join();
    } catch (const std::exception& ex) {
        std::cerr << "Client error: " << ex.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
