#pragma once
#include <condition_variable>
#include <mutex>
#include <queue>

#include "message.h"
namespace messaging {

// 消息队列
class queue {
  std::mutex mtx_;
  std::condition_variable cv_;
  // 内部队列存在消息基类的智能指针
  std::queue<std::shared_ptr<message_base>> queue_;

 public:
  template <typename Msg>
  void push(Msg const& msg) {
    // 这里没限制队列的最大容量，插入时也就没判断队列是否已满
    std::lock_guard<std::mutex> lock(mtx_);
    // 将消息包装并插入队列
    queue_.push(std::make_shared<wrapped_messgae<Msg>>(msg));
    cv_.notify_all();
  }

  std::shared_ptr<message_base> wait_and_pop() {
    std::unique_lock<std::mutex> lock(mtx_);
    cv_.wait(lock, [this]() { return !queue_.empty(); });
    auto res = queue_.front();
    queue_.pop();
    return res;
  }
};
}  // namespace messaging