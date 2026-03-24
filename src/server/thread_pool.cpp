#include "server/thread_pool.hpp"

#include <algorithm>
#include <stdexcept>

namespace asiochat::server {

ThreadPool::~ThreadPool() {
    stop();
}

void ThreadPool::start(unsigned int thread_count) {
    if (thread_count == 0) {
        thread_count = std::max(2u, std::thread::hardware_concurrency());
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (running_) {
        return;
    }

    queue_.reset();
    running_ = true;
    workers_.reserve(thread_count);
    for (unsigned int i = 0; i < thread_count; ++i) {
        workers_.emplace_back([this]() {
            Task task;
            while (queue_.pop(task)) {
                if (task) {
                    task();
                }
            }
        });
    }
}

void ThreadPool::stop() {
    std::vector<std::thread> workers;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_) {
            return;
        }

        running_ = false;
        queue_.close();
        workers.swap(workers_);
    }

    for (auto& worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void ThreadPool::submit(Task task) {
    if (!task) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (!running_) {
        throw std::runtime_error("ThreadPool is not running.");
    }
    queue_.push(std::move(task));
}

bool ThreadPool::running() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return running_;
}

}  // namespace asiochat::server
