#pragma once
#include <functional>
#include <iostream>
#include "receiver.h"
class Bank {
 private:
  messaging::receiver incoming_;
  unsigned int balance_;

 public:
  Bank() : balance_(99) {}
  Bank(unsigned int balance) : balance_(balance) {}
  messaging::sender get_sender() { return incoming_; }
  void done() { get_sender().send(messaging::close_queue()); }

  void run() {
    try {
      for (;;) {
        incoming_.wait()
            .handle<verify_pin, std::function<void(verify_pin const& msg)>>(
                [&](verify_pin const& msg) {
                  if (msg.pin_ == "123456") {
                    msg.sender_to_atm_.send(pin_verified());
                  } else {
                    msg.sender_to_atm_.send(pin_incorrect());
                  }
                },
                "verify_pin")
            .handle<request_withdraw,
                    std::function<void(request_withdraw const& msg)>>(
                [&, this](request_withdraw const& msg) {
                  if (this->balance_ >= msg.amount_) {
                    msg.sender_to_atm_.send(withdraw_success());
                    this->balance_ -= msg.amount_;
                  } else {
                    msg.sender_to_atm_.send(withdraw_denied());
                  }
                },
                "request_withdraw")
            .handle<complete_withdraw,
                    std::function<void(complete_withdraw const& msg)>>(
                [&](complete_withdraw const& msg) {
                  std::cout << "withdraw completed." << std::endl;
                },
                "complete_withdraw");
      }
    } catch (messaging::close_queue const&) {
    }
  }
};