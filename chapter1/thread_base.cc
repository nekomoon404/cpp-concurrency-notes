#include <iostream>
#include <string>
#include <thread>
#include <memory>

void thread_work1(std::string str) {
  std::cout << "str is " << str << std::endl;
}

// 2.仿函数作为参数，仿函数可以作为算法函数的参数传递
class background_task {
 public:
  void operator()() { std::cout << "background_taskc called" << std::endl; }
};

struct func {
  int& i_;
  func(int i) : i_(i) {}
  void operator()() {
    for (int i = 0; i < 3; ++i) {
      i_ = i;
      std::cout << "i_ = " << i_ << std::endl;
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  }
};

// 2.detach在某些情况下有风险
void oops() {
  int some_local_state = 1;
  func myfunc(some_local_state);
  std::thread functhread(myfunc);
  functhread.join();
  // functhread.detach();
  // 用detach有风险，当oops执行完，局部变量some_local_state被释放
  // 而子线程使用到了它，还在后台运行，容易出现crash
}

// 3.使用RAII技术，保证线程对象析构的时候等待线程运行结束，实现一个线程守卫
class thread_guard {
 private:
  std::thread& t_;

 public:
  explicit thread_guard(std::thread& t) : t_(t) {}
  ~thread_guard() {
    // join只能调用一次
    if (t_.joinable()) {
      t_.join();
    }
  }
  // 禁用类的拷贝构造和拷贝赋值操作
  thread_guard(thread_guard const&) = delete;
  thread_guard& operator=(thread_guard const&) = delete;
};

// 线程守卫的使用
void auto_guard() {
  int some_local_state = 0;
  func my_func(some_local_state);
  std::thread t(my_func);
  thread_guard g(t);
  std::cout << "auto guard finished" << std::endl;
}

void print_str(int i, std::string const& s) {
    std::cout << "i is " << i << " str is " << s << std::endl;
}

// 4.慎用隐式转换，thread构造函数会把参数转成副本，然后转成右值
void danger_oops(int some_param) {
  char buffer[1024];
  sprintf(buffer, "%i", some_param);

  std::thread t(print_str, some_param, buffer);
  // 定义thread对象t时，buffer参数被保存线程的成员变量中
  // 当thread对象t内部启动并运行线程时，参数被传递给调用函数print_str
  // 而此时 buffer 可能随着 } 结束而释放了
  t.detach();
  std::cout << "danger oops finished" << std::endl;
}

// 安全的做法是将buffer传递给thread式显式转换为string
void safe_oops(int some_param) {
  char buffer[1024];
  sprintf(buffer, "%i", some_param);
  std::thread t(print_str, some_param, std::string(buffer));
  t.detach();
}

// 5.当线程要调用的函数的参数为引用类型时，需要将参数显式地转化为引用对象传递给线程的构造函数
// 传递给thread的参数都是按照右值的方式构造为Tuple类型
void change_param(int& param) {
  param++;
}

void ref_oops(int some_param) {
  std::cout << "before change, param is " << some_param << std::endl;
  // std::thread t2(change_param, some_param); // 会编译报错
  // 需要使用引用显式转换
  std::thread t2(change_param, std::ref(some_param));
  t2.join();
  std::cout << "after change, param is " << some_param << std::endl;
}

// 6.绑定类成员函数，前面必须加取地址符&
class X {
  public:
  void do_lengthy_work() {
    std::cout << "do_lengthy_work" << std::endl;
  }
};

void bind_class_oops() {
  X my_x;
  std::thread t(&X::do_lengthy_work, &my_x); // &my_x相当于this指针？
  t.join();
}

// 7.如果传递给线程的参数是独占的，不支持拷贝赋值和构造
// 可以通过 std::move 将参数的所有权转移给线程
void deal_unique(std::unique_ptr<int> p) {
  std::cout << "unique ptr data is " << *p << std::endl;
  (*p)++;

  std::cout << "after unique ptr data is " << *p << std::endl;
}

void move_oops() {
  // auto p = std::make_unique<int>(100); // c++14才引入make_unique
  std::unique_ptr<int> p(new int(100));
  std::thread t(deal_unique, std::move(p));
  t.join();

  // 不能再使用p了，p已经被move废弃
}

int main() {
  std::thread t1(thread_work1, "hello thread");
  t1.join();

  // std::thread t2(background_task()); 会报错
  // 这个仿函数如果带参数也会报错，为什么？
  std::thread t3{background_task()};
  t3.join();

  std::string hellostr = "hello thread";
  // lambda表达式作为thread的参数
  std::thread t4(
      [](std::string str) {
        std::cout << "lambda expersion called, str is " << str << std::endl;
      },
      hellostr);
  t4.join();

  // oops();
  // std::this_thread::sleep_for(std::chrono::seconds(1));

  // 线程守卫示例
  // auto_guard();

  // 隐式转换示例
  // danger_oops(333333);
  // safe_oops(333333);

  // 引用参数示例
  // ref_oops(333);

  // move示例
  move_oops();

  return 0;
}