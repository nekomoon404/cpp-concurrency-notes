#ifndef thread_pool_h_
#define thread_pool_h_

#include <atomic>
#include <condition_variable>
#include <future>
#include <iostream>
#include <queue>
#include <thread>
#include <vector>
#include <functional>
// 一个线程池大致需要实现三件事:
// 1. task任务队列
// 2. 封装task, 其回调函数需要是一个模板
// 3. 空闲-->挂起, 忙碌<--notify

// 注意事项:
// 1. 线程池要执行的任务是无序的, 如果是要求有序的任务则不应该用线程池来做
// 2. 如果执行的任务关联性很强或者是互斥操作, 也不应该用线程池来做

class ThreadPool {
 public:
  ThreadPool(const ThreadPool&) = delete;
  ThreadPool& operator=(const ThreadPool&) = delete;
  ~ThreadPool() { Stop(); }

  static ThreadPool& instance() {
    static ThreadPool instance;
    return instance;
  }

  using Task = std::packaged_task<void()>;

  // 可变参数模板，返回一个std::future<T>的对象，T是函数f的返回值类型
  template <class F, class... Args>
  auto Commit(F&& f, Args&&... args) -> std::future<decltype(f(args...))> {
    using RetType = decltype(f(args...));
    if (stop_.load()) {
      return std::future<RetType>{};
    }
    auto task = std::make_shared<std::packaged_task<RetType()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...));
    // std::bind将函数f和它的参数绑定在一起, 得到一个无参数的函数

    std::future<RetType> ret = task->get_future();
    {
      std::lock_guard<std::mutex> lock(mtx_);
      tasks_.emplace([task] { (*task)(); });
      // 用lambda表达式构造一个std::packaged_task<void()>对象, 插入任务队列中
      // 等从任务队列中取出Task, 执行Task();
      // 实际上就是执行std::bind绑定得到的那个无参函数
      // lambda表达式捕获了局部变量task的值, 使task的引用计数+1,
      // 保证task在任务函数执行前都是安全的, 不会被释放掉, 从而达到伪闭包的效果
    }
    cv_.notify_one();
    return ret;
  }

 private:
  ThreadPool(unsigned int thread_num = 5) : stop_(false) {
    thread_num_ = thread_num < 1 ? 1 : thread_num;
    Start();
  }

  void Start() {
    for (int i = 0; i < thread_num_; ++i) {
      pool_.emplace_back([this]() {
        while (!this->stop_.load()) {
          Task task;
          {
            std::unique_lock<std::mutex> lock(mtx_);
            // 等线程池停止，或者任务队列不为空
            cv_.wait(lock, [this]() {
              return this->stop_.load() || !tasks_.empty();
            });

            // 被唤醒后可能是stop为true但任务队列为空, 所以需要再次检查任务队列是否为空
            if (tasks_.empty()) {
              return;
            }
            task = std::move(tasks_.front());
            this->tasks_.pop();
          }                     // 离开作用域释放锁
          this->thread_num_--;  // 空闲线程数减1
          task();
          this->thread_num_++;
        }
      });
    }
  }

  void Stop() {
    stop_.store(true);
    cv_.notify_all();  // 通知所有线程
    for (auto& td : pool_) {
      if (td.joinable()) {
        std::cout << "join thread " << td.get_id() << std::endl;
        td.join();
      }
    }
  }

 private:
  std::mutex mtx_;  // 保护任务队列
  std::condition_variable cv_;
  std::atomic_bool stop_;
  std::atomic_int thread_num_;  // 空闲的线程数
  std::queue<Task> tasks_;
  std::vector<std::thread> pool_;
};

#endif  // thread_pool_h_
