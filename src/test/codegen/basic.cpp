extern "C" {

int myFunc(int a, int b) {
  int c = 0;
  if (a == 3) {
    c = a + b + 1;
    return c;
  } else {
    c = a + b;
  }
  int d = c + 4;
  return d;
}

int main() {
    return 42;
}

}
