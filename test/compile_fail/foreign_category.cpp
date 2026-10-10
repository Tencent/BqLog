// Must fail: a category of another generated log class is rejected by this one.
#include "bq_log/bq_log.h"
#include "other_category_log.h"

bool case_foreign_category(const bq::first_category_log& first, const bq::second_category_log& second)
{
    return first.fast_info<"foreign {}">(second.cat.one, int32_t(1));
}
