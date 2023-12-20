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
  // 注意packaged_task是可移动不可拷贝的,
  // 因为其内部包含的std::future是不可拷贝的
  std::thread t(std::move(task));
  t.detach();

  // 等待异步任务完成并获取结果
  int value = result.get();
  std::cout << "The result is: " << value << std::endl;
}

// 4. std::promise, 可以在一个线程中通过存储一个值或异常,
// 并允许在另一个线程中通过与std::promise关联的std::future获取这个值或异常;
// 通常用于异步计算结果的传递, 线程间的事件通知, 或是线程间异常的传递
// 相比std::async和std::packaged_task, std::promise允许任务线程
// 自定set value的时机, 使关联的std::future状态为ready
void set_value(std::promise<int> prom) {
  std::this_thread::sleep_for(std::chrono::seconds(5));
  prom.set_value(10);
}

void use_promise() {
  std::promise<int> prom;
  // 获取与promise关联的future对象
  std::future<int> fut = prom.get_future();
  // 任务线程负责设置prom的值
  std::thread t(set_value, std::move(prom));
  // 在主线程获取future的值
  std::cout << "Waiting for the thread to set the value..." << std::endl;
  std::cout << "Value set by the thread: " << fut.get() << std::endl;

  t.join();
}

// 5.1. 将异常保存到future中, 如果async调用的函数,
// 或者packaged_task封装的函数抛出异常, 则异常会保存到与其关联的future中;
// 当调用future的get()函数时, 保存的异常就被抛出
void may_throw() { throw std::runtime_error("Oops, something went wrong"); }

void use_async_throw_exception() {
  std::future<void> result(std::async(std::launch::async, may_throw));

  try {
    result.get();
  } catch (const std::exception& e) {
    std::cerr << "Caught exception: " << e.what() << std::endl;
  }
}

// 5.2. promise可以通过set_exception()函数显式地设置异常,
// 该方法接受一个std::exception_ptr参数,
// 该参数可以通过调用std::current_exception()函数获取
void set_exception(std::promise<void> prom) {
  try {
    throw std::runtime_error("An error ocurred!");
  } catch (...) {
    prom.set_exception(std::current_exception());
  }
}

void use_promoise_set_exception() {
  std::promise<void> prom;
  std::future<void> fut = prom.get_future();
  // 任务线程设置promise的异常
  std::thread t(set_exception, std::move(prom));
  // 在主线程获取future中保存的异常
  try {
    std::cout << "Waiting for the thread to set the exception..." << std::endl;
    fut.get();
  } catch (const std::exception& e) {
    std::cout << "Exception set by the thread: " << e.what() << std::endl;
  }
  t.join();
}

// 5.3. 如果不调用promise的两个set函数, 或者没有执行packaged_task封装的函数,
// 就将promise/packaged_task对象销毁了, 析构函数会在future中保存异常
// 当其他线程再调用与之关联的future的get()函数时就会报错
void set_value_fake(std::promise<int> prom) {
  std::this_thread::sleep_for(std::chrono::seconds(5));
  // prom.set_value(10);
}

void use_promise_destruct() {
  std::thread t;
  std::future<int> fut;
  {
    std::promise<int> prom;
    fut = prom.get_future();
    t = std::thread(set_value_fake, std::move(prom));
  }  // 离开局部作用域, promise被析构
  // 在主线程获取future的值, 会报错std::future_error::Broken promise
  std::cout << "Waiting for the thread to set the value..." << std::endl;
  std::cout << "Value set by the thread: " << fut.get() << std::endl;
  t.join();
}

void use_packaged_task_destruct() {
  std::thread t;
  std::future<int> fut;
  {
    std::packaged_task<int()> task(my_task);
    fut = task.get_future();
    // 这里不执行task
    // t = std::thread(std::move(task));
    // t.detach();
  }
  // 在主线程获取future的值, 会报错std::future_error::Broken promise
  std::cout << "Waiting for the thread to set the value..." << std::endl;
  std::cout << "Value set by the thread: " << fut.get() << std::endl;
}

// 6. std::shared_future允许多个线程等待同一目标事件,
// 使用时向每个线程传递std::shared_future对象的副本,
// 它们通过自有的std::shared_future对象读取状态数据, 访问是安全的
void MyFunction(std::promise<int>&& promise) {
  std::this_thread::sleep_for(std::chrono::seconds(1));
  promise.set_value(42);
}

void ThreadFunction(std::shared_future<int> future) {
  try {
    int result = future.get();
    std::cout << "Result: " << result << std::endl;
  } catch (const std::future_error& e) {
    std::cout << "Future error: " << e.what() << std::endl;
  }
}

void use_shared_future() {
  std::promise<int> prom;
  std::shared_future<int> sf(prom.get_future());  // 隐式转移归属权
  // auto sf = prom.get_future().share();   // 也可以这样创建

  std::thread my_thread1(MyFunction, std::move(prom));
  // 将shared_future的副本传递给子线程,
  std::thread my_thread2(ThreadFunction, sf);
  std::thread my_thread3(ThreadFunction, sf);

  my_thread1.join();
  my_thread2.join();
  my_thread3.join();
}

#endif