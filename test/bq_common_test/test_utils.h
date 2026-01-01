#pragma once
#include "test_base.h"
#include "bq_common/bq_common.h"
#include <chrono>

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

                // =================================================================================
                // Test for bq::util::get_hash and bq::util::get_hash_64
                // =================================================================================
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

                // =================================================================================
                // Test for bq_memcpy_with_hash and bq_hash_only
                // =================================================================================
                for (uint32_t i = 0; i < 64; ++i) {
                    size_t size = static_cast<size_t>(bq::util::rand()) % 4064 + 32;
                    
                    uint8_t* src_base = static_cast<uint8_t*>(bq::platform::aligned_alloc(16, size + 32));
                    uint8_t* dst_base = static_cast<uint8_t*>(bq::platform::aligned_alloc(16, size + 32));
                    uint8_t* ref_data = static_cast<uint8_t*>(bq::platform::aligned_alloc(16, size));
                    
                    for (size_t k = 0; k < size; ++k) {
                        ref_data[k] = static_cast<uint8_t>(bq::util::rand() & 0xFF);
                    }

                    uint64_t expected_hash = bq::util::bq_hash_only(ref_data, size);

                    for (size_t src_off = 0; src_off < 16; ++src_off) {
                        for (size_t dst_off = 0; dst_off < 16; dst_off += 4) { // Iterate mostly 4-byte aligned dst
                            
                            uint8_t* s = src_base + src_off;
                            uint8_t* d = dst_base + dst_off;
                            
                            memcpy(s, ref_data, size);

                            memset(d, 0xCC, size);

                            uint64_t hash_copy = bq::util::bq_memcpy_with_hash(d, s, size);

                            int32_t cmp_res = memcmp(ref_data, d, size);
                            result.add_result(cmp_res == 0, "bq_memcpy_with_hash copy correctness size=%zu src_off=%zu dst_off=%zu", size, src_off, dst_off);

                            uint64_t hash_only = bq::util::bq_hash_only(s, size);

                            result.add_result(hash_copy == hash_only, "bq_memcpy_with_hash vs bq_hash_only match size=%zu src_off=%zu dst_off=%zu", size, src_off, dst_off);
                            result.add_result(hash_copy == expected_hash, "Hash alignment stability check size=%zu src_off=%zu dst_off=%zu", size, src_off, dst_off);
                        }
                    }

                    bq::platform::aligned_free(src_base);
                    bq::platform::aligned_free(dst_base);
                    bq::platform::aligned_free(ref_data);
                }

                // =================================================================================
                // Comprehensive Small Size Correctness Test (1-128 bytes)
                // =================================================================================
                {
                    const size_t max_test_len = 128;
                    // Allocate enough space with guards
                    uint8_t* src = static_cast<uint8_t*>(bq::platform::aligned_alloc(64, max_test_len * 2));
                    uint8_t* dst = static_cast<uint8_t*>(bq::platform::aligned_alloc(64, max_test_len * 2));
                    uint8_t* ref_dst = static_cast<uint8_t*>(bq::platform::aligned_alloc(64, max_test_len * 2));

                    // Fill src with random pattern
                    for (size_t i = 0; i < max_test_len * 2; ++i) {
                        src[i] = (uint8_t)(i * 13 + 7); // Pseudo-random pattern
                    }

                    for (size_t len = 1; len <= max_test_len; ++len) {
                        // Prepare destinations with a guard pattern (0xAA)
                        memset(dst, 0xAA, max_test_len * 2);
                        memset(ref_dst, 0xAA, max_test_len * 2);

                        // 1. Calculate Expected Hash using Hash Only
                        uint64_t hash_only = bq::util::bq_hash_only(src, len);

                        // 2. Run Copy+Hash
                        uint64_t hash_copy = bq::util::bq_memcpy_with_hash(dst, src, len);

                        // 3. Verify Hash Consistency
                        result.add_result(hash_only == hash_copy, "Hash consistency len=%zu", len);

                        // 4. Verify Content Correctness (using memcmp)
                        // Use standard memcpy for reference
                        memcpy(ref_dst, src, len);
                        int32_t content_match = memcmp(dst, ref_dst, len);
                        result.add_result(content_match == 0, "Content match len=%zu", len);

                        // 5. Verify Guard Bytes (No Overflow)
                        // Check the byte immediately after len
                        result.add_result(dst[len] == 0xAA, "Guard byte check len=%zu", len);
                    }

                    bq::platform::aligned_free(src);
                    bq::platform::aligned_free(dst);
                    bq::platform::aligned_free(ref_dst);
                }

                bq::util::set_log_device_console_min_level(bq::log_level::debug);
                // =================================================================================
                // Performance Benchmark
                // =================================================================================
                {
                    // Benchmark settings
                    const size_t bench_buf_size = static_cast<size_t>(64 * 1024 * 1024); // 64 MB
                    const size_t total_data_to_process = static_cast<size_t>(0xffffffff); // 4 GB
                    const size_t iterations = total_data_to_process / bench_buf_size;

                    uint8_t* src = static_cast<uint8_t*>(bq::platform::aligned_alloc(64, bench_buf_size));
                    uint8_t* dst = static_cast<uint8_t*>(bq::platform::aligned_alloc(64, bench_buf_size));

                    // Fill source with random data
                    for (size_t i = 0; i < bench_buf_size; ++i) {
                        src[i] = (uint8_t)(i & 0xFF);
                    }
                    // Warmup
                    bq::util::bq_memcpy_with_hash(dst, src, bench_buf_size);

                    auto get_throughput_gbps = [&](double seconds) {
                        return (double)total_data_to_process / (1024.0 * 1024.0 * 1024.0) / seconds;
                        };

                    // 1. Benchmark: System memcpy
                    auto start = std::chrono::high_resolution_clock::now();
                    for (size_t i = 0; i < iterations; ++i) {
                        memcpy(dst, src, bench_buf_size);
                    }
                    auto end = std::chrono::high_resolution_clock::now();
                    std::chrono::duration<double> diff = end - start;
                    bq::util::log_device_console(bq::log_level::debug, "Benchmark - memcpy: %.3f GB/s", get_throughput_gbps(diff.count()));

                    // 2. Benchmark: Hash Only
                    uint64_t dummy_hash = 0;
                    start = std::chrono::high_resolution_clock::now();
                    for (size_t i = 0; i < iterations; ++i) {
                        dummy_hash += bq::util::bq_hash_only(src, bench_buf_size);
                    }
                    end = std::chrono::high_resolution_clock::now();
                    diff = end - start;
                    bq::util::log_device_console(bq::log_level::debug, "Benchmark - Hash Only: %.3f GB/s (Sum: %" PRIu64 ")", get_throughput_gbps(diff.count()), dummy_hash);

                    // 3. Benchmark: Copy + Hash
                    dummy_hash = 0;
                    start = std::chrono::high_resolution_clock::now();
                    for (size_t i = 0; i < iterations; ++i) {
                        dummy_hash += bq::util::bq_memcpy_with_hash(dst, src, bench_buf_size);
                    }
                    end = std::chrono::high_resolution_clock::now();
                    diff = end - start;
                    bq::util::log_device_console(bq::log_level::debug, "Benchmark - Copy+Hash: %.3f GB/s (Sum: %" PRIu64 ")", get_throughput_gbps(diff.count()), dummy_hash);

                    bq::platform::aligned_free(src);
                    bq::platform::aligned_free(dst);
                }

                // =================================================================================
                // Performance Benchmark - Small Data (Random 20-128 Bytes)
                // =================================================================================
                {
                    const size_t bench_buf_size = static_cast<size_t>(64 * 1024 * 1024); // 64 MB Buffer to simulate RingBuffer
                    const size_t total_data_approx = static_cast<size_t>(1024 * 1024 * 1024); // 1 GB approx
                    
                    const size_t size_table_len = 4096;
                    const size_t size_table_mask = size_table_len - 1;
                    size_t random_sizes[size_table_len];
                    
                    size_t average_size = 0;
                    for (size_t i = 0; i < size_table_len; ++i) {
                        random_sizes[i] = (static_cast<size_t>(bq::util::rand()) % (128 - 20 + 1)) + 20;
                        average_size += random_sizes[i];
                    }
                    average_size /= size_table_len;
                    
                    const size_t iterations = total_data_approx / average_size;

                    uint8_t* src = static_cast<uint8_t*>(bq::platform::aligned_alloc(64, bench_buf_size));
                    uint8_t* dst = static_cast<uint8_t*>(bq::platform::aligned_alloc(64, bench_buf_size));

                    // Fill source with random data
                    for (size_t i = 0; i < bench_buf_size; ++i) {
                        src[i] = (uint8_t)(i & 0xFF);
                    }

                    auto get_throughput_gbps = [&](double seconds, size_t total_bytes) {
                        return (double)total_bytes / (1024.0 * 1024.0 * 1024.0) / seconds;
                    };

                    bq::util::log_device_console(bq::log_level::debug, "--- Small Data Benchmark (Random 20-128 Bytes, RingBuffer Sim) ---");

                    // 1. memcpy
                    size_t total_processed = 0;
                    size_t offset = 0;
                    auto start = std::chrono::high_resolution_clock::now();
                    for (size_t i = 0; i < iterations; ++i) {
                        size_t sz = random_sizes[i & size_table_mask];
                        if (offset + sz > bench_buf_size) offset = 0;
                        
                        memcpy(dst + offset, src + offset, sz);
                        
                        total_processed += sz;
                        offset += sz;
                    }
                    auto end = std::chrono::high_resolution_clock::now();
                    std::chrono::duration<double> diff = end - start;
                    bq::util::log_device_console(bq::log_level::debug, "Small Bench - memcpy: %.3f GB/s", get_throughput_gbps(diff.count(), total_processed));

                    // 2. Hash Only
                    uint64_t dummy_hash = 0;
                    total_processed = 0;
                    offset = 0;
                    start = std::chrono::high_resolution_clock::now();
                    for (size_t i = 0; i < iterations; ++i) {
                        size_t sz = random_sizes[i & size_table_mask];
                        if (offset + sz > bench_buf_size) offset = 0;

                        dummy_hash += bq::util::bq_hash_only(src + offset, sz);
                        
                        total_processed += sz;
                        offset += sz;
                    }
                    end = std::chrono::high_resolution_clock::now();
                    diff = end - start;
                    bq::util::log_device_console(bq::log_level::debug, "Small Bench - Hash Only: %.3f GB/s (Sum: %" PRIu64 ")", get_throughput_gbps(diff.count(), total_processed), dummy_hash);

                    // 3. Copy + Hash
                    dummy_hash = 0;
                    total_processed = 0;
                    offset = 0;
                    start = std::chrono::high_resolution_clock::now();
                    for (size_t i = 0; i < iterations; ++i) {
                        size_t sz = random_sizes[i & size_table_mask];
                        if (offset + sz > bench_buf_size) offset = 0;

                        dummy_hash += bq::util::bq_memcpy_with_hash(dst + offset, src + offset, sz);
                        
                        total_processed += sz;
                        offset += sz;
                    }
                    end = std::chrono::high_resolution_clock::now();
                    diff = end - start;
                    bq::util::log_device_console(bq::log_level::debug, "Small Bench - Copy+Hash: %.3f GB/s (Sum: %" PRIu64 ")", get_throughput_gbps(diff.count(), total_processed), dummy_hash);

                    bq::platform::aligned_free(src);
                    bq::platform::aligned_free(dst);
                }
                bq::util::set_log_device_console_min_level(bq::log_level::warning);
                
                return result;
            }
        };
    }
}