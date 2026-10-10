// Must fail: a plain bq::log owns no categories, so passing one is rejected.
#include "bq_log/bq_log.h"
#include "other_category_log.h"

bool case_plain_log_category(const bq::log& l, const bq::first_category_log& first)
{
    return l.fast_info<"plain {}">(first.cat.one, int32_t(1));
}
