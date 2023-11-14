#include "lib.h"

// Какой-то тестовый класс.
struct Adder {
  int val;

  // [atomic] -- можно добавить какие-нибудь аннотации для кодгена.
  // Например, atomic - считать функцию атомарной при выполнении линчека.
  void add() {
    int x = get();
    x++;
    val = x;
  }
  int get() { return val; }
};

// Environment.
// Перед каждым исполнением инициализируется заново.
struct Env {
  Adder ad;
  Env() { ad = Adder{0}; }
};

// Тестируемая функция.
int func(Env &e) {
  e.ad.add();
  return e.ad.get();
}

// Исполняем func в 2 потоках и выводим результаты.
EXECUTE(func, 2);
