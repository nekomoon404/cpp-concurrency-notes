#include <condition_variable>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>

#include "threadsafe_queue.h"
#include "future_sample.h"
#include "thread_pool.h"
#include "parallel_quick_sort.h"
// 1. C++标准提供了两种条件变量:
// std::condition_variable 和 std::condition_variable_any
std::mutex mtx;
std::queue<int> data_queue;
std::condition_variable data_cond;
void data_preparation_thread() {
  int num = 10;
  while (num--) {
    int i = num;  // prepare data sample
    {
      std::lock_guard<std::mutex> lock(mtx);
      data_queue.push(i);
    }
    data_cond.notify_one();
  }
}
void data_processing_data() {
  while (true) {
    std::unique_lock<std::mutex> lock(mtx);
    data_cond.wait(lock, []() { return !data_queue.empty(); });
    int data = data_queue.front();
    data_queue.pop();
    lock.unlock();  // proecess data前解锁
    std::cout << "process data sample: " << data << std::endl;
    if (data == 0) {  // last data chunk
      break;
    }
  }
}
void TestCondSample() {
  std::thread t1(data_preparation_thread);
  std::thread t2(data_processing_data);

  t1.join();
  t2.join();
}

// 2. 使用条件变量的例子: 两个线程交替打印1和2
int num = 1;
std::mutex num_mtx;
std::condition_variable cv_a;
std::condition_variable cv_b;
void AlternatePrint() {
  std::thread t1([]() {
    for (;;) {
      std::unique_lock<std::mutex> lock(num_mtx);

      // 写法1: while循环检查条件是否满足，不能用if，因为存在虚假唤醒的情况;
      // cv_a被通知后，仍需要检查条件是否满足
      // while (num != 1) {
      //   cva.wait();
      // }

      // 写法2:
      // 使用谓词predicate；当cv_a被通知时，线程被唤醒，它会重新获得锁并检查条件是否满足，
      // 如果条件满足, 则从wait()返回并继续持有锁;如果条件不满足则释放锁,
      // 并继续挂起; 这样就避免虚假唤醒 spurious wakeup
      cv_a.wait(lock, []() { return num == 1; });

      num++;
      // lock.unlock(); // 访问完共享数据后可以及时unlock
      std::cout << "thread A print 1..." << std::endl;
      cv_b.notify_one();
    }
  });

  std::thread t2([]() {
    for (;;) {
      std::unique_lock<std::mutex> lock(num_mtx);
      cv_b.wait(lock, []() { return num == 2; });

      num--;
      // lock.unlock();
      std::cout << "thread B print 2..." << std::endl;
      cv_a.notify_one();
    }
  });

  t1.join();
  t2.join();
}

void TestSafeQueue() {
  threadsafe_queue<int> safe_queue;
  std::mutex print_mtx;
  std::thread producer([&](){
    for(int i = 0; ;++i) {
      safe_queue.push(i);
      {
        std::lock_guard<std::mutex> lock(print_mtx);
        std::cout << "producer push data is " << i << std::endl;
        // 打印时加个锁, 避免多个线程同时调std::cout时缓冲区混乱
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
  });

  std::thread constumer1([&](){
    for(;;) {
      std::shared_ptr<int> data = safe_queue.wait_and_pop();
      {
        std::lock_guard<std::mutex> lock(print_mtx);
        std::cout << "constumer1 wait_and_pop data is " << *data << std::endl;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
  });

  std::thread constumer2([&](){
    for(;;) {
      std::shared_ptr<int> data = safe_queue.try_pop();
      if (data != nullptr) {
        {
          std::lock_guard<std::mutex> lock(print_mtx);
          std::cout << "constumer2 try_pop data is " << *data << std::endl;
        }
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
  });

  producer.join();
  constumer1.join();
  constumer2.join();
}

int main() {
  // 1. 条件变量示例
  // TestCondSample();
  // AlternatePrint();
  // TestSafeQueue();


  // 2. async, packaged_task, promise示例
  // use_async();
  // use_packaged_task();
  // use_promise();


  // 3. 线程间抛出异常示例
  // use_async_throw_exception();
  // use_promoise_set_exception();
  // use_promise_destruct();
  // use_packaged_task_destruct();

  // 4. shared_future示例
  // use_shared_future();

  // 5. 线程池示例
  int m = 0;
  // ThreadPool::instance().Commit([](int& m) {
  //   m = 1024;
  //   std::cout << "inner set m is " << m << std::endl;
  //   std::cout << "m address is " << &m << std::endl;
  // }, m);

  // std::this_thread::sleep_for(std::chrono::seconds(3));
  // std::cout << "m is " << m << std::endl;
  // std::cout << "m address is " << &m << std::endl;
  
  // 执行后会发现子线程和主线程中变量m的值和地址不一样
  // 关注Commit的参数Args&&...args右值引用的模板, 
  // 特化成int&&&m, 然后引用折叠成int& m, 即一个左值引用
  // std::bind构造函数里通过 decay_t<_Types> 会把引用去掉(这里_Types是int&), 
  // 即变成了右值/副本, 所以子线程修改的是m副本的值

  // 正确的写法
  ThreadPool::instance().Commit([](int& m) {
    m = 1024;
    std::cout << "inner set m is " << m << std::endl;
    std::cout << "m address is " << &m << std::endl;
  }, std::ref(m));
  // std::ref则是封装了一个wrapper, 它实现了一个仿函数, 返回m的地址

  std::this_thread::sleep_for(std::chrono::seconds(3));
  std::cout << "m is " << m << std::endl;
  std::cout << "m address is " << &m << std::endl;


  // 6. 并行版快速排序示例
  // test_sequential_sort();
  // test_parallel_sort();
  // test_thread_pool_sort();

  return 0;
}