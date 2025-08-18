#pragma once
/*
 * Copyright (C) 2024 THL A29 Limited, a Tencent company.
 * BQLOG is licensed under the Apache License, Version 2.0.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */
#include "bq_common/bq_common.h"
#include "bq_log/log/log_level_bitmap.h"

namespace bq {
    class log_utils {
    public:
        template <typename T>
        struct is_signed {
        public:
            using Type = typename bq::remove_cv_t<typename bq::remove_reference_t<T>>;
            static constexpr bool value = static_cast<Type>(-1) < static_cast<Type>(0);
        };

        template <typename T>
        struct make_unsinged {
        public:
            using type = typename bq::condition_type_t<
                sizeof(T) == 1, uint8_t,
                typename bq::condition_type_t<
                    sizeof(T) == 2, uint16_t,
                    typename bq::condition_type_t<
                        sizeof(T) == 4, uint32_t,
                        typename bq::condition_type_t<
                            sizeof(T) == 8, uint64_t, void>>>>;
            static_assert(!is_signed<type>::value, " ");
        };

        template <typename T>
        struct make_singed {
        public:
            using type = typename bq::condition_type_t<
                sizeof(T) == 1, int8_t,
                typename bq::condition_type_t<
                    sizeof(T) == 2, int16_t,
                    typename bq::condition_type_t<
                        sizeof(T) == 4, int32_t,
                        typename bq::condition_type_t<
                            sizeof(T) == 8, int64_t, void>>>>;
            static_assert(is_signed<type>::value, " ");
        };

        class zigzag {
        public:
            template <typename T>
            bq_forceinline static BQ_FUNC_RETURN_CONSTEXPR typename make_unsinged<T>::type encode(T value)
            {
                static_assert(is_signed<T>::value, "zigzag encode only support signed integers");
                typedef typename make_unsinged<T>::type UT;
                return static_cast<UT>((value << 1) ^ (value >> (sizeof(T) * 8 - 1)));
            }
            template <typename T>
            bq_forceinline static BQ_FUNC_RETURN_CONSTEXPR typename make_singed<T>::type decode(T value)
            {
                static_assert(!is_signed<T>::value, "zigzag decode only support unsigned integers");
                typedef typename make_singed<T>::type ST;
                return static_cast<ST>((value >> 1) ^ (static_cast<T>(0) - (value & 1)));
            }
        };

        class vlq {
        private:
            template <uint32_t LENGTH>
            class prefix {
            public:
                static constexpr uint8_t value = 1 << (8 - LENGTH); // 0b00000001
            };

            template <uint32_t LENGTH>
            class min_value_of_length {
            public:
                static constexpr uint64_t value = min_value_of_length<LENGTH - 1>::value + ((uint64_t)1 << ((LENGTH - 1) * 7));
            };

            bq_forceinline static BQ_FUNC_RETURN_CONSTEXPR uint32_t get_vlq_decode_length(uint8_t prefix_byte);

        public:
            static constexpr size_t invalid_decode_length = (size_t)-1;

            template <typename T>
            bq_forceinline static constexpr size_t vlq_max_bytes_count()
            {
                return sizeof(T) + 1;
            }

            bq_forceinline static BQ_FUNC_RETURN_CONSTEXPR uint32_t get_vlq_encode_length(uint64_t value);

            bq_forceinline static BQ_FUNC_RETURN_CONSTEXPR uint64_t get_vlq_data_min_size_by_encode_length(uint32_t encode_length);

            bq_forceinline static BQ_FUNC_RETURN_CONSTEXPR uint64_t get_vlq_data_max_size_by_encode_length(uint32_t encode_length);

            template <typename T>
            bq_forceinline static BQ_FUNC_RETURN_CONSTEXPR size_t vlq_encode(T value, void* target_data, size_t data_len);

            template <typename T>
            bq_forceinline static BQ_FUNC_RETURN_CONSTEXPR size_t vlq_decode(T& value, const void* src_data);
        };

        // Recursive function to return gcd of a and b
        template <typename T>
        T gcd(T a, T b)
        {
            if (b == 0)
                return a;
            return gcd(b, a % b);
        }

        // Function to return LCM of two numbers
        template <typename T>
        T lcm(T a, T b)
        {
            return (a / gcd(a, b)) * b;
        }

        static bool get_categories_mask_by_config(const bq::array<bq::string> categories_name, const bq::property_value& categories_mask_config, bq::array_inline<uint8_t>& out_categories_mask);

        static bool get_log_level_bitmap_by_config(const bq::property_value& log_level_bitmap_config, bq::log_level_bitmap& out_level_bitmap);
    };

    template <>
    class log_utils::vlq::prefix<9> {
    public:
        static constexpr uint8_t value = 0;
    };
    template <>
    class log_utils::vlq::min_value_of_length<1> {
    public:
        static constexpr uint64_t value = 0;
    };

    bq_forceinline BQ_FUNC_RETURN_CONSTEXPR uint32_t log_utils::vlq::get_vlq_encode_length(uint64_t value)
    {
        if (value < min_value_of_length<2>::value) {
            return 1;
        } else if (value < min_value_of_length<3>::value) {
            return 2;
        } else if (value < min_value_of_length<4>::value) {
            return 3;
        } else if (value < min_value_of_length<5>::value) {
            return 4;
        } else if (value < min_value_of_length<6>::value) {
            return 5;
        } else if (value < min_value_of_length<7>::value) {
            return 6;
        } else if (value < min_value_of_length<8>::value) {
            return 7;
        } else if (value < min_value_of_length<9>::value) {
            return 8;
        }
        return 9;
    }

    bq_forceinline BQ_FUNC_RETURN_CONSTEXPR uint64_t log_utils::vlq::get_vlq_data_min_size_by_encode_length(uint32_t encode_length)
    {
        constexpr uint64_t min_value_of_length_[9] = {
            min_value_of_length<1>::value, min_value_of_length<2>::value, min_value_of_length<3>::value, min_value_of_length<4>::value, min_value_of_length<5>::value, min_value_of_length<6>::value, min_value_of_length<7>::value, min_value_of_length<8>::value, min_value_of_length<9>::value
        };
        assert(encode_length <= 9);
        return min_value_of_length_[encode_length - 1];
    }

    bq_forceinline BQ_FUNC_RETURN_CONSTEXPR uint64_t log_utils::vlq::get_vlq_data_max_size_by_encode_length(uint32_t encode_length)
    {
        constexpr uint64_t max_value_of_length_[9] = {
            min_value_of_length<2>::value - 1, min_value_of_length<3>::value - 1, min_value_of_length<4>::value - 1, min_value_of_length<5>::value - 1, min_value_of_length<6>::value - 1, min_value_of_length<7>::value - 1, min_value_of_length<8>::value - 1, min_value_of_length<9>::value - 1, UINT64_MAX
        };
        assert(encode_length <= 9);
        return max_value_of_length_[encode_length - 1];
    }

    bq_forceinline BQ_FUNC_RETURN_CONSTEXPR uint32_t log_utils::vlq::get_vlq_decode_length(uint8_t prefix_byte)
    {
        /*
        if (prefix_byte == 0)
            return 9;
#if defined(BQ_MSVC)
        uint32_t zero_count = _lzcnt_u32(static_cast<uint32_t>(prefix_byte));
#else
        uint32_t zero_count = __builtin_clz(static_cast<uint32_t>(prefix_byte));
#endif
        const uint32_t bit_pos = 31 - zero_count;
        return 8 - bit_pos;
        */
        constexpr uint8_t vlq_length_lut[256] = {
            9,
            8,
            7, 7,
            6, 6, 6, 6,
            5, 5, 5, 5, 5, 5, 5, 5,
            4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
            3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
            2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
            2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
        };
        return vlq_length_lut[prefix_byte];
    }

    template <typename T>
    bq_forceinline BQ_FUNC_RETURN_CONSTEXPR size_t log_utils::vlq::vlq_encode(T value, void* target_data, size_t data_len)
    {
        using PURE_T = typename bq::remove_cv_t<typename bq::remove_reference_t<T>>;
        static_assert(is_signed<PURE_T>::value == false, "vlq_encode only support unsigned integers");
        static_assert(sizeof(T) <= 8, "vlq_encode only support integers with size less than or equal to 8 bytes");
        (void)data_len;
        using U_T = typename make_unsinged<PURE_T>::type;
        uint32_t length = get_vlq_encode_length((U_T)value);
#ifndef NDEBUG
        assert((size_t)length <= data_len && "VLQ encoding buffer size not enough");
        assert(bq::util::is_little_endian() && "Only Little-Endian is Supported!");
#endif
        uint8_t* target_data_uint8 = (uint8_t*)target_data;
        uint64_t encoded_value = 0;
        const uint8_t* encoded_value_ptr = (const uint8_t*)&encoded_value;
        switch (length) {
        case 1:
            encoded_value = value - min_value_of_length<1>::value;
            target_data_uint8[0] = prefix<1>::value + encoded_value_ptr[0];
            break;
        case 2:
            encoded_value = value - min_value_of_length<2>::value;
            target_data_uint8[0] = prefix<2>::value + encoded_value_ptr[1];
            target_data_uint8[1] = encoded_value_ptr[0];
            break;
        case 3:
            encoded_value = value - min_value_of_length<3>::value;
            target_data_uint8[0] = prefix<3>::value + encoded_value_ptr[2];
            target_data_uint8[1] = encoded_value_ptr[0];
            target_data_uint8[2] = encoded_value_ptr[1];
            break;
        case 4:
            encoded_value = value - min_value_of_length<4>::value;
            target_data_uint8[0] = prefix<4>::value + encoded_value_ptr[3];
            target_data_uint8[1] = encoded_value_ptr[0];
            target_data_uint8[2] = encoded_value_ptr[1];
            target_data_uint8[3] = encoded_value_ptr[2];
            break;
        case 5:
            encoded_value = value - min_value_of_length<5>::value;
            target_data_uint8[0] = prefix<5>::value + encoded_value_ptr[4];
            target_data_uint8[1] = encoded_value_ptr[0];
            target_data_uint8[2] = encoded_value_ptr[1];
            target_data_uint8[3] = encoded_value_ptr[2];
            target_data_uint8[4] = encoded_value_ptr[3];
            break;
        case 6:
            encoded_value = value - min_value_of_length<6>::value;
            target_data_uint8[0] = prefix<6>::value + encoded_value_ptr[5];
            target_data_uint8[1] = encoded_value_ptr[0];
            target_data_uint8[2] = encoded_value_ptr[1];
            target_data_uint8[3] = encoded_value_ptr[2];
            target_data_uint8[4] = encoded_value_ptr[3];
            target_data_uint8[5] = encoded_value_ptr[4];
            break;
        case 7:
            encoded_value = value - min_value_of_length<7>::value;
            target_data_uint8[0] = prefix<7>::value + encoded_value_ptr[6];
            target_data_uint8[1] = encoded_value_ptr[0];
            target_data_uint8[2] = encoded_value_ptr[1];
            target_data_uint8[3] = encoded_value_ptr[2];
            target_data_uint8[4] = encoded_value_ptr[3];
            target_data_uint8[5] = encoded_value_ptr[4];
            target_data_uint8[6] = encoded_value_ptr[5];
            break;
        case 8:
            encoded_value = value - min_value_of_length<8>::value;
            target_data_uint8[0] = prefix<8>::value + encoded_value_ptr[7];
            target_data_uint8[1] = encoded_value_ptr[0];
            target_data_uint8[2] = encoded_value_ptr[1];
            target_data_uint8[3] = encoded_value_ptr[2];
            target_data_uint8[4] = encoded_value_ptr[3];
            target_data_uint8[5] = encoded_value_ptr[4];
            target_data_uint8[6] = encoded_value_ptr[5];
            target_data_uint8[7] = encoded_value_ptr[6];
            break;
        case 9:
            encoded_value = value - min_value_of_length<9>::value;
            target_data_uint8[0] = prefix<9>::value;
            target_data_uint8[1] = encoded_value_ptr[0];
            target_data_uint8[2] = encoded_value_ptr[1];
            target_data_uint8[3] = encoded_value_ptr[2];
            target_data_uint8[4] = encoded_value_ptr[3];
            target_data_uint8[5] = encoded_value_ptr[4];
            target_data_uint8[6] = encoded_value_ptr[5];
            target_data_uint8[7] = encoded_value_ptr[6];
            target_data_uint8[8] = encoded_value_ptr[7];
            break;
        }
        return length;
    }

    template <typename T>
    bq_forceinline BQ_FUNC_RETURN_CONSTEXPR size_t log_utils::vlq::vlq_decode(T& value, const void* src_data)
    {
        using PURE_T = typename bq::remove_cv<typename bq::remove_reference<T>::type>::type;
        static_assert(is_signed<PURE_T>::value == false, "vlq_decode only support unsigned integers");
        static_assert(sizeof(T) <= 8, "vlq_decode only support integers with size less than or equal to 8 bytes");
        const uint8_t* src_data_uint8 = (const uint8_t*)src_data;
        uint32_t length = get_vlq_decode_length(*src_data_uint8);
        if (length > vlq_max_bytes_count<PURE_T>()) {
            return invalid_decode_length;
        }
        uint64_t target_value = 0;
        uint8_t* target_ptr = (uint8_t*)&target_value;
        switch (length) {
        case 1:
            target_ptr[0] = src_data_uint8[0] - prefix<1>::value;
            target_value += min_value_of_length<1>::value;
            ;
            break;
        case 2:
            target_ptr[1] = src_data_uint8[0] - prefix<2>::value;
            target_ptr[0] = src_data_uint8[1];
            target_value += min_value_of_length<2>::value;
            ;
            break;
        case 3:
            target_ptr[2] = src_data_uint8[0] - prefix<3>::value;
            target_ptr[0] = src_data_uint8[1];
            target_ptr[1] = src_data_uint8[2];
            target_value += min_value_of_length<3>::value;
            ;
            break;
        case 4:
            target_ptr[3] = src_data_uint8[0] - prefix<4>::value;
            target_ptr[0] = src_data_uint8[1];
            target_ptr[1] = src_data_uint8[2];
            target_ptr[2] = src_data_uint8[3];
            target_value += min_value_of_length<4>::value;
            ;
            break;
        case 5:
            target_ptr[4] = src_data_uint8[0] - prefix<5>::value;
            target_ptr[0] = src_data_uint8[1];
            target_ptr[1] = src_data_uint8[2];
            target_ptr[2] = src_data_uint8[3];
            target_ptr[3] = src_data_uint8[4];
            target_value += min_value_of_length<5>::value;
            ;
            break;
        case 6:
            target_ptr[5] = src_data_uint8[0] - prefix<6>::value;
            target_ptr[0] = src_data_uint8[1];
            target_ptr[1] = src_data_uint8[2];
            target_ptr[2] = src_data_uint8[3];
            target_ptr[3] = src_data_uint8[4];
            target_ptr[4] = src_data_uint8[5];
            target_value += min_value_of_length<6>::value;
            ;
            break;
        case 7:
            target_ptr[6] = src_data_uint8[0] - prefix<7>::value;
            target_ptr[0] = src_data_uint8[1];
            target_ptr[1] = src_data_uint8[2];
            target_ptr[2] = src_data_uint8[3];
            target_ptr[3] = src_data_uint8[4];
            target_ptr[4] = src_data_uint8[5];
            target_ptr[5] = src_data_uint8[6];
            target_value += min_value_of_length<7>::value;
            ;
            break;
        case 8:
            target_ptr[7] = src_data_uint8[0] - prefix<8>::value;
            target_ptr[0] = src_data_uint8[1];
            target_ptr[1] = src_data_uint8[2];
            target_ptr[2] = src_data_uint8[3];
            target_ptr[3] = src_data_uint8[4];
            target_ptr[4] = src_data_uint8[5];
            target_ptr[5] = src_data_uint8[6];
            target_ptr[6] = src_data_uint8[7];
            target_value += min_value_of_length<8>::value;
            break;
        case 9:
            target_ptr[0] = src_data_uint8[1];
            target_ptr[1] = src_data_uint8[2];
            target_ptr[2] = src_data_uint8[3];
            target_ptr[3] = src_data_uint8[4];
            target_ptr[4] = src_data_uint8[5];
            target_ptr[5] = src_data_uint8[6];
            target_ptr[6] = src_data_uint8[7];
            target_ptr[7] = src_data_uint8[8];
            target_value += min_value_of_length<9>::value;
            break;
        default:
            assert(false && "invalid path");
            break;
        }
        value = (PURE_T)target_value;
        return length;
    }
}
