#pragma once

#include <chrono>

struct Timer {
    void Start() {
        start = std::chrono::high_resolution_clock::now();
    }

    int64_t Stop() {
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    }
private:
    std::chrono::_V2::system_clock::time_point start;
};