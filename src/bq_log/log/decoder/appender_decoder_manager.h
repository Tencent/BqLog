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
#include "bq_log/log/decoder/appender_decoder_base.h"

namespace bq {

    class appender_decoder_manager {
    private:
        appender_decoder_manager();
        ~appender_decoder_manager();

    public:
        static appender_decoder_manager& instance();

        /// <summary>
        /// create a file decoder
        /// </summary>
        /// <param name="path"></param>
        /// <param name="out_handle"></param>
        /// <returns></returns>
        appender_decode_result create_decoder(const bq::string& path, uint32_t& out_handle);

        /// <summary>
        /// destroy a decoder to release memory
        /// </summary>
        /// <param name="handle"></param>
        void destroy_decoder(uint32_t handle);

        /// <summary>
        /// decode a log item
        /// </summary>
        /// <param name="handle"></param>
        /// <param name="out_decoded_log_text">if result is success, this is the decoded log text, and it's always valid before next time you call decode_single_item or destroy_decoder</param>
        /// <returns></returns>
        appender_decode_result decode_single_item(uint32_t handle, const bq::string*& out_decoded_log_text);

    private:
#if !BQ_TOOLS
        // tools are running in single thread, performance will benefit from removing mutex
        bq::platform::mutex mutex_;
#endif
        bq::platform::atomic<uint32_t> idx_seq_;
        bq::hash_map<uint32_t, bq::unique_ptr<appender_decoder_base>> decoders_map_;
    };
}