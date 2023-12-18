#ifndef promise_sample_h_
#define promise_sample_h_
#include <chrono>
#include <future>
#include <iostream>

// 1. std::future的作用类似于一次性的条件变量, 它可以表示一个异步操作的结果
// 这个结果可能在将来的某个时间点变成可用状态
// std::thread并未提供直接回传计算结果的方法

// std::future::get() 阻塞调用, 用于获取std::future对象关联的值或异常,
// 如果异步任务还没完成, get()会阻塞当前线程; get()只能调用一次

// std::future::wait() 阻塞调用, 只等待异步任务完成不返回future关联的值
// wait()可以多次调用

// std::future 本身不提供同步访问机制，如果存在多个线程访问
// std::future对象的情况，需要用mutex或其他同步机制来保护它
// 通常与std::promoise, std::packaged_task, std::async一起使用

// 2. std::async是用于启动异步任务的函数模板, 它返回一个std::future对象
// 通过future对象获取异步执行函数的返回值, 当需要用到这个值时, 就调用
// future对象的get()函数, 当前线程就会阻塞等待异步执行函数的结果返回
std::string FetchDataFromDB(std::string query) {
  // 模拟一个异步任务, 比如从数据库中获取数据
  std::this_thread::sleep_for(std::chrono::seconds(5));
  return "Data: " + query;
}

void use_async() {
  // 异步调用
  std::future<std::string> result =
      std::async(std::launch::async, FetchDataFromDB, "Data");

  // 工作线程先做其他事情
  std::cout << "Doing something else..." << std::endl;

  // 通过future获取异步任务的结果
  std::string data = result.get();
  std::cout << data << std::endl;
}
// std::async的启动策略:
// (1) std::launch::async, 指定异步执行, 即新开辟一个线程执行输入的函数
// (2) std::launch::deferred,
// 任务将在调用std::future::get()或std::future::wait()时才执行 (3)
// std::launch::async | std::launch::deferred, 默认策略,
//     采用哪种由std::async的具体实现决定, 取决于编译器和标准库的实现

// 3. std::packaged_task<>是一个类模板, 模板参数是函数签名function signature
// 它包装了一个可调用对象, 允许异步获取该可调用对象的结果;
// 当std::packaged_task对象被调用时, 它会调用内部的可调用对象,
// 并将结果存储到std::future, 以便以后获取共享状态

// 成员函数get_future()返回一个std::future<>实例,其特化类型取决于函数签名的返回值;
// std::packaged_task<>重载了函数调用操作符, 其参数取决于函数签名的参数
int my_task() {
  std::this_thread::sleep_for(std::chrono::seconds(5));
  std::cout << "my task run 5s" << std::endl;
  return 42;
}

void use_packaged_task() {
  // 创建一个包装了任务的std::packaged_taskd对象
  std::packaged_task<int()> task(my_task);
  // 获取与其关联的std::future对象
  std::future<int> result = task.get_future();

  // 在另一个线程上执行任务
  // 注意packaged_task是可移动不可拷贝的, 因为其内部包含的std::future是不可拷贝的
  std::thread t(std::move(task));
  t.detach();

  // 等待异步任务完成并获取结果
  int value = result.get();
  std::cout << "The result is: " << value << std::endl;
}

// 4. std::promise, 可以在一个线程中通过存储一个值或异常,
// 并允许在另一个线程中通过与std::promise关联的std::future获取这个值或异常;
// 通常用于异步计算结果的传递, 线程间的事件通知, 或是线程间异常的传递

#endif