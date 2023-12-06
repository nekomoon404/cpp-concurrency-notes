#include <iostream>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <string>
// unique_lock

// unique_lock有移动构造，可以作为局部变量返回

// 锁粒度表示加锁的精细程度

// C++17标准支持了共享锁shared_mutex
// 比如读时不加锁，写时才加锁，但读的时候不允许写，写的时候不允许读
// 即读和写是互斥的，读是共享的，写是独享的
class DNService {
 public:
  DNService() {}
  // 读操作采用共享锁
  std::string QueryDNS(const std::string& dnsname) {
    std::shared_lock<std::shared_mutex>(shared_mutex_);
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