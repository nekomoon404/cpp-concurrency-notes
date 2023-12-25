#pragma once
#include "message_queue.h"

namespace messaging {
// 示意关闭队列的消息
struct close_queue {};

// 消息分发类模板, 为了方便理解先阅读下面的dispatcher类吧
template <typename PreviousDispatcher, typename Msg, typename Func>
class TemplateDispatcher {
 private:
  std::string msg_;
  queue* que_;
  PreviousDispatcher* prev_;
  Func func_;
  bool chained_;

  // 同样不允许复制和拷贝
  TemplateDispatcher(TemplateDispatcher const&) = delete;
  TemplateDispatcher& operator=(TemplateDispatcher const&) = delete;

  // 类模板的instantiations互为友元, 注意这里模板参数与上面声明类用的参数区分开
  template <typename Dispatcher, typename OtherMsg, typename OtherFunc>
  friend class TemplateDispatcher;

  // 析构时调用, 循环等待消息,如果消息处理成功则跳出循环
  void wait_and_dispatch() {
    for (;;) {
      auto msg = que_->wait_and_pop();
      if (dispatch(msg)) {
        break;
      }
    }
  }

  // 判断从队列中取出的消息类型与期望的消息类型，即模板参数Msg匹配
  bool dispatch(std::shared_ptr<message_base> const& msg) {
    // 若消息类型匹配则调用处理函数f
    wrapped_messgae<Msg>* wrapper =
        dynamic_cast<wrapped_messgae<Msg>*>(msg.get());
    if (wrapper) {
      func_(wrapper->contents_);
      return true;
    } else {
      // 若消息类型不匹配，则调用前一个临时对象的dispatch()方法
      return prev_->dispatch(msg);
    }
  }

 public:
  // 允许移动构造
  TemplateDispatcher(TemplateDispatcher&& other)
      : que_(other.que_),
        prev_(other.prev_),
        func_(std::move(other.func_)),
        chained_(other.chained_),
        msg_(other.msg_) {
    other.chained_ = true;
  }

  // 构造函数
  TemplateDispatcher(queue* que, PreviousDispatcher* prev, Func&& func,
                     std::string msg)
      : que_(que), prev_(prev), func_(func), msg_(msg) {
    prev->chained_ = true;
  }

  // handle()方法返回一个实例，用于链式调用handle()
  template <typename OtherMsg, typename OtherFunc>
  TemplateDispatcher<TemplateDispatcher, OtherMsg, OtherFunc> handle(
      OtherFunc&& of, std::string info_msg) {
    return TemplateDispatcher<TemplateDispatcher, OtherMsg, OtherFunc>(
        que_, this, std::forward<OtherFunc>(of), info_msg);
  }

  // 同样允许析构函数抛出异常
  ~TemplateDispatcher() noexcept(false) {
    if (!chained_) {
      wait_and_dispatch();
    }
  }
};

// 消息分发类
class dispatcher {
 private:
  queue* que_;
  bool chained_;  // 是否被链式调用了

  // dispatch的实例不可复制
  dispatcher(dispatcher const&) = delete;
  dispatcher& operator=(dispatcher const&) = delete;

  // 允许TemplateDispatcher类实例访问私有成员
  template <typename Dispatcher, typename Msg, typename Func>
  friend class TemplateDispatcher;

  // 析构时调用, 循环等待消息
  void wait_and_dispatch() {
    for (;;) {
      auto msg = que_->wait_and_pop();
      dispatch(msg);
    }
  }

  // dispatch()判断消息是否属于close_queue类型, 若属于则抛出异常
  // 链式调用recevier.wait().handle().hanle().handle()中
  // wait()返回一个dispatch类的临时对象,
  // 尾部handle()返回的临时对象析构时会循环等待消息,
  // 当从队列取出的消息被正确处理时才会跳出循环
  // 当取出的消息与临时对象模板参数中的消息类型不匹配时，就会调用前一个临时对象的dispatch()方法
  // 即第 行 return prev->dispatch(); 的作用
  // 向前调用直到调用到最前面，即wait()返回的dispatcher临时对象的dispatch()方法，返回false，循环继续
  // 只有当收到close_queue消息时，才能退出循环; 这就是这个函数的作用
  bool dispatch(std::shared_ptr<message_base> const& msg) {
    // msg.get()返回存储在std::shared_ptr中的原始指针，指向shared_ptr所拥有的对象
    if (dynamic_cast<wrapped_messgae<close_queue>*>(msg.get())) {
      throw close_queue();
    }
    return false;
  }

 public:
  // 移动构造函数
  dispatcher(dispatcher&& other) : que_(other.que_), chained_(other.chained_) {
    // source shouldn't wait for message,
    // 当dispatcher作为局部变量返回时，先执行移动构造，再执行析构；
    // 将chained=true，避免局部变量/临时对象析构时执行wait_and_dispatch()循环等待消息
    other.chained_ = true;
  }

  // 构造函数
  explicit dispatcher(queue* que) : que_(que), chained_(false) {}

  // handle()返回一个TemplateDispatcher模板类的实例,
  // recevier.wait().handle().hanle().handle()中wait()返回一个dispatch类的临时对象
  // 后面的handle()都会返回一个TemplateDispater模板类的临时对象，
  // 调用顺序是从左到右, 然后临时对象的析构顺序是从右到左
  // 注意这里第一个模板参数指定了是dispatcher类型，因为要在TemplateDispatcher的构造函数里
  // 将当前dispatcher的chanied_置为true，所以第一个模板参数指定为了dispatcher，
  // 即当前类型，然后在TemplateDispatcher构造函数里传入this指针
  template <typename Msg, typename Func>
  TemplateDispatcher<dispatcher, Msg, Func> handle(Func&& f,
                                                   std::string info_msg) {
    return TemplateDispatcher<dispatcher, Msg, Func>(
        que_, this, std::forward<Func>(f), info_msg);
  }
  // 返回局部变量给外部使用会先调用临时对象的移动构造函数,其chanied被置true,
  // 这样临时对象析构时就不会等待消息，只有链式调用最尾部的临时对象析构时才会循环等待消息

  // noexpect(false)允许析构函数抛出异常，C++析构函数默认是不抛的
  ~dispatcher() noexcept(false) {
    if (!chained_) {
      wait_and_dispatch();
    }
  }
};
}  // namespace messaging