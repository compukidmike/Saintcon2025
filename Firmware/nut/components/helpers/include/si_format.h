#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <math.h>

/**
 * @brief  Format a raw value into SI‐prefixed string.
 *
 * @param  value    The raw integer to format (Hz, bytes, whatever)
 * @param  suffix   A short unit suffix (e.g. "Hz", "iB", "B", "%")
 * @param  buf      Output buffer
 * @param  bufsize  Size of output buffer
 */
static inline void si_format(int64_t value, const char *suffix, char *buf, size_t bufsize) {
    // clang-format off
    const struct { int64_t thresh; char prefix; } si[] = {
        {1000000000000000LL, 'P'},
        {1000000000000LL,    'T'},
        {1000000000LL,       'G'},
        {1000000LL,          'M'},
        {1000LL,             'k'},
        {    0,              0  }
    };
    // clang-format on

    int64_t absval = value < 0 ? -value : value;
    for (int i = 0; si[i].thresh > 0; i++) {
        if (absval >= si[i].thresh) {
            double scaled   = value / (double)si[i].thresh;
            double rounded  = round(scaled);
            bool is_integer = fabs(rounded - scaled) < 0.000001;

            if (is_integer) {
                snprintf(buf, bufsize, "%.0f%c%s", scaled, si[i].prefix, suffix);
            } else {
                snprintf(buf, bufsize, "%.1f%c%s", scaled, si[i].prefix, suffix);
            }
            return;
        }
    }
    snprintf(buf, bufsize, "%lld%s", (long long)value, suffix);
}

#ifdef __cplusplus
}
#endif