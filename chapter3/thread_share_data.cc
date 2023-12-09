#include <iostream>
#include <mutex>
#include <stack>
#include <thread>

#include "hierarchical_mutex.h"
#include "test_dead_lock.h"
#include "other_locks.h"
std::mutex mtx1;
int shared_data = 100;

// 1.通过mutex对共享数据进行加锁，防止多线程访问共享区造成数据不一致问题
void use_lock() {
  while (true) {
    mtx1.lock();
    shared_data++;
    std::cout << "current thread is " << std::this_thread::get_id()
              << std::endl;
    std::cout << "shared data is " << shared_data << std::endl;
    mtx1.unlock();

    std::this_thread::sleep_for(std::chrono::microseconds(10));
  }
}

// 2.使用lock_guard来自动加锁和解锁，
void test_lock() {
  std::thread t1(use_lock);

  std::thread t2([]() {
    while (true) {
      // 当lock_guard离开局部作用域时自动释放锁(RAII机制)，避免对共享资源占用时间太长
      {
        std::lock_guard<std::mutex> lock(mtx1);
        shared_data--;
        std::cout << "current thread is " << std::this_thread::get_id()
                  << std::endl;
        std::cout << "shared data is " << shared_data << std::endl;
      }
      std::this_thread::sleep_for(std::chrono::microseconds(10));
    }
  });

  t1.join();
  t2.join();
}

// 不用在锁保护的范围内传递被保护数据成员的指针或引用，包括：returning them from
// a function, storing them in externally visibile memory, or passing them as
// arguments to user-supplied function
// 否则可能会破坏对被保护数据互斥访问的效果，造成恶性条件竞争
// 错误示例：类成员函数将被保护数据的指针传递给外部函数，外部函数使用数据就不受mutex保护了
class some_data {
  int a;
  std::string b;

 public:
  void do_something() { a++; }
};
class data_wrapper {
 private:
  some_data data;
  std::mutex m;

 public:
  template <typename Function>
  void process_data(Function func) {
    std::lock_guard<std::mutex> lock(m);
    func(data);  // pass protected data to user-supplied function
  }
};
some_data* unprotected;
void malicious_func(some_data& protected_data) {
  unprotected = &protected_data;
}
data_wrapper x;
void test_unsafe_member_func() {
  x.process_data(malicious_func);
  unprotected->do_something();  // unprotected access to protected data
}

// 3.数据安全的错误示例，有时会将对共享数据的访问和修改封装到一个函数里
// 即使在函数内部可以做到线程安全，但如果有返回值给外部使用，则可能存在不安全性
template <typename T>
class threadunsafe_stack {
 private:
  std::stack<T> data;
  // const成员函数，如果想对mutex加锁，这是一个修改行为，因此要将mutex对象声明为mutable
  mutable std::mutex m;

 public:
  threadunsafe_stack() {}
  threadunsafe_stack(const threadunsafe_stack& other) {
    // 避免从other拷贝构造时，other被修改
    std::lock_guard<std::mutex> lock_other(other.m);
    data = other.data;
  }
  // 禁用拷贝赋值
  threadunsafe_stack& operator=(const threadunsafe_stack&) = delete;

  void push(T new_value) {
    std::lock_guard<std::mutex> lock(m);
    data.push(std::move(new_value));  // 调用移动构造，减少一次拷贝
  }

  // 问题代码，需要在pop前做stack判空处理
  T pop() {
    std::lock_guard<std::mutex> lock(m);
    auto element = data.top();
    data.pop();
    return element;
    // 返回局部变量时优先执行移动构造，RVO优化
  }

  // 问题代码，empty的bool值返回给外部，使用的时机不知道
  // 比如线程A先检查stack.empty()，然后调用top()操作；
  // 线程B在上面两个操作之间pop()出了stack中最后一个元素，那么线程A的top()操作就有问题
  bool empty() const {
    std::lock_guard<std::mutex> lock(m);
    return data.empty();
  }
};

void test_threadsafe_stack() {
  threadunsafe_stack<int> safe_stack;
  safe_stack.push(1);

  std::thread t1([&safe_stack]() {
    if (!safe_stack.empty()) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
      safe_stack.pop();
    }
  });

  std::thread t2([&safe_stack]() {
    if (!safe_stack.empty()) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
      safe_stack.pop();
    }
  });
  // 两个线程判断stack不为空后都做了pop操作，导致crash
  t1.join();
  t2.join();
}

// 4.修改上面的线程不安全stack
struct empty_stack : std::exception {
  const char* what() const throw();
};

template <typename T>
class threadsafe_stack {
 private:
  std::stack<T> data;
  // const成员函数，如果想对mutex加锁，这是一个修改行为，因此要将mutex对象声明为mutable
  mutable std::mutex m;

 public:
  threadsafe_stack() {}
  threadsafe_stack(const threadsafe_stack& other) {
    // 避免从other拷贝构造时，other被修改
    std::lock_guard<std::mutex> lock_other(other.m);
    data = other.data;
  }
  // 禁用拷贝赋值
  threadsafe_stack& operator=(const threadsafe_stack&) = delete;

  void push(T new_value) {
    std::lock_guard<std::mutex> lock(m);
    data.push(std::move(new_value));  // 调用移动构造，减少一次拷贝
  }

  std::shared_ptr<T> pop() {
    std::lock_guard<std::mutex> lock(m);
    if (data.empty()) {
      // throw empty_stack();
      // 这时也可以不抛异常，直接返回一个空指针，给外部去判断
      return nullptr;
    }
    std::shared_ptr<T> const res(std::make_shared<T>(data.top()));
    // 调用一次拷贝构造；auto element = data.top();也是调用一次拷贝构造
    data.pop();
    return res;  // 将return T类型优化成return 智能指针
  }

  void pop(T& value) {
    std::lock_guard<std::mutex> lock(m);
    if (data.empty()) {
      throw empty_stack();
    }
    value = data.top();  // 通过传引用来减少拷贝的开销
    data.pop();
  }

  bool empty() const {
    std::lock_guard<std::mutex> lock(m);
    return data.empty();
  }
};

int main() {
  // test_lock();
  // test_threadsafe_stack();

  // test_dead_lock(); // 会死锁
  // test_safe_lock();

  // test_danger_swap();  // 会死锁
  // test_safe_swap();

  test_heirarchy_lock();  // 用层级锁检查死锁

  // use_own_lock(); // unique_lock示例，会死锁

  return 0;
}