#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// @complexity O(N)
// Compute mean of sample array
int32_t slownessscout_mean(const int32_t* samples, size_t n);

// @complexity O(N)
// Compute variance of sample array
int32_t slownessscout_variance(const int32_t* samples, size_t n);

// @complexity O(1)
// Determine if sample exceeds threshold or statistical outlier
bool slownessscout_should_flag(int32_t sample, int32_t mean, int32_t variance, int32_t threshold);
