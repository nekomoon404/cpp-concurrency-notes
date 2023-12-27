#pragma once
#include <functional>
#include <iostream>
#include <mutex>

#include "receiver.h"
// UI线程没有状态(或者说只有一种状态，就是等待消息)，只负责处理消息
class UserInterface {
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
                "dispaly_enter_pin")
            .handle<
                display_pin_incorrect_message,
                std::function<void(display_pin_incorrect_message const& msg)>>(
                [&](display_pin_incorrect_message const& msg) {
                  {
                    std::lock_guard<std::mutex> lock(ui_mtx_);
                    std::cout << "Pin is incorrect." << std::endl;
                  }
                },
                "display_pin_incorrect_message")
            .handle<display_withdrawal_options,
                    std::function<void(display_withdrawal_options const& msg)>>(
                [&](display_withdrawal_options const& msg) {
                  {
                    std::lock_guard<std::mutex> lk(ui_mtx_);
                    std::cout << "Withdraw 50? (w)" << std::endl;
                    std::cout << "Cancel? (c)" << std::endl;
                  }
                },
                "display_withdrawal_options")
            .handle<issue_money, std::function<void(issue_money const& msg)>>(
                [&](issue_money const& msg) {
                  {
                    std::lock_guard<std::mutex> lock(ui_mtx_);
                    std::cout << "Issuing $" << msg.amount_ << std::endl;
                  }
                },
                "issue_money")
            .handle<display_insufficient_funds,
                    std::function<void(display_insufficient_funds const& msg)>>(
                [&](display_insufficient_funds const& msg) {
                  {
                    std::lock_guard<std::mutex> lock(ui_mtx_);
                    std::cout << "Insufficient funds." << std::endl;
                  }
                },
                "display_insufficient_funds")
            .handle<display_withdrawal_canceled,
                    std::function<void(display_withdrawal_canceled const& msg)>>(
                [&](display_withdrawal_canceled const& msg) {
                  {
                    std::lock_guard<std::mutex> lock(ui_mtx_);
                    std::cout << "Withdrawal canceled." << std::endl;
                  }
                },
                "display_withdrawal_canceled")
            .handle<eject_card, std::function<void(eject_card const& msg)>>(
                [&](eject_card const& msg) {
                  {
                    std::lock_guard<std::mutex> lk(ui_mtx_);
                    std::cout << "Ejecting card" << std::endl;
                  }
                },
                "eject_card");
      }
    } catch (messaging::close_queue&) {
    }
  }
};