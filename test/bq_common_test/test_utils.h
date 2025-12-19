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
                
                for (uint32_t i = 0; i < 128; ++i) {
                    size_t size = static_cast<size_t>(bq::util::rand()) % static_cast<size_t>(128 * 1024) + 9;
                    uint8_t* data_base = static_cast<uint8_t*>(bq::platform::aligned_alloc(8, size));
                    size_t j = 0;
                    for (j = 0; j + 4 < size; j = j + 4) {
                        *reinterpret_cast<uint32_t*>(data_base + j) = bq::util::rand();
                    }
                    for (; j < size; ++j) {
                        data_base[j] = static_cast<uint8_t>(bq::util::rand() % static_cast<uint32_t>(UINT8_MAX));
                    }
                    size_t real_data_size = size - static_cast<size_t>(8);
                    uint32_t base_hash_32 = bq::util::get_hash(data_base, real_data_size);
                    uint64_t base_hash_64 = bq::util::get_hash_64(data_base, real_data_size);
                    uint8_t* data_tmp = data_base;
                    for (size_t offset = 1; offset < 8; ++offset) {
                        ++data_tmp;
                        memmove(data_tmp, data_tmp - 1, real_data_size);
                        uint32_t base_hash_32_tmp = bq::util::get_hash(data_tmp, real_data_size);
                        uint64_t base_hash_64_tmp = bq::util::get_hash_64(data_tmp, real_data_size);
                        result.add_result(base_hash_32 == base_hash_32_tmp, "hash32 test at offset %" PRId32, static_cast<int32_t>(offset));
                        result.add_result(base_hash_64 == base_hash_64_tmp, "hash64 test at offset %" PRId32, static_cast<int32_t>(offset));
                    }
                    bq::platform::aligned_free(data_base);
                }
                
                return result;
            }
        };
    }
}
