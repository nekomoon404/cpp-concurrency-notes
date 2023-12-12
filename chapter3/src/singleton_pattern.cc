#include <iostream>
#include <mutex>
#include <thread>
// 1.C++11及以上的标准，可以利用局部静态变量实现单例
// 在C++11以前存在多线程不安全的情况,存在开辟多个单例的可能
class Single {
 private:
  Single() {}
  Single(const Single&) = delete;
  Single& operator=(const Single&) = delete;

 public:
  static Single& GetInstance() {
    static Single instance;
    return instance;
  }
};

void test_single() {
  std::cout << "s1 addr is " << &Single::GetInstance() << std::endl;
  std::cout << "s2 addr is " << &Single::GetInstance() << std::endl;
}

// C++11以前单例模式的演变:
// 2.饿汉式初始化: 在主线程启动后，在其他线程启动前
// 由主线程先初始化单例资源，其他线程获取资源就不涉及重复初始化了
class Single2Hungry {
 private:
  Single2Hungry() {}
  Single2Hungry(const Single&) = delete;
  Single2Hungry& operator=(const Single2Hungry&) = delete;

 public:
  static Single2Hungry* GetInstance() {
    if (single == nullptr) {
      single = new Single2Hungry();
    }
    return single;
  }

 private:
  static Single2Hungry* single;
};

// 饿汉式单例的使用
Single2Hungry* Single2Hungry::single = Single2Hungry::GetInstance();
void thread_func_s2(int i) {
  std::cout << "this is hungry thread" << i << std::endl;
  std::cout << "instance is " << Single2Hungry::GetInstance() << std::endl;
}

void test_single2hungry() {
  std::cout << "s1 addr is " << &Single::GetInstance() << std::endl;
  std::cout << "s2 addr is " << &Single::GetInstance() << std::endl;
  for (int i = 0; i < 3; ++i) {
    std::thread tid(thread_func_s2, i);
    tid.join();
  }
}
// 但饿汉式要求使用前先初始化,从限制了用户的使用规则,这种写法并不推荐

// 3.懒汉式初始化: 使用时才初始化
class SinglePointer {
 private:
  SinglePointer() {}
  SinglePointer(const SinglePointer&) = delete;
  SinglePointer& operator=(const SinglePointer&) = delete;

 public:
  static SinglePointer* GetInstance() {
    if (single != nullptr) {
      return single;
    }
    s_mutex.lock();
    if (single != nullptr) {
      s_mutex.unlock();
      return single;
    }
    single = new SinglePointer();
    s_mutex.unlock();
    return single;
  }

 private:
  static SinglePointer* single;
  static std::mutex s_mutex;
};

// 懒汉式单例的使用
SinglePointer* SinglePointer::single = nullptr;
std::mutex SinglePointer::s_mutex;
void thread_func_lazy(int i) {
  std::cout << "this is lazy thread" << i << std::endl;
  std::cout << "instance is " << SinglePointer::GetInstance() << std::endl;
}
void test_single_lazy() {
  for (int i = 0; i < 3; ++i) {
    std::thread tid(thread_func_lazy, i);
    tid.join();
  }
}
// 懒汉式的缺点是new出来的资源不知道如何回收, 多个线程调用GetInstance(),
// 并不知道由哪个线程去delete, 可能存在多重释放的问题

// 4.智能指针版的懒汉式初始化, 利用智能指针自动回收资源
// 但仍存在隐患,如果用户手动delete智能指针,就会造成双重释放的问题
class SingleAuto {
 private:
  SingleAuto() {}
  SingleAuto(const SingleAuto&) = delete;
  SingleAuto& operator=(const SingleAuto&) = delete;

 public:
  ~SingleAuto() { std::cout << "single auto delete success" << std::endl; }
  static std::shared_ptr<SingleAuto> GetInstance() {
    if (single != nullptr) {
      return single;
    }
    s_mutex.lock();
    if (single != nullptr) {
      s_mutex.unlock();
      return single;
    }
    single = std::shared_ptr<SingleAuto>(new SingleAuto);
    s_mutex.unlock();
    return single;
  }

 private:
  static std::shared_ptr<SingleAuto> single;
  static std::mutex s_mutex;
};

// 智能指针版单例的调用方式:
std::shared_ptr<SingleAuto> SingleAuto::single = nullptr;
std::mutex SingleAuto::s_mutex;
void test_single_auto() {
  auto sp1 = SingleAuto::GetInstance();
  auto sp2 = SingleAuto::GetInstance();
  std::cout << "sp1 is " << sp1 << std::endl;
  std::cout << "sp2 is " << sp2 << std::endl;
  // 如果手动释放指针，会造成崩溃 free(): double free detected
  // delete sp1.get();
}

// 5.利用辅助类帮助智能指针释放资源，将智能指针的析构设置为私有函数
class SingleAutoSafe {
 private:
  SingleAutoSafe() {}
  ~SingleAutoSafe() {
    std::cout << "this is single auto safe deletor" << std::endl;
  }
  SingleAutoSafe(const SingleAutoSafe&) = delete;
  SingleAutoSafe& operator=(const SingleAutoSafe&) = delete;
  // 声明友元类，使SafeDeletor可以访问私有的析构函数
  friend class SafeDeletor;

 public:
  static std::shared_ptr<SingleAutoSafe> GetInstance();

 private:
  static std::shared_ptr<SingleAutoSafe> single;
  static std::mutex s_mutex;
};
// 自定义的删除器
class SafeDeletor {
 public:
  void operator()(SingleAutoSafe* sf) {
    std::cout << "this is safe deleter operator()" << std::endl;
    delete sf;
  }
};
// 将SafeDeletor定义在SingleAutoSafe后面, 消除编译告警warning:
// possible problem detected in invocation of delete operator

std::shared_ptr<SingleAutoSafe> SingleAutoSafe::single = nullptr;
std::mutex SingleAutoSafe::s_mutex;
std::shared_ptr<SingleAutoSafe> SingleAutoSafe::GetInstance() {
  if (single != nullptr) {
    return single;
  }
  s_mutex.lock();
  if (single != nullptr) {
    s_mutex.unlock();
    return single;
  }
  // 为shared_ptr指定删除器
  single = std::shared_ptr<SingleAutoSafe>(new SingleAutoSafe, SafeDeletor());
  s_mutex.unlock();
  return single;
}
// 调用方式和SingleAuto类一样

// **双重锁检测仍然存在风险, 因为new是一个非原子操作,
// SingleAutoSafe* temp = new SingleAutoSafe()操作可分为三步:
// (1)调用allocate开辟内存
// (2)调用construct执行SingleAutoSafe的构造函数
// (3)调用赋值操作将内存地址赋给temp
// 真实的执行顺序可能是1,3,2,
// 当线程1执行完3后，线程2判断指针非空获取到了实例的指针，调用其成员函数
// 但这时线程1还未执行完毕实例的构造函数，线程2的操作就会导致undefined behavior
// 因此这种双重锁检测的写法也被废弃了

// 6. 使用std:;call_once使用单例类, std::call_once的作用是
// 保证某个函数在多个线程中只被调用一次, 需要配合std::once_flag使用
// std::call_once的开销通常比使用mutex小
// std:;call_once可接受函数指针或者可调用对象作为参数
class SingleOnce {
 private:
  SingleOnce(){};
  SingleOnce(const SingleOnce&) = delete;
  SingleOnce& operator=(const SingleOnce&) = delete;

 public:
  ~SingleOnce() { std::cout << "this is singleton destructor" << std::endl; }
  static std::shared_ptr<SingleOnce> GetInstance() {
    static std::once_flag s_flag;
    std::call_once(s_flag, [&]() {
      instance_ = std::shared_ptr<SingleOnce>(new SingleOnce);
    });
    return instance_;
  }

  void PrintAddress() { std::cout << instance_.get() << std::endl; }

 private:
  static std::shared_ptr<SingleOnce> instance_;
};
// 调用方式:
std::shared_ptr<SingleOnce> SingleOnce::instance_ = nullptr;
void test_single_callonce() {
  std::thread t1([]() {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    SingleOnce::GetInstance()->PrintAddress();
  });
  std::thread t2([]() {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    SingleOnce::GetInstance()->PrintAddress();
  });

  t1.join();
  t2.join();
}

// 7. 使用call_once实现一个单例类模板, 可以通过继承它实现多个单例类
template <typename T>
class Singleton {
 protected:
  Singleton() {}
  Singleton(const Singleton<T>&) = delete;
  Singleton& operator=(const Singleton<T>&) = delete;
  static std::shared_ptr<T> instance_;

 public:
  static std::shared_ptr<T> GetInstance() {
    static std::once_flag s_flag;
    std::call_once(s_flag, [&]() { instance_ = std::shared_ptr<T>(new T); });
    return instance_;
  }

  ~Singleton() { std::cout << "this is singleton destructor" << std::endl; }
  void PrintAddress() { std::cout << instance_.get() << std::endl; }
};

template <typename T>
std::shared_ptr<T> Singleton<T>::instance_ = nullptr;
// 实现自己的单例类时，继承上面的模板类
class MySingle : public Singleton<MySingle> {
  friend class Singleton<MySingle>;

 public:
  ~MySingle() {}

 private:
  MySingle() {}
};