extern "C" {

int bar(int c) {
  c++;
  return c;
}

int mini(int a) {
  if (a == 5) {
    return a;
  }
  a++;
  return mini(a);
}

int foo(int a, int b) {
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

int main() { return 42; }
}
