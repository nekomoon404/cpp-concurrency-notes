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