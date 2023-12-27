#pragma once

#include <functional>
#include <iostream>

#include "receiver.h"
class Atm {
 private:
  messaging::receiver incoming_;
  messaging::sender sender_to_ui_;
  messaging::sender sender_to_bank_;
  void (Atm::*state)();  // 函数指针
  unsigned int withdrawal_amount_;
  unsigned int const pin_length_ = 6;
  std::string account_;
  std::string pin_;

 private:
  Atm(Atm const&) = delete;
  Atm& operator=(Atm const&) = delete;
  // 状态函数
  // 等待用户插卡
  void waiting_for_card() {
    sender_to_ui_.send(display_enter_card());
    incoming_.wait()
        .handle<card_inserted, std::function<void(card_inserted const& msg)>>(
            [&](card_inserted const& msg) {
              account_ = msg.account_;
              pin_ = "";
              sender_to_ui_.send(display_enter_pin());
              state = &Atm::getting_pin;
            },
            "card_inserted");
  }
  // 等待用户输入密码
  void getting_pin() {
    incoming_.wait()
        .handle<digit_pressed, std::function<void(digit_pressed const& msg)>>(
            [&](digit_pressed const& msg) {
              pin_ += msg.digit_;
              if (pin_.length() == pin_length_) {
                std::cout << "pin is " << pin_ << std::endl;
                sender_to_bank_.send(verify_pin(account_, pin_, incoming_));
                state = &Atm::wait_for_verifying_pin;
              }
            },
            "digit_pressed");
  }
  // 等待Bank校验密码
  void wait_for_verifying_pin() {
    incoming_.wait()
        .handle<pin_verified, std::function<void(pin_verified const& msg)>>(
            [&](pin_verified const&) {
              std::cout << "pin is correct." << std::endl;
              state = &Atm::wait_for_user_withdraw;
            },
            "pin_verfied")
        .handle<pin_incorrect, std::function<void(pin_incorrect const& msg)>>(
            [&](pin_incorrect const&) {
              sender_to_ui_.send(display_pin_incorrect_message());
              state = &Atm::done_processing;
            },
            "pin_incorrect");
  }

  // 等待用户取钱
  void wait_for_user_withdraw() {
    sender_to_ui_.send(display_withdrawal_options());
    incoming_.wait()
        .handle<withdraw_pressed,
                std::function<void(withdraw_pressed const& msg)>>(
            [&](withdraw_pressed const& msg) {
              withdrawal_amount_ = msg.amount_;
              sender_to_bank_.send(
                  request_withdraw(account_, msg.amount_, incoming_));
              state = &Atm::wait_for_process_withdraw;
            },
            "withdraw_pressed")
        .handle<cancel_pressed, std::function<void(cancel_pressed const& msg)>>(
            [&](cancel_pressed const& msg) {
              sender_to_ui_.send(display_withdrawal_canceled());
              state = &Atm::done_processing;
            },
            "cancel_pressed");
  }

  // 等待Bank处理取钱过程
  void wait_for_process_withdraw() {
    incoming_.wait()
        .handle<withdraw_success,
                std::function<void(withdraw_success const& msg)>>(
            [&](withdraw_success const& msg) {
              sender_to_ui_.send(issue_money(withdrawal_amount_));
              sender_to_bank_.send(
                  complete_withdraw(account_, withdrawal_amount_));
              // 这里取钱成功让用户可以继续操作
              state = &Atm::wait_for_user_withdraw;
              // state = &Atm::done_processing;
            },
            "withdraw_ok")
        .handle<withdraw_denied,
                std::function<void(withdraw_denied const& msg)>>(
            [&](withdraw_denied const& msg) {
              sender_to_ui_.send(display_insufficient_funds());
              state = &Atm::done_processing;
            },
            "withdraw_denied");
  }

  // Atm结束流程
  void done_processing() {
    sender_to_ui_.send(eject_card());
    state = &Atm::waiting_for_card;
  }

 public:
  Atm(messaging::sender sender_of_bank, messaging::sender sender_of_ui)
      : sender_to_bank_(sender_of_bank), sender_to_ui_(sender_of_ui) {}
  messaging::sender get_sender() { return incoming_; }
  void done() { get_sender().send(messaging::close_queue()); }
  void run() {
    state = &Atm::waiting_for_card;
    try {
      for (;;) {
        (this->*state)();
      }
    } catch (messaging::close_queue const&) {
    }
  }
};