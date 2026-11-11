#include "pch.h"

namespace {
    void Legacy_Qsort_Swap(uint8_t* first, uint8_t* second, size_t width) {
        if (first == second) {
            return;
        }

        while (width-- > 0) {
            uint8_t value = *first;
            *first++ = *second;
            *second++ = value;
        }
    }

    void Legacy_Qsort_Short(uint8_t* low, uint8_t* high, size_t width, legacy_qsort_compare compare) {
        while (high > low) {
            uint8_t* maximum = low;
            for (uint8_t* current = low + width; current <= high; current += width) {
                if (compare(current, maximum) > 0) {
                    maximum = current;
                }
            }

            Legacy_Qsort_Swap(maximum, high, width);
            high -= width;
        }
    }
} // namespace

void Legacy_Qsort(void* base, size_t count, size_t width, legacy_qsort_compare compare) {
    if (count < 2 || width == 0) {
        return;
    }

    uint8_t* low = (uint8_t*)base;
    uint8_t* high = low + (count - 1) * width;
    uint8_t* low_stack[30];
    uint8_t* high_stack[30];
    int stack_count = 0;

    while (true) {
        const size_t partition_count = (size_t)(high - low) / width + 1;
        if (partition_count <= 8) {
            Legacy_Qsort_Short(low, high, width, compare);
        } else {
            Legacy_Qsort_Swap(low + (partition_count / 2) * width, low, width);

            uint8_t* low_scan = low;
            uint8_t* high_scan = high + width;
            while (true) {
                do {
                    low_scan += width;
                } while (low_scan <= high && compare(low_scan, low) <= 0);

                do {
                    high_scan -= width;
                } while (high_scan > low && compare(high_scan, low) >= 0);

                if (high_scan < low_scan) {
                    break;
                }

                Legacy_Qsort_Swap(low_scan, high_scan, width);
            }

            Legacy_Qsort_Swap(low, high_scan, width);

            const ptrdiff_t left_span = high_scan - low - 1;
            const ptrdiff_t right_span = high - low_scan;
            if (left_span >= right_span) {
                if (low_scan < high) {
                    low_stack[stack_count] = low_scan;
                    high_stack[stack_count] = high;
                    ++stack_count;
                }

                if (low + width < high_scan) {
                    high = high_scan - width;
                    continue;
                }
            } else {
                if (low + width < high_scan) {
                    low_stack[stack_count] = low;
                    high_stack[stack_count] = high_scan - width;
                    ++stack_count;
                }

                if (low_scan < high) {
                    low = low_scan;
                    continue;
                }
            }
        }

        if (stack_count == 0) {
            return;
        }

        --stack_count;
        low = low_stack[stack_count];
        high = high_stack[stack_count];
    }
}
