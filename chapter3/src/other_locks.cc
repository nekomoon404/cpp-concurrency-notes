#include <iostream>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <thread>

int shared_data_ = 0;
std::mutex mtx;
// 9.1. unique_lock，std::defer_lock表示unique_lock构造时不对mutex上锁
// 随后再调用std::lock()对unique_lock上锁，也可以手动调用unlock()进行解锁，或者等unique_lock离开作用域时自动释放锁
// 相比lock_guard，在上锁的时机上更加灵活，方便控制锁的粒度
// unique_lock内部会维护一个flag，表示该unique_lock的实例是否占有mutex，通过owns_lock()成员函数flag的值
void use_own_lock() {
  std::unique_lock<std::mutex> lock(mtx);
  // 判断是否拥有锁
  if (lock.owns_lock()) {
    std::cout << "Main thread has the lock." << std::endl;
  } else {
    std::cout << "Main thread dose not have the lock." << std::endl;    
  }

  std::thread t([]() {
    std::unique_lock<std::mutex> lock(mtx, std::defer_lock);
    if (lock.owns_lock()) {
      std::cout << "Thread has the lock." << std::endl;
    } else {
      std::cout << "Main thread dose not have the lock." << std::endl;    
    }
    // 加锁
    lock.lock();

    if (lock.owns_lock()) {
      std::cout << "Thread has the lock." << std::endl;
    } else {
      std::cout << "Main thread dose not have the lock." << std::endl;    
    }
    //解锁
    lock.unlock();
  });

  //等待子线程退出
  t.join();
}
// 会deadlock，主线程没有释放锁，子线程在等加锁，主线程在等子线程退出


// 9.2.
// std::mutex是不可拷贝和移动的，但unique_lock是支持移动的，因此可以通过unique_lock来转移mutex的归属权
// 当source是右值时，move是自动完成的；当source是左值时，需要显式调用std::move
std::unique_lock<std::mutex> get_lock() {
  std::unique_lock<std::mutex> lock(mtx);
  shared_data_++;
  return lock;
}
void use_return_lock() {
  std::unique_lock<std::mutex> lock(get_lock());
  shared_data_++;
}


// 9.3.
// 使用unique_lock可以比较灵活地控制锁的粒度，可以自己通过unlock()解锁，不必等到unique_lock离开作用域自动释放
void get_and_process_data() {
  std::unique_lock<std::mutex> lock(mtx);
  shared_data_++;
  lock.unlock();  // 用完shared_data_之后及时解锁

  // 不涉及shared_data_的操作不要放在锁的范围内
  std::this_thread::sleep_for(std::chrono::seconds(1));
  // 需要用shared_data_时再通过lock()加锁
  lock.lock();
  shared_data_++;
}


// 10. 有时对共享数据的访问并不需要同等级别的锁粒度，比如读操作和写操作
// 读时不需要加锁，写时需要加锁，但读的时候不允许写，写的时候不允许读
// 即读和写是互斥的，读是共享的，写是独享的
// C++17标准支持了共享锁shared_mutex，可以方便地解决这种问题
class DNService {
 public:
  DNService() {}
  // 读操作采用共享锁
  std::string QueryDNS(const std::string& dnsname) {
    std::shared_lock<std::shared_mutex> shared_lock(shared_mtx_);
    auto iter = dns_info_.find(dnsname);
    if (iter != dns_info_.end()) {
      return iter->second;
    }
    return "";
  }

  // 写操作采用独占锁
  void AddDNSInfo(const std::string& dnsname, const std::string& dnsentry) {
    std::lock_guard<std::shared_mutex> guard_lock(shared_mtx_);
    dns_info_.insert(std::make_pair(dnsname, dnsentry));
  }

 private:
  std::map<std::string, std::string> dns_info_;
  mutable std::shared_mutex shared_mtx_;
};

// 递归锁 recursive_mutex
// 比如有两个接口，A会调用B，但A和B中都有加锁操作，如果用普通的std::mutex就会造成递归加锁的情况
// 但不推荐使用递归锁，尽量优化设计，避免使用递归锁
class RecursiveDemo {
 public:
  RecursiveDemo() {}
  bool QueryStudent(std::string name) {
    std::lock_guard<std::recursive_mutex> recursive_lock(recursive_mtx_);
    auto iter = students_info_.find(name);
    if (iter != students_info_.end()) {
      return false;
    }
    return true;
  }

  void AddScore(std::string name, int score) {
    std::lock_guard<std::recursive_mutex> recursive_lock(recursive_mtx_);
    if (!QueryStudent(name)) {
      students_info_.insert(std::make_pair(name, score));
      return;
    }
    students_info_[name] = students_info_[name] + score;
    return;
  }

  // 优化设计，避免使用递归锁，将共有逻辑拆分成统一接口
  void AddScoreAtomic(std::string name, int score) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto iter = students_info_.find(name);
    if (iter != students_info_.end()) {
      students_info_.insert(std::make_pair(name, score));
      return;
    }
    students_info_[name] = students_info_[name] + score;
    return;
  }

 private:
  std::map<std::string, int> students_info_;
  std::recursive_mutex recursive_mtx_;
  std::mutex mtx_;
};