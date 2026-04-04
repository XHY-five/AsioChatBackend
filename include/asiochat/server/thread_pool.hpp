#pragma once

#include "asiochat/server/blocking_queue.hpp"

#include <functional>
#include <thread>
#include <mutex>
#include <vector>

namespace asiochat::server
{

    class ThreadPool
    {
    public:
        using Task = std::function<void()>;
        
        ThreadPool() = default;
        ~ThreadPool();

        ThreadPool(const ThreadPool &) = delete;
        ThreadPool &operator=(const ThreadPool &) = delete;

        void start(unsigned int thread_count);
        void stop();
        void submit(Task task);
        bool running() const;

    private:
        mutable std::mutex mutex_;
        BlockingQueue<Task> queue_;
        std::vector<std::thread> workers_;
        bool running_{false};
    };

} // namespace asiochat::server
