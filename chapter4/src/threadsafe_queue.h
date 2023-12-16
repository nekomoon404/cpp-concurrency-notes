#ifndef threadsafe_queue_h_
#define threadsafe_queue_h_
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>

template <typename T>
class threadsafe_queue {
 private:
  mutable std::mutex
      mut;  // 定义成mutable，是因为在一些读操作里也需要对mutex进行加锁,
            // 比如const成员函数empty()
  std::queue<T> data_queue;
  std::condition_variable data_cond;

 public:
  threadsafe_queue() {}
  threadsafe_queue(const threadsafe_queue& other) {
    std::lock_guard<std::mutex> lock(other.mut);
    data_queue = other.data_queue;
  }
  threadsafe_queue& operator=(const threadsafe_queue&) = delete;

  bool empty() const {
    std::lock_guard<std::mutex> lock(mut);
    return data_queue.empty();
  }
  void push(T new_value) {
    std::lock_guard<std::mutex> lock(mut);
    data_queue.push(new_value);
    data_cond.notify_one();  // 唤醒等待数据的挂起线程
  }

  bool try_pop(T& value) {
    std::lock_guard<std::mutex> lock(mut);
    if (data_queue.empty()) {
      return false;
    }
    value = data_queue.front();
    data_queue.pop();
    return true;
  }

  std::shared_ptr<T> try_pop() {
    std::lock_guard<std::mutex> lock(mut);
    if (data_queue.empty()) {
      return std::shared_ptr<T>();
    }
    std::shared_ptr<T> res = std::make_shared<T>(data_queue.front());
    data_queue.pop();
    return res;
  }

  void wait_and_pop(T& value) {
    std::unique_lock<std::mutex> lock(mut);
    data_cond.wait(lock, [this]() { return !data_queue.empty(); });
    value = data_queue.front();  // 使用引用传output param, 避免拷贝
    data_queue.pop();
  }

  std::shared_ptr<T> wait_and_pop() {
    std::unique_lock<std::mutex> lock(mut);
    data_cond.wait(lock, [this]() { return !data_queue.empty(); });
    std::shared_ptr<T> res = std::make_shared<T>(data_queue.front());
    data_queue.pop();
    return res;  
    // 返回一个局部的智能指针是安全的, 创建智能指针时, 其引用计数加1为1,
    // 外部调用函数时会将返回的智能指针赋值给外部变量, 做一次拷贝, 引用计数加1为2, 
    // 当离开作用域时,销毁局部的智能指针, 引用计数减1为1, 其所指向的对象仍是安全的
  }
};
#endif  // threadsafe_queue_h_