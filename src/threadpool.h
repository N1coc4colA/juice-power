#ifndef JP_THREADPOOL_H
#define JP_THREADPOOL_H

#include <cassert>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <vector>

#include "src/keywords.h"

class ThreadPool
{
public:
    explicit ThreadPool(const size_t num_threads = std::thread::hardware_concurrency())
    {
        assert(m_instance == nullptr);

        m_instance = this;

        for (size_t i = 0; i < num_threads; ++i) {
            workers.emplace_back([this] -> void {
                for (;;) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(this->queue_mutex);

                        this->condition.wait(lock, [this] -> bool { return this->stop || !this->tasks.empty(); });
                        if (this->stop && this->tasks.empty()) {
                            return;
                        }

                        task = std::move(this->tasks.front());
                        this->tasks.pop();
                    }
                    task();
                }
            });
        }
    }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;

    ~ThreadPool()
    {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            stop = true;
        }

        condition.notify_all();
        for (std::thread& worker : workers) {
            worker.join();
        }

        if (m_instance == this) {
            m_instance = nullptr;
        }
    }

    auto operator=(const ThreadPool&) = delete;
    auto operator=(ThreadPool&&) = delete;

    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>>
    {
        using return_type = std::invoke_result_t<F, Args...>;

        auto task = std::make_shared<std::packaged_task<return_type()>>(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
        std::future<return_type> res = task->get_future();
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            if (stop) {
                throw std::runtime_error("enqueue on stopped ThreadPool");
            }

            tasks.emplace([task]() -> auto { (*task)(); });
        }

        condition.notify_one();

        return res;
    }

    _nodiscard auto threadsCount() const { return workers.size(); }

    static auto instance() -> ThreadPool& { return *m_instance; }

private:
    std::vector<std::thread> workers{};
    std::queue<std::function<void()>> tasks{};
    std::mutex queue_mutex{};
    std::condition_variable condition{};
    bool stop = false;
    static ThreadPool* m_instance;
};

#endif // JP_THREADPOOL_H
