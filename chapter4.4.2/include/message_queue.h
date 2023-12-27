#pragma once
#include <condition_variable>
#include <mutex>
#include <queue>

namespace messaging {
// 消息基类，队列中存储的项目
struct message_base {
  virtual ~message_base() {}
};

// 派生出一个消息的模板类, 这里Msg一般是自定义的消息struct
template <typename Msg>
struct wrapped_messgae : message_base {
  Msg contents_;
  // explicit关键字禁止单参数的构造函数用于隐式转换
  explicit wrapped_messgae(Msg const& contents) : contents_(contents) {}
};

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