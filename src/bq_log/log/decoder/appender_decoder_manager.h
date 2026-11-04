/* Copyright (C) 2025 Tencent.
 * BQLOG is licensed under the Apache License, Version 2.0.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */
#pragma once
#include "bq_common/bq_common.h"
#include "bq_log/log/decoder/appender_decoder_base.h"

namespace bq {

    class appender_decoder_manager {
        friend struct log_global_vars;

    private:
        appender_decoder_manager();
        ~appender_decoder_manager();

    public:
        static appender_decoder_manager& instance();

        /// <summary>
        /// Makes a decoder class available for a log format version other than the one this runtime
        /// writes. The runtime registers none, because it decodes exactly one version; the standalone
        /// tool registers the old formats at startup, and its own code lives with it rather than here.
        /// </summary>
        typedef bq::unique_ptr<appender_decoder_base> (*decoder_factory_fn)();
        void register_decoder_factory(uint32_t format_version, decoder_factory_fn factory);

        /// <summary>
        /// The format versions this build can decode, oldest first.
        /// </summary>
        bq::array<uint32_t> get_supported_format_versions() const;

        /// <summary>
        /// create a file decoder
        /// </summary>
        /// <param name="path"></param>
        /// <param name="private_key"></param>
        /// <param name="out_handle"></param>
        /// <returns></returns>
        appender_decode_result create_decoder(const bq::string& path, const bq::string& private_key_str, uint32_t& out_handle);

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
#if !defined(BQ_TOOLS)
        // tools are running in single thread, performance will benefit from removing mutex
        bq::platform::mutex mutex_;
#endif
        bq::platform::atomic<uint32_t> idx_seq_;
        bq::hash_map<uint32_t, bq::unique_ptr<appender_decoder_base>> decoders_map_;
        // Filled by register_decoder_factory; empty in the runtime.
        bq::hash_map<uint32_t, decoder_factory_fn> extra_factories_;
    };
}