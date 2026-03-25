#include "server/chat_server.hpp"
#include "server/app_config.hpp"
#include "server/business_executor.hpp"
#include "server/store_factory.hpp"

#include <boost/asio.hpp>

#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <thread>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {

void configure_console_utf8() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        configure_console_utf8();

        const std::filesystem::path config_path = argc > 1 ? argv[1] : "config.json";
        const asiochat::server::AppConfig config = asiochat::server::load_app_config(config_path);
        const unsigned short port = config.server_port;
        const unsigned int io_thread_count =
            config.worker_threads > 0 ? config.worker_threads : std::max(2u, std::thread::hardware_concurrency());
        const unsigned int business_thread_count = config.business_threads > 0 ? config.business_threads : 2u;

        boost::asio::io_context io_context;
        asiochat::server::BusinessExecutor::instance().initialize(business_thread_count);

        auto offline_message_store = asiochat::server::create_offline_message_store(config);
        auto online_status_store = asiochat::server::create_online_status_store(config);
        asiochat::server::ChatServer server(io_context, config, offline_message_store, online_status_store);

        std::vector<std::thread> workers;
        workers.reserve(io_thread_count);
        for (unsigned int i = 0; i < io_thread_count; ++i) {
            workers.emplace_back([&io_context]() { io_context.run(); });
        }

        std::cout << "Asio chat server listening on port " << port
                  << " with io_threads=" << io_thread_count
                  << " business_threads=" << business_thread_count
                  << ". Config: " << config_path.string()
                  << " offline_store=" << config.offline_store_backend
                  << " online_store=" << config.online_store_backend
                  << " room_ai_agents=" << config.room_ai_agents.size()
                  << " MySQL: " << config.mysql.host << ':' << config.mysql.port
                  << " database=" << config.mysql.database
                  << " Redis: " << config.redis.host << ':' << config.redis.port << "\n";

        for (auto& worker : workers) {
            worker.join();
        }

        asiochat::server::BusinessExecutor::instance().shutdown();
    } catch (const std::exception& ex) {
        asiochat::server::BusinessExecutor::instance().shutdown();
        std::cerr << "Server error: " << ex.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
