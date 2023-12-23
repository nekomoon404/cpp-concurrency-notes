#ifndef csp_sample_h_
#define csp_sample_h_
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>
#include <iostream>
// csp模式: communicating sequential process 是一种并发编程模式
// csp将消息投递给channel, 不关注谁从channel中取数据; 好处是取消共享状态, 解耦合
// 对比actor模式, 它的消息发送方和接收方是知道彼此的

// 下面是用C++实现类似于Go语言中的channel的示例
template <typename T>
class Channel {
 private:
  std::queue<T> queue_;
  std::mutex mtx_;
  std::condition_variable cv_producer_;
  std::condition_variable cv_consumer_;
  size_t capacity_;  // 缓冲区大小
  bool closed_ = false;

 public:
  Channel(size_t capacity = 0) : capacity_(capacity) {}
  bool send(T value) {
    std::unique_lock<std::mutex> lock(mtx_);
    cv_producer_.wait(lock, [this]() {
      // 对于缓冲区=0的channel, 要等消费者把队列取空才能再添加新的数据
      return (capacity_ == 0 && queue_.empty()) || queue_.size() < capacity_ ||
             closed_;
    });

    if (closed_) {
      return false;
    }
    queue_.push(value);
    cv_consumer_.notify_one();
    return true;
  }

  bool receive(T& value) {
    std::unique_lock<std::mutex> lock(mtx_);
    cv_consumer_.wait(lock, [this](){
      return !queue_.empty() || closed_;
    });
    // channel关闭时队列中可能还有没消费的数据, 要把它们处理完
    if (closed_ && queue_.empty()) {
      return false;
    }
    value = queue_.front();
    queue_.pop();
    cv_producer_.notify_one();
    return true;
  }

  void close() {
    std::unique_lock<std::mutex> lock(mtx_);
    closed_ = true;
    cv_producer_.notify_all();
    cv_consumer_.notify_all();
  }
};

void use_csp_sample() {
  Channel<int> chan(5); // 缓冲区大小为10的channel
  std::thread producer([&](){
    for (int i =0; i < 10; ++i) {
      chan.send(i);
      std::cout << "Send: " << i << std::endl;
    }
    chan.close();
  });
  std::thread consumer([&](){
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    int value;
    while(chan.receive(value)) {
      std::cout << "Received: " << value << std::endl;
    }
  });

  producer.join();
  consumer.join();
}

#endif  // csp_sample_h_