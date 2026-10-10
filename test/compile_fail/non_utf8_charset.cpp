// Must fail under a non-UTF-8 execution charset (e.g. -fexec-charset=GBK): a plain literal with
// non-ASCII bytes cannot be used unless the execution charset really is UTF-8. The format string
// below is "café {}" written with a universal character name so this file stays ASCII; the
// compiler converts U+00E9 to the execution charset, which is not UTF-8 under GBK.
#include "bq_log/bq_log.h"

bool case_non_utf8_charset(const bq::log& l)
{
    return l.fast_info<"café {}">(int32_t(1));
}
