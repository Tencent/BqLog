#pragma once
#include "test_base.h"
#include "bq_common/bq_common.h"

namespace bq {
    namespace test {
        class test_utils : public test_base {
        public:
            virtual test_result test() override
            {
                test_result result;
                result.add_result(bq::roundup_pow_of_two(3) == 4, "roundup_pow_of_tow, 3");
                result.add_result(bq::roundup_pow_of_two(31) == 32, "roundup_pow_of_tow, 31");
                result.add_result(bq::roundup_pow_of_two(32) == 32, "roundup_pow_of_tow, 32");
                result.add_result(bq::roundup_pow_of_two(1020) == 1024, "roundup_pow_of_tow, 1020");
                result.add_result(bq::roundup_pow_of_two(0xFFFFFFFF) == 0x00, "roundup_pow_of_tow, max uint32");
                return result;
            }
        };
    }
}
