#pragma once

#include <span>
#include <type_traits>

#include "compat.h"

// Keep the original tie order while giving callers a typed comparator.
template<class T, class Compare>
void Legacy_Sort(std::span<T> records, Compare compare) {
    static_assert(std::is_trivially_copyable_v<T>);
    Legacy_Qsort(records.data(), records.size(), sizeof(T),
        [](void* context, const void* lhs, const void* rhs) {
            return (*static_cast<Compare*>(context))(*static_cast<const T*>(lhs), *static_cast<const T*>(rhs));
        }, &compare);
}
