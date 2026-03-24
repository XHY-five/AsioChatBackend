#pragma once

#include <memory>
#include <mutex>

namespace asiochat::server {

template <typename T>
class Singleton {
public:
    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton&) = delete;
    Singleton(Singleton&&) = delete;
    Singleton& operator=(Singleton&&) = delete;

    static T& instance() {
        std::call_once(init_flag(), []() {
            instance_ptr() = std::unique_ptr<T>(new T());
        });
        return *instance_ptr();
    }

protected:
    Singleton() = default;
    ~Singleton() = default;

private:
    static std::once_flag& init_flag() {
        static std::once_flag flag;
        return flag;
    }

    static std::unique_ptr<T>& instance_ptr() {
        static std::unique_ptr<T> value;
        return value;
    }
};

}  // namespace asiochat::server
