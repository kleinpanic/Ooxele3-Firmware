#include "slownessscout_lib.h"
#include <furi.h>

int32_t slownessscout_mean(const int32_t* samples, size_t n) {
    if(n == 0) return 0;
    int64_t sum = 0;
    for(size_t i = 0; i < n; i++) sum += samples[i];
    return (int32_t)(sum / n);
}

int32_t slownessscout_variance(const int32_t* samples, size_t n) {
    if(n < 2) return 0;
    int32_t m = slownessscout_mean(samples, n);
    int64_t sum_sq_dev = 0;
    for(size_t i = 0; i < n; i++) {
        int64_t dev = samples[i] - m;
        sum_sq_dev += dev * dev;
    }
    return (int32_t)(sum_sq_dev / n);
}

bool slownessscout_should_flag(int32_t sample, int32_t mean, int32_t variance, int32_t threshold) {
    UNUSED(mean);
    UNUSED(variance);
    return (sample > threshold);
}
