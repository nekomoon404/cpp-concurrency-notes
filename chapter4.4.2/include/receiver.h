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
class receiver {
  // 消息队列始终被接受者持有
  queue que_;

 public:
  // 实现了由receiver类型向sender类型的隐式转换
  operator sender() { return sender(&que_); }
  // 等待行为会返回一个dispatcher对象
  dispatcher wait() { return dispatcher(&que_); }
};
}  // namespace messaging


// 1. 用户层发送的消息：
// 用户已插卡
struct card_inserted {
  std::string account_;
  explicit card_inserted(std::string const& account) : account_(account) {}
};
// 用户按数字键输入密码
struct digit_pressed {
  char digit_;
  explicit digit_pressed(char digit) : digit_(digit) {}
};
// 用户点击取消
struct cancel_pressed {};
// 用户点击取钱
struct withdraw_pressed {
  unsigned amount_;
  explicit withdraw_pressed(unsigned amount) : amount_(amount) {}
};


// 2. atm发送给UI层的消息：
// UI提示请插卡
struct display_enter_card {};
// UI提示输入密码
struct display_enter_pin {};
// UI提示密码不正确
struct display_pin_incorrect_message {};
// 弹出卡片
struct eject_card {};
// UI显示取钱选项
struct display_withdrawal_options {};
// UI提示余额不足
struct display_insufficient_funds {};
// 给用户吐钱
struct issue_money {
  unsigned amount_;
  issue_money(unsigned amount) : amount_(amount) {}
};
// UI提示取钱取消
struct display_withdrawal_canceled {};


// 3. atm发送给bank的消息：
// 请求bank校验密码
struct verify_pin {
  std::string account_;
  std::string pin_;
  mutable messaging::sender sender_to_atm_;
  verify_pin(std::string const& account, std::string const& pin,
             messaging::sender sender_to_atm)
      : account_(account), pin_(pin), sender_to_atm_(sender_to_atm) {}
};

// 请求取钱
struct request_withdraw {
  std::string account_;
  unsigned int amount_;
  mutable messaging::sender sender_to_atm_;
  request_withdraw(std::string const& account, unsigned int amount,
                   messaging::sender sender_to_atm)
      : account_(account), amount_(amount), sender_to_atm_(sender_to_atm) {}
};

// 通知已完成取钱操作
struct complete_withdraw {
  std::string account_;
  unsigned int amount_;
  complete_withdraw(std::string const& account, unsigned int amount)
      : account_(account), amount_(amount) {}
};


// 4. bank发送给atm的消息
// 密码验证通过
struct pin_verified {};

// 密码验证失败
struct pin_incorrect {};

// 取钱成功
struct withdraw_success {};

// 取钱失败
struct withdraw_denied {};
