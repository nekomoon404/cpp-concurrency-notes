#pragma once
#include <functional>
#include <iostream>
#include <mutex>

#include "receiver.h"
// UI线程没有状态(或者说只有一种状态，就是等待消息)，只负责处理消息
class user_interface {
 private:
  std::mutex ui_mtx_;
  messaging::receiver incoming;

 public:
  messaging::sender get_sender() { return incoming; }  // 隐式转换
  void done() { get_sender().send(messaging::close_queue()); }

  void run() {
    try {
      for (;;) {
        incoming.wait()
            .handle<display_enter_card,
                    std::function<void(display_enter_card const& msg)>>(
                [&](display_enter_card const& msg) {
                  {
                    std::lock_guard<std::mutex> lock(ui_mtx_);
                    std::cout << "Please enter you card (I)" << std::endl;
                  }
                },
                "dispaly_enter_card")
            .handle<display_enter_pin,
                    std::function<void(display_enter_pin const& msg)>>(
                [&](display_enter_pin const& msg) {
                  {
                    std::lock_guard<std::mutex> lock(ui_mtx_);
                    std::cout << "Please enter you pin (0~9)" << std::endl;
                  }
                },
                "dispaly_enter_pin");
      }
    } catch (messaging::close_queue&) {
    }
  }
};