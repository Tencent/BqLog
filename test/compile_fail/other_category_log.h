#pragma once
// Two minimal generated-style category logs, used by the compile-fail cases to show that a
// category belonging to one log class is rejected by another. They mirror the shape the
// generator emits, without pulling in its full template.
#include "bq_log/bq_log.h"

namespace bq {
#define BQ_COMPILE_FAIL_CATEGORY_LOG(CLASS_NAME)                                                        \
    class CLASS_NAME : public bq::category_log {                                                        \
    private:                                                                                            \
        template <uint32_t CAT_INDEX>                                                                   \
        struct CLASS_NAME##_category_base : public bq::log_category_base<CAT_INDEX> {                   \
        };                                                                                              \
        struct CLASS_NAME##_category_root {                                                             \
            CLASS_NAME##_category_base<1> one;                                                          \
            CLASS_NAME##_category_base<2> two;                                                          \
        };                                                                                              \
                                                                                                        \
    public:                                                                                             \
        const CLASS_NAME##_category_root cat { };                                                       \
        using bq::log::fast_info;                                                                       \
        template <bq::fixed_format F, uint32_t CAT_INDEX, typename... Args>                             \
        bool fast_info(const CLASS_NAME##_category_base<CAT_INDEX>&, const Args&... args) const         \
        {                                                                                               \
            return bq::fast_info_detail::fast_impl<F, bq::log_level::info, CAT_INDEX>(*this, args...);  \
        }                                                                                               \
    };

    BQ_COMPILE_FAIL_CATEGORY_LOG(first_category_log)
    BQ_COMPILE_FAIL_CATEGORY_LOG(second_category_log)
#undef BQ_COMPILE_FAIL_CATEGORY_LOG
}
