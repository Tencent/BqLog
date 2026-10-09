#include "bq_log/bq_log.h"

extern bq::log g_log;
bool probe_macro_global(int32_t x, float y)
{
    BQ_LOG_FAST_INFO(g_log, "a {} {}", x, y);
    return true;
}
bool probe_macro_ref(const bq::log& l, int32_t x, float y)
{
    BQ_LOG_FAST_INFO(l, "a {} {}", x, y);
    return true;
}
#if defined(BQ_CPP_20)
// T08 adds:
// bool probe_fast_global(int32_t x, float y) { return g_log.fast_info<"a {} {}">(x, y); }
// bool probe_fast_ref(const bq::log& l, int32_t x, float y) { return l.fast_info<"a {} {}">(x, y); }
#endif
