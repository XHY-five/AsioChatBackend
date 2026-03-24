#include "client/chat_client.hpp"

#include "common/protocol.hpp"

#include <boost/asio.hpp>

#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>
#include <thread>

int main(int argc, char* argv[]) {
    try {
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
            const std::string encoded = asiochat::protocol::normalize_client_input(line);
            if (encoded.empty()) {
                continue;
            }

            client->write(encoded);
            if (asiochat::protocol::trim(line) == "/quit") {
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
