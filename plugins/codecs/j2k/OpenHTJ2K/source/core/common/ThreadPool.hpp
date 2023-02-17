// Copyright (c) 2021, Aaron Boxer
// Copyright (c) 2022, Osamu Watanabe
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
//    modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
// list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimer in the documentation
// and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
//    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
//    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
//    DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
//    SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
//    CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifdef OPENHTJ2K_THREAD
  #pragma once

  #include <atomic>
  #include <cstdint>
  #include <functional>
  #include <future>
  #include <map>
  #include <memory>
  #include <mutex>
  #include <queue>
  #include <thread>
  #include <type_traits>

class ThreadPool {
 public:
  inline explicit ThreadPool(size_t thread_count) : stop(false), thread_count_(thread_count) {
    // if (thread_count == 1) return;

    threads = std::make_unique<std::thread[]>(thread_count_);

    for (size_t i = 0; i < thread_count_; ++i) {
      threads[i] = std::thread(&ThreadPool::worker, this);
    }
    size_t t = 0;
    for (size_t i = 0; i < thread_count_; ++i) {
      id_map[threads[i].get_id()] = t;
      t++;
    }
  }

  /**
   * @brief Destruct the thread pool. Waits for all tasks to complete, then destroys all threads.
   *
   */
  inline ~ThreadPool() {
    {
      // Lock task queue to prevent adding a new task.
      std::lock_guard<std::mutex> lock(tasks_mutex);
      stop = true;
    }

    // Wake up all threads so that they may exist
    condition.notify_all();

    for (size_t i = 0; i < thread_count_; ++i) {
      threads[i].join();
    }
  }

  int thread_number(std::thread::id id) {
    if (id_map.find(id) != id_map.end()) return (int)id_map[id];
    return -1;
  }

  size_t num_threads() const { return thread_count_; }

  #if ((defined(_MSVC_LANG) && _MSVC_LANG >= 201703L) || __cplusplus >= 201703L)
  /**
   * @brief Enqueue a function with zero or more arguments and a return value into the task queue,
   * and get a future for its eventual returned value.
   */
  template <typename F, typename... Args,
            typename R = std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>>
  #else
  template <typename F, typename... Args,
            typename R = typename std::result_of<std::decay_t<F>(std::decay_t<Args>...)>::type>
  #endif
  std::future<R> enqueue(F&& func, Args&&... args) {
    auto task   = std::make_shared<std::packaged_task<R()>>([func, args...]() { return func(args...); });
    auto future = task->get_future();

    push_task([task]() { (*task)(); });
    return future;
  }

  static ThreadPool* get() { return instance(0); }

  static ThreadPool* instance(size_t numthreads) {
    std::unique_lock<std::mutex> lock(singleton_mutex);
    if (!singleton) {
      singleton = new ThreadPool(numthreads ? numthreads : std::thread::hardware_concurrency());
    }
    return singleton;
  }

  static void release() {
    std::unique_lock<std::mutex> lock(singleton_mutex);
    delete singleton;
    singleton = nullptr;
  }

 private:
  template <typename F>
  inline void push_task(const F& task) {
    {
      const std::lock_guard<std::mutex> lock(tasks_mutex);

      if (stop) {
        throw std::runtime_error("Cannot schedule new task after shutdown.");
      }

      tasks.push(std::function<void()>(task));
    }

    condition.notify_one();
  }

  /**
   * @brief A worker function to be assigned to each thread in the pool.
   *
   *  Continuously pops tasks out of the queue and executes them, as long as the atomic variable running is
   * set to true.
   */
  void worker() {
    for (;;) {
      std::function<void()> task;

      {
        std::unique_lock<std::mutex> lock(tasks_mutex);
        condition.wait(lock, [&] { return !tasks.empty() || stop; });

        if (stop && tasks.empty()) {
          return;
        }

        task = std::move(tasks.front());
        tasks.pop();
      }

      task();
    }
  }

 private:
  /**
   * @brief A mutex to synchronize access to the task queue by different threads.
   */
  mutable std::mutex tasks_mutex{};

  /**
   * @brief An atomic variable indicating to the workers to keep running.
   *
   * When set to false, the workers permanently stop working.
   */
  std::atomic<bool> stop;

  std::map<std::thread::id, size_t> id_map;

  /**
   * @brief A queue of tasks to be executed by the threads.
   */
  std::queue<std::function<void()>> tasks;

  /**
   * @brief The number of threads in the pool.
   */
  size_t thread_count_;

  /**
   * @brief A smart pointer to manage the memory allocated for the threads.
   */
  std::unique_ptr<std::thread[]> threads;

  /**
   * @brief A condition variable used to notify worker threads of state changes.
   */
  std::condition_variable condition;

  /**
   * @brief A singleton for the instance.
   */
  static ThreadPool* singleton;

  /**
   * @brief A mutex to synchronize access to the instance.
   */
  static std::mutex singleton_mutex;
};

#endif
