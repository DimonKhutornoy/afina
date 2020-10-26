#ifndef AFINA_CONCURRENCY_EXECUTOR_H
#define AFINA_CONCURRENCY_EXECUTOR_H

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

namespace Afina {
namespace Concurrency {

/**
 * # Thread pool
 */
class Executor {
public:
    enum class State {
        // Threadpool is fully operational, tasks could be added and get executed
        kRun,

        // Threadpool is on the way to be shutdown, no ned task could be added, but existing will be
        // completed as requested
        kStopping,

        // Threadppol is stopped
        kStopped
    };

    Executor(std::string name, int size, size_t min_threads = 2, size_t max_threads = 4, size_t wt_time = 3000)
        : _name(std::move(name)), max_queue_size(size), low_watermark(min_threads), high_watermark(max_threads),
          wt_time(wt_time) {
        std::unique_lock<std::mutex> lock(this->mutex);
        for (int i = 0; i < low_watermark; ++i) {
            threads.emplace_back(std::thread([this]{return thread_worker(this);}));
        }
    }

    ~Executor() {
        if (state != State::kStopped) {
            Stop(true);
        }
    };

    /**
     * Signal thread pool to stop, it will stop accepting new jobs and close threads just after each become
     * free. All enqueued jobs will be complete.
     *
     * In case if await flag is true, call won't return until all background jobs are done and all threads are stopped
     */
    void Stop(bool await = false) {
        std::unique_lock<std::mutex> lock(this->mutex);
        state = State::kStopping;
        empty_condition.notify_all();
        for (auto &thread : threads) {
            if (await) {
                thread.join();
            }
        }
        threads.clear();
        state = State::kStopped;
    };

    /**
     * Add function to be executed on the threadpool. Method returns true in case if task has been placed
     * onto execution queue, i.e scheduled for execution and false otherwise.
     *
     * That function doesn't wait for function result. Function could always be written in a way to notify caller about
     * execution finished by itself
     */
    template <typename F, typename... Types> bool Execute(F &&func, Types... args) {
        // Prepare "task"
        auto exec = std::bind(std::forward<F>(func), std::forward<Types>(args)...);

        std::unique_lock<std::mutex> lock(this->mutex);
        if (state != State::kRun || cur_queue_size >= max_queue_size) {
            return false;
        }

        // Enqueue new task

        if (threads.size() < high_watermark && cur_queue_size++ > 0 && threads.size() == worked_threads) {
            threads.emplace_back(std::thread([this] { return thread_worker(this); }));
        }

        std::unique_lock<std::mutex> _lock(this->mutex);
        tasks.push_back(exec);
        empty_condition.notify_one();
        return true;
    };

private:
    // No copy/move/assign allowed
    Executor(const Executor &);            // = delete;
    Executor(Executor &&);                 // = delete;
    Executor &operator=(const Executor &); // = delete;
    Executor &operator=(Executor &&);      // = delete;

    /**
     * Main function that all pool threads are running. It polls internal task queue and execute tasks
     */
    friend void thread_worker(Executor *executor)
    {
            std::unique_lock<std::mutex> cv_lock(executor->cv_mutex);
            while (executor->state == Executor::State::kRun) {
                std::function<void()> task;
                {
                    while (executor->tasks.empty()) {
                        executor->empty_condition.wait_for(cv_lock, std::chrono::milliseconds(executor->wt_time));
                        if (executor->tasks.empty() && executor->threads.size() > executor->low_watermark) {
                            auto this_thread_id = std::this_thread::get_id();
                            for (auto it = executor->threads.begin(); it < executor->threads.end(); it++) {
                                if (it->get_id() == this_thread_id) {
                                    it->detach();
                                    executor->threads.erase(it);
                                    return;
                                }
                            }
                        }
                    }
                    task = std::move(executor->tasks.front());
                    executor->tasks.pop_front();
                    executor->cur_queue_size--;
                    executor->worked_threads++;
                }
                task();
                executor->worked_threads--;
            }
    }

    /**
     * Mutex to protect state below from concurrent modification
     */
    std::mutex mutex;

    /**
     * Conditional variable to await new data in case of empty queue
     */
    std::condition_variable empty_condition;

    /**
     * Vector of actual threads that perorm execution
     */
    std::vector<std::thread> threads;

    /**
     * Task queue
     */
    std::deque<std::function<void()>> tasks;

    /**
     * Flag to stop bg threads
     */
    State state = State::kRun;
    size_t low_watermark;
    size_t high_watermark;
    size_t max_queue_size;
    size_t wt_time;
    std::string _name;
    size_t cur_queue_size = 0;
    size_t worked_threads = 0;
    std::mutex cv_mutex;
};

} // namespace Concurrency
} // namespace Afina

#endif // AFINA_CONCURRENCY_EXECUTOR_H

