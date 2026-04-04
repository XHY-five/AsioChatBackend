#pragma once

#include "asiochat/server/singleton.hpp"
#include "asiochat/server/thread_pool.hpp"

namespace asiochat::server{

    class BusinessExecutor : public Singleton<BusinessExecutor>{
        public:
            void initialize(unsigned int thread_count);
            void shutdown();
            void submit(ThreadPool::Task task);
            bool running() const;



        private:
            friend class Singleton<BusinessExecutor>;

            BusinessExecutor() = default;

            ThreadPool pool_;
    };
}//namespace asiochat::server