#pragma once
#include "dispatcher.h"

namespace messaging {
// 消息发送者
class sender {
  // 只持有消息队列的指针
  queue* que_;

 public:
  // 默认构造函数，不持有队列
  sender() : que_(nullptr) {}
  sender(queue* que) : que_(que) {}

  // 向消息队列添加消息
  template <typename Msg>
  void send(Msg const& msg) {
    if (que_) {
      que_->push(msg);
    }
  }
};

// 消息的接受者
class recevier {
  // 消息队列始终被接受者持有
  queue que_;

 public:
  operator sender() { return sender(&que_); }
  // 等待行为会返回一个dispatcher对象
  dispatcher wait() { return dispatcher(&que_); }
};
}  // namespace messaging
