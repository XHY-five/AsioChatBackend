#pragma once

#include <queue>
#include <utility>
#include <mutex>

namespace asiochat::server
{

    template <typename T>
    class BlockingQueue
    {
    public:
        void push(T value)
        {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (closed_)
                {
                    return;
                }

                queue_.push(std::move(value));
            }

            cv_.notify_one();
        }

        bool pop(T &value)
        {
            std::unique_lock<std::mutex> lock(mutex_);

            cv_.wait(lock, [this]()
                     { return closed_ || !queue_.empty(); });

            if (queue_.empty())
            {
                return false;
            }

            value = std::move(queue_.front());
            queue_.pop();
            return true;
        }

        void close()
        {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                closed_ = true;
            }

            cv_.notify_all();
        }

        void reset()
        {
            std::lock_guard<std::mutex> lock(mutex_);
            closed_ = false;
            queue_ = {};
        }

    private:
        std::mutex mutex_;
        std::queue<T> queue_;
        std::condition_variable cv_;
        bool closed_{false};
    };

} // namespace asiochat::server