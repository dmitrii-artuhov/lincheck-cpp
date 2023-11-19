#include <numeric>
#include <iostream>

int main() {
    std::gcd(12, 13);
    std::cout << "hello from main" << std::endl;
    std::gcd(13, 14);
    std::cout << "hello from main 2" << std::endl;
    return 0;
}
