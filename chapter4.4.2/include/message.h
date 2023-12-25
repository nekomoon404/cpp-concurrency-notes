#pragma once
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
}  // namespace messaging

// 自定义的消息类
struct display_enter_card {};
struct card_inserted {
  std::string account_;
  explicit card_inserted(std::string const& account) : account_(account) {}
};

struct display_enter_pin {};
struct digit_pressed {
  char digit_;
  explicit digit_pressed(char digit) : digit_(digit) {}
};

// 点击取消
struct cancel_pressed {};