// Must fail: a UTF-16 format literal is not a supported fast_info format.
#include "bq_log/bq_log.h"

bool case_width16(const bq::log& l)
{
    return l.fast_info<u"wide {}">(int32_t(1));
}
