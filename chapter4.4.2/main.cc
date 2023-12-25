#include "atm.h"
#include "dispatcher.h"
#include "thread"
#include "user_interface.h"
// 4.2.2节使用CSP模式实现的atm机程序示例

int main() {
  messaging::sender bank;
  user_interface ui;
  atm machine(bank, ui.get_sender());

  std::thread ui_thread(&user_interface::run, &ui);
  std::thread atm_thread(&atm::run, &machine);

  messaging::sender sender_to_atm(machine.get_sender());
  bool quit_pressed = false;
  while (!quit_pressed) {
    char ch = getchar();
    switch (ch) {
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
        sender_to_atm.send(digit_pressed(ch));
        break;
      case 'c':
        sender_to_atm.send(cancel_pressed());
      case 'q':
        quit_pressed = true;
        break;
      case 'i':
        sender_to_atm.send(card_inserted("acc1234"));
        break;
    }
  }

  ui.done();
  machine.done();
  ui_thread.join();
  atm_thread.join();

  return 0;
}