#include "hierarchical_mutex.h"

#include <climits>
#include <mutex>
#include <thread>
// 8. 层级锁，有时需要两个锁分开加锁，又要保证安全性，
// 则可以通过层级锁来保证对多个互斥量加锁是有序的
// 思路是保证每个线程加锁时，先加level高的锁，加锁的顺序只能是从高到低
// 通过将当前锁的权重保存在线程变量中，线程再次加锁时只能加比当前锁权重更小的锁
class hierarchical_mutex {
 public:
  explicit hierarchical_mutex(unsigned long value)
      : hierarchical_value_(value), previous_hierarchical_value_(0) {}

  // 禁用拷贝构造和拷贝赋值，则也不能移动了
  hierarchical_mutex(const hierarchical_mutex& other) = delete;
  hierarchical_mutex& operator=(const hierarchical_mutex&) = delete;

  void lock() {
    check_for_hierarchical_violation();
    internal_mutex_.lock();
    update_hierarchical_value();
  }

  void unlock() {
    if (this_thread_hierarchical_value_ != hierarchical_value_) {
      // 加锁和解锁不是同一把锁，说明有问题
      throw std::logic_error("mutex hierarchy violated");
    }
    this_thread_hierarchical_value_ = previous_hierarchical_value_;
    internal_mutex_.unlock();
  }

  bool try_lock() {
    check_for_hierarchical_violation();
    if (!internal_mutex_.try_lock()) {
      return false;
    }
    update_hierarchical_value();
    return true;
  }

 private:
  std::mutex internal_mutex_;
  // 当前层级值
  unsigned long const hierarchical_value_;
  // 上一级层级值
  unsigned long previous_hierarchical_value_;
  // 使用thread_local变量记录当前线程的层级值
  // every thread has its own copy, the state of the variable in one thread is indepedent
  static thread_local unsigned long this_thread_hierarchical_value_;

  void check_for_hierarchical_violation() {
    if (this_thread_hierarchical_value_ <= hierarchical_value_) {
      throw std::logic_error("mutex hierarchy violated");
    }
  }

  void update_hierarchical_value() {
    previous_hierarchical_value_ = this_thread_hierarchical_value_;
    this_thread_hierarchical_value_ = hierarchical_value_;
  }
};

thread_local unsigned long hierarchical_mutex::this_thread_hierarchical_value_(
    ULONG_MAX);

void test_heirarchy_lock() {
  hierarchical_mutex hmtx1(1000);
  hierarchical_mutex hmtx2(500);

  std::thread t1([&hmtx1, &hmtx2]() {
    hmtx1.lock();
    hmtx2.lock();
    hmtx2.unlock();
    hmtx1.unlock();
  });

  std::thread t2([&hmtx1, &hmtx2]() {
    hmtx2.lock();
    hmtx1.lock();
    hmtx1.unlock();
    hmtx2.unlock();
  });
  // 用层级锁测试，立马会抛出异常表示有死锁

  t1.join();
  t2.join();
}