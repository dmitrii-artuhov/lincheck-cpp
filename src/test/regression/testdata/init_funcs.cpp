#include <iostream>

#include "macro.h"

void ini init_func() { std::cout << "init_func()" << std::endl; }

void ini f() { std::cout << "f()" << std::endl; }

void ini super_init() { std::cout << "super_init()" << std::endl; }

extern "C" void na test() { std::cout << "test_func()" << std::endl; }
