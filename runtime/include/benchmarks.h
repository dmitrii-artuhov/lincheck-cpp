#pragma once

#include <cstdint>

struct BenchmarkData {
    int total_tasks;

    int sequential_minimization_result;
    int64_t sequential_minimization_ms;

    int exploration_minimization_result;
    int64_t exploration_minimization_ms;

    int greedy_minimization_result;
    int64_t greedy_minimization_ms;

    int double_greedy_minimization_result;
    int64_t double_greedy_minimization_ms;

    int smart_minimization_result;
    int64_t smart_minimization_ms;

    int combined_3_minimization_result;
    int64_t combined_3_minimization_ms;
};