extern "C" {

int x;

struct Adder {
  void add() {
    int y = x;
    y++;
    x = y;
  }

  int get() { return x; }
};

int main() {
    Adder a{};
    a.add();
    return a.get();
}

}