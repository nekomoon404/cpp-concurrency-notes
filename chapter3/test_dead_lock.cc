#include <iostream>
#include <mutex>
#include <thread>

// 5.死锁发生的一种情况：2个线程循环加锁
std::mutex t_lock1;
std::mutex t_lock2;
int m_1 = 0;
int m_2 = 0;

void dead_lock1() {
  while (true) {
    std::cout << "dead_lock1 begin " << std::endl;
    t_lock1.lock();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    m_1 = 1024;
    t_lock2.lock();
    m_2 = 2048;
    t_lock2.unlock();
    t_lock1.unlock();
    std::cout << "dead_lock1 end " << std::endl;
  }
}

void dead_lock2() {
  while (true) {
    std::cout << "dead_lock2 begin " << std::endl;
    t_lock2.lock();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    m_2 = 1024;
    t_lock1.lock();
    m_1 = 2048;
    t_lock1.unlock();
    t_lock2.unlock();
    std::cout << "dead_lock2 end " << std::endl;
  }
}

void test_dead_lock() {
  std::thread t1(dead_lock1);
  std::thread t2(dead_lock2);
  t1.join();
  t2.join();
}

// 6.日常开发中，通常是将一次加锁和解锁的功能封装为独立的函数
// 保证在独立的函数里执行完操作后就解锁，不会导致一个函数里使用多个锁

// 加锁和解锁作为“原子操作”解耦合，各自只管理自己的功能
void atomic_lock1() {
  std::cout << "lock1 begin lock" << std::endl;
  t_lock1.lock();
  m_1 = 1024;
  t_lock1.unlock();
  std::cout << "lock1 end lock" << std::endl;
}

void atomic_lock2() {
  std::cout << "lock2 begin lock" << std::endl;
  t_lock2.lock();
  m_2 = 2048;
  t_lock2.unlock();
  std::cout << "lock2 end lock" << std::endl;
}

void safe_lock1() {
  while (true) {
    atomic_lock1();
    atomic_lock2();
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
}

void safe_lock2() {
  while (true) {
    atomic_lock2();
    atomic_lock1();
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
}

void test_safe_lock() {
  std::thread t1(safe_lock1);
  std::thread t2(safe_lock2);
  t1.join();
  t2.join();
}

// 7.对于要使用两个互斥量的情况，为了避免死锁，可以同时加锁
// 假设有一个很复杂的数据结构，希望尽量避免执行拷贝构造
class some_big_object {
 public:
  some_big_object(int data) : data_(data) {}
  // 拷贝构造
  some_big_object(const some_big_object& other) : data_(other.data_) {}
  // 移动构造
  some_big_object(some_big_object&& other) : data_(std::move(other.data_)) {}
  // 实现了移动构造，还不能用移动赋值，比如obj1=std::move(obj2);

  // 重载赋值运算符（拷贝赋值）
  // 如果没实现移动赋值，obj1=std::move(obj2); 则是调拷贝赋值
  some_big_object& operator = (const some_big_object& other) {
    if(this == &other) {
      return *this;
    }
    data_ = other.data_;
    return *this;
  }

  // 移动赋值
  some_big_object& operator = (const some_big_object&& other) {
    data_ = std::move(other.data_);
    return *this;
  }

  // 重载输出运算符
  friend std::ostream& operator<<(std::ostream& os,
                                  const some_big_object& big_obj) {
    os << big_obj.data_;
    return os;
  }

  // 交换数据，注意不是成员函数
  friend void swap(some_big_object& b1, some_big_object& b2) {
    some_big_object temp = std::move(b1);
    b1 = std::move(b2);
    b2 = std::move(temp);
  } 
 private:
  int data_;
};

// big_object管理者
class big_object_mgr {
public:
  big_object_mgr(int data = 0) : obj_(data) {}
  void printinfo() {
    std::cout << "current obj data is " << obj_ << std::endl;
  }

  friend void danger_swap(big_object_mgr& objm1, big_object_mgr& objm2);
  friend void safe_swap(big_object_mgr& objm1, big_object_mgr& objm2);
  friend void safe_swap_scope(big_object_mgr& objm1, big_object_mgr& objm2);
 
private:
  std::mutex mtx_;
  some_big_object obj_;
};

void danger_swap(big_object_mgr& objm1, big_object_mgr& objm2) {
  std::cout << "thread [" << std::this_thread::get_id() 
    << "] begin" << std::endl;
  if (&objm1 == &objm2) {
    return;
  }

  std::lock_guard<std::mutex> lock1(objm1.mtx_);
  // 为了必现死锁，sleep 1s
  std::this_thread::sleep_for(std::chrono::seconds(1));

  std::lock_guard<std::mutex> lock2(objm2.mtx_);
  swap(objm1.obj_, objm2.obj_);

  std::cout << "thread [" << std::this_thread::get_id() 
    << "] end" << std::endl;
}

void safe_swap(big_object_mgr& objm1, big_object_mgr& objm2) {
  std::cout << "safe_swap thread [" << std::this_thread::get_id() 
    << "] begin" << std::endl;
  if (&objm1 == &objm2) {
    return;
  }

  std::lock(objm1.mtx_, objm2.mtx_);
  // “领养锁”机制管理互斥量解锁
  std::lock_guard<std::mutex> guard1(objm1.mtx_, std::adopt_lock); // std::adopt_lock，那么guard只负责mutex的解锁
  // 和dead_swap()一样，中间sleep 1s
  // std::this_thread::sleep_for(std::chrono::seconds(1));
  std::lock_guard<std::mutex> guard2(objm2.mtx_, std::adopt_lock);

  // C++17支持了scoped_lock
  // std::scoped_lock guard(objm1.mtx_, objm2.mtx_)
  swap(objm1.obj_, objm2.obj_);
  std::cout << "safe_swap thread [" << std::this_thread::get_id() 
    << "] end" << std::endl;
}

void test_danger_swap() {
  big_object_mgr objm1(5);
  big_object_mgr objm2(100);
  std::thread t1(danger_swap, std::ref(objm1), std::ref(objm2)); // 线程1会先对objm1加锁
  std::thread t2(danger_swap, std::ref(objm2), std::ref(objm1)); // 线程2会先对objm2加锁
  t1.join();
  t2.join();

  objm1.printinfo();
  objm2.printinfo();
}

void test_safe_swap() {
  big_object_mgr objm1(5);
  big_object_mgr objm2(100);
  std::thread t1(safe_swap, std::ref(objm1), std::ref(objm2));
  std::thread t2(safe_swap, std::ref(objm2), std::ref(objm1));
  t1.join();
  t2.join();

  objm1.printinfo();
  objm2.printinfo();
}
