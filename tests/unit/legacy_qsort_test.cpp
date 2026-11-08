#include "pch.h"

#include "doctest/doctest.h"

namespace {
    struct s_entry {
        int key;
        int original_position;
    };

    int Compare(const void* lhs, const void* rhs) {
        return static_cast<const s_entry*>(lhs)->key -
            static_cast<const s_entry*>(rhs)->key;
    }

    int Compare_With_Context(void* context, const void* lhs, const void* rhs) {
        return *static_cast<const int*>(context) * Compare(lhs, rhs);
    }
} // namespace

DOCTEST_TEST_CASE("contextual legacy sorting preserves comparison and tie order") {
    for (size_t count : {size_t{0}, size_t{1}, size_t{8}, size_t{9}, size_t{99}}) {
        std::vector<s_entry> original;
        for (size_t index = 0; index < count; ++index) {
            original.push_back({(int)((index * 17) % 7), (int)index});
        }
        auto contextual = original;
        int direction = 1;
        Legacy_Qsort(original.data(), original.size(), sizeof(s_entry), Compare);
        Legacy_Qsort(contextual.data(), contextual.size(), sizeof(s_entry), Compare_With_Context, &direction);
        for (size_t index = 0; index < count; ++index) {
            DOCTEST_CHECK(contextual[index].key == original[index].key);
            DOCTEST_CHECK(contextual[index].original_position == original[index].original_position);
        }

        direction = -1;
        Legacy_Qsort(contextual.data(), contextual.size(), sizeof(s_entry), Compare_With_Context, &direction);
        for (size_t index = 1; index < count; ++index) {
            DOCTEST_CHECK(contextual[index - 1].key >= contextual[index].key);
        }
    }
}
