#pragma once

#include <functional>

#include "receiver.h"
#include <iostream>
class atm {
 private:
  messaging::receiver incoming_;
  messaging::sender sender_to_ui_;
  messaging::sender sender_to_bank_;
  void (atm::*state)();  // 函数指针
  unsigned int withdrawal_amount_;
  unsigned int const pin_length_ = 4;
  std::string account_;
  std::string pin_;

 private:
  // 状态函数
  void waiting_for_card() {
    sender_to_ui_.send(display_enter_card());
    incoming_.wait()
        .handle<card_inserted, std::function<void(card_inserted const& msg)>>(
            [&](card_inserted const& msg) {
              account_ = msg.account_;
              pin_ = "";
              sender_to_ui_.send(display_enter_pin());
              state = &atm::getting_pin;
            },
            "card_inserted");
  }

  void getting_pin() {
    incoming_.wait().handle<digit_pressed, std::function<void(digit_pressed const& msg)>>(
      [&](digit_pressed const& msg){
        pin_ += msg.digit_;
        if (pin_.length() == pin_length_) {
          std::cout << "pin is " << pin_ << std::endl;
          // sender_to_bank_.send(verify_pin(account_, pin_, incoming_));
          // state = &atm::verify_pin;
        }
      }, "digit_pressed"
    );
  }

 public:
  atm(messaging::sender sender_of_bank, messaging::sender sender_of_ui)
      : sender_to_bank_(sender_of_bank), sender_to_ui_(sender_of_ui) {}
  messaging::sender get_sender() { return incoming_; }
  void done() { get_sender().send(messaging::close_queue()); }
  void run() {
    state = &atm::waiting_for_card;
    try {
      for (;;) {
        (this->*state)();
      }
    } catch (messaging::close_queue const&) {}
  }
};