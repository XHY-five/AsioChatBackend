#include "server/business_executor.hpp"

namespace asiochat::server {

void BusinessExecutor::initialize(unsigned int thread_count) {
    pool_.start(thread_count);
}

void BusinessExecutor::shutdown() {
    pool_.stop();
}

void BusinessExecutor::submit(ThreadPool::Task task) {
    pool_.submit(std::move(task));
}

bool BusinessExecutor::running() const {
    return pool_.running();
}

}  // namespace asiochat::server
