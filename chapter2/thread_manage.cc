#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <numeric>

// 1. std::thread没有拷贝构造和拷贝赋值，只能通过移动和局部变量返回的方式
// 将thread对象管理的线程转移给其他对象管理
void some_function() {
  while (true) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
}

void some_other_function() {
  while (true) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
}

void transfer_oops() {
  std::thread t1(some_function);
  // 转移t1管理的线程给t2，转移后t1无效
  std::thread t2 = std::move(t1);
  // t1可再绑定线程, moving from 临时对象是自动和隐式的
  t1 = std::thread(some_other_function);

  std::thread t3;
  // 转移t2管理的线程给t3
  t3 = std::move(t2);

  // 转移t3管理的线程给t1，这里会crash
  // t1 = std::move(t3);
  // t1已经有关联的线程了，再将t3管理的线程转移给t1，
  // 会触发std::terminate()终止程序(在std::thread的析构函数内调用)
  // 因此不能通过向管理线程的std::thread对象 转移新线程 的方式来drop它当前管理的线程

  // 排除是因为主线程退出导致的crash
  // std::this_thread::sleep_for(std::chrono::seconds(2000));
}

// 2.在函数内返回一个的std::thread变量，C++返回局部变量时
// 优先寻找类的拷贝构造函数，如果没有就寻找类的移动构造函数
std::thread f() { return std::thread(some_function); }

void some_other_function2(int param) {
  std::cout << "param is " << param << std::endl;
  while (true) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
}

std::thread g() {
  std::thread t(some_other_function2, 42);
  return t;
}

// std::thread也可以作为参数传入函数
void fin(std::thread t) {}
void gin() {
  fin(std::thread(some_function));  // moving from 临时对象是自动和隐式的
  std::thread t(some_function);
  fin(std::move(t));  // 需要显式调用move
}

// 3. 利用std::thread的move性质，实现掌握线程归属权的thread_guard类
// 保证thread_guard对象离开作用域时，线程已经执行完成
class scoped_thread {
  std::thread t_;

 public:
  explicit scoped_thread(std::thread t) : t_(std::move(t)) {
    if (!t_.joinable()) {
      throw std::logic_error("No thread");
    }
  }

  ~scoped_thread() {
    t_.join();  // 对比thread_guard，scoped_thread在构造函数里判断thread是否joinable
  }

  scoped_thread(scoped_thread const&) = delete;
  scoped_thread& operator=(scoped_thread const&) = delete;
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

void scoped_oops() {
  int some_local_state = 123;
  scoped_thread t{std::thread(func(some_local_state))};
  // do_something_in_current_thread
}

// 4.用容器存储std::thread，if those containers are move-aware
// 比如vector的push_back()操作会调用std::thread的拷贝构造函数，但std::thread没有拷贝构造
// 使用emplace_back()则可以直接根据std::thread构造函数需要的参数来构造一个thread对象，避免调用拷贝构造函数
void param_function(int a) {
  std::cout << "param is " << a << std::endl;
  std::this_thread::sleep_for(std::chrono::seconds(1));
}

void vector_oops() {
  std::vector<std::thread> threads;
  for (size_t i = 0; i < 10; ++i) {
    threads.emplace_back(param_function, i);
  }
  for (auto& entry : threads) {
    entry.join();
  }
}

// 5. 选择运行时线程的数量，std::thread::hardware_concurrency()
// 返回值表示程序执行时的can truly run concurrently的线程数，一般等于CPU的核心数
// 下面的例子是并行版本的std::accumulate
template<typename Iterator, typename T>
struct accumulate_block {
  void operator()(Iterator first, Iterator last, T& result) {
    result = std::accumulate(first, last, result);
  }
};
template<typename Iterator, typename T>
T parallel_accumulate(Iterator first, Iterator last, T init) {
  unsigned long const length = std::distance(first, last);
  if (length == 0) {
    return init;
  }
  unsigned long const min_per_thread = 25;
  unsigned long const max_threads = 
    (length + min_per_thread - 1) / min_per_thread;
  unsigned long const hardware_threads = 
    std::thread::hardware_concurrency();
  unsigned long const num_threads = 
    std::min(hardware_threads != 0 ? hardware_threads : 2, max_threads);

  unsigned long const block_size = length / num_threads;
  std::vector<T> results(num_threads);
  // 主线程也会算一个block(最好一个block)，因此只需新创建Num-1个线程
  std::vector<std::thread> threads(num_threads - 1);

  Iterator block_start = first;
  for (unsigned long i = 0; i < (num_threads - 1); ++i) {
    Iterator block_end = block_start;
    std::advance(block_end, block_size);
    threads[i] = std::thread(
      accumulate_block<Iterator, T>(),
      block_start, block_end, std::ref(results[i])
    );
    block_start = block_end;
  }

  accumulate_block<Iterator, T>()(
    block_start, last, results[num_threads - 1]);

  for(auto& entry: threads) {
    entry.join();
  }
  return std::accumulate(results.begin(), results.end(), init);
}
// 并行版本的accumulate 要和普通版本的accumulate 计算结果一致，
// 要求数据类型T是associative的，即满足结合律(a+b)+c=a+(b+c)，比如float和double就不满足

void use_parallel_accu() {
  std::vector<int> vec(10000);
  for (int i = 0; i < 10000; ++i) {
    vec.push_back(i);
  }
  int sum = 0;
  sum = parallel_accumulate<std::vector<int>::iterator, int>(
    vec.begin(), vec.end(), 0);
  std::cout << "sum is " << sum << std::endl;
}


// 6. 线程标识 std::thread::id
// 两种方式：std::thread对象调用get_id()成员函数；
//         std::this_thread::get_id()获取当前线程的id
// 如果std::thread没有绑定线程，则get_id()返回一个默认值，表示"not any thread"
// 如果两个std::thread::id相等，则表示是相同的线程，或者都是"not any thread"值
// std::thread::id 可以排序、比较、用做关联容器的key
void identify_oops() {
  std::thread t1([](){
    std::cout << "thread t1 start" << std::endl;
  });
  std::cout << "thread t1 id = " << t1.get_id() << std::endl;
  t1.join();

  std::thread t2([](){
    std::cout << "in thread id " <<
    std::this_thread::get_id() << std::endl;
    std::cout << "thread start" << std::endl;
  });
  std::cout << "thread t2 id = " << t2.get_id() << std::endl;
  t2.join();
}

int main() {
  // 线程归属权示例
  // transfer_oops();

  scoped_oops();

  // vector_oops();

  // identify_oops();

  use_parallel_accu();

  return 0;
}