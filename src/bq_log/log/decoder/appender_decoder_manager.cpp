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
#include "bq_log/log/decoder/appender_decoder_manager.h"

#include "bq_log/global/log_vars.h"
#include "bq_log/log/decoder/appender_decoder_compressed.h"
#include "bq_log/log/appender/appender_file_binary.h"

bq::appender_decoder_manager::appender_decoder_manager()
    :
#if !defined(BQ_TOOLS)
    mutex_(true)
    ,
#endif
    idx_seq_(0)
{
}

bq::appender_decoder_manager::~appender_decoder_manager()
{
}

bq::appender_decoder_manager& bq::appender_decoder_manager::instance()
{
    return log_global_vars::get().appender_decoder_manager_inst_;
}

bq::appender_decode_result bq::appender_decoder_manager::create_decoder(const bq::string& path, const bq::string& private_key_str, uint32_t& out_handle)
{
    string path_tmp = TO_ABSOLUTE_PATH(path, 1);
    auto handle = bq::file_manager::instance().open_file(path_tmp, file_open_mode_enum::read);
    if (!handle.is_valid()) {
        bq::util::log_device_console(log_level::error, "decode log file :%s open failed, error code:%" PRId32, path.c_str(), bq::file_manager::get_and_clear_last_file_error());
        return appender_decode_result::failed_io_error;
    }
    bq::appender_file_binary::appender_file_header head;
    auto read_size = bq::file_manager::instance().read_file(handle, &head, sizeof(head), file_manager::seek_option::begin, 0);
    if (read_size != sizeof(head)) {
        bq::util::log_device_console(log_level::error, "decode log file :%s failed, size less than appender head", path.c_str());
        return appender_decode_result::failed_decode_error;
    }
    bq::unique_ptr<appender_decoder_base> decoder;
    if (head.format == bq::appender_file_binary::appender_format_type::compressed) {
        if (head.version == bq::appender_file_compressed::format_version) {
            decoder = bq::make_unique<bq::appender_decoder_compressed>();
        } else {
            auto factory_iter = extra_factories_.find(head.version);
            if (factory_iter == extra_factories_.end()) {
                // Not "corrupt", not "unsupported by this tool version": the runtime carries one
                // format version only, so the advice is where to find the other one.
                bq::util::log_device_console(log_level::error,
                    "decode log file :%s failed, format version %" PRIu32 " is not supported by this runtime (supports %" PRIu32 "), use the standalone BqLog_LogDecoder tool",
                    path.c_str(), head.version, bq::appender_file_compressed::format_version);
                return appender_decode_result::failed_decode_error;
            }
            decoder = factory_iter->value()();
        }
    } else {
        bq::util::log_device_console(log_level::error, "decode log file :%s failed, unrecognized format", path.c_str());
        return appender_decode_result::failed_decode_error;
    }
    bq::appender_decode_result result = decoder->init(handle, private_key_str);
    if (result != appender_decode_result::success) {
        return result;
    }
    out_handle = idx_seq_.add_fetch_seq_cst(1);
#if !defined(BQ_TOOLS)
    bq::platform::scoped_mutex lock(mutex_);
#endif
    decoders_map_.add(out_handle, bq::move(decoder));

    return result;
}

void bq::appender_decoder_manager::register_decoder_factory(uint32_t format_version, decoder_factory_fn factory)
{
#if !defined(BQ_TOOLS)
    bq::platform::scoped_mutex lock(mutex_);
#endif
    extra_factories_.add(format_version, factory);
}

bq::array<uint32_t> bq::appender_decoder_manager::get_supported_format_versions() const
{
    bq::array<uint32_t> versions;
    for (auto iter = extra_factories_.begin(); iter != extra_factories_.end(); ++iter) {
        versions.push_back(iter->key());
    }
    // Copied into a local first: push_back takes a reference, and binding one to a static
    // constexpr data member is an odr-use that needs an out-of-line definition before C++17.
    const uint32_t runtime_version = bq::appender_file_compressed::format_version;
    versions.push_back(runtime_version);
    // Oldest first, so the list reads the way the formats were added.
    const size_t count = versions.size();
    for (size_t i = 1; i < count; ++i) {
        for (size_t j = i; j > 0 && versions[j - 1] > versions[j]; --j) {
            const uint32_t tmp = versions[j - 1];
            versions[j - 1] = versions[j];
            versions[j] = tmp;
        }
    }
    return versions;
}

void bq::appender_decoder_manager::destroy_decoder(uint32_t handle)
{
#if !defined(BQ_TOOLS)
    bq::platform::scoped_mutex lock(mutex_);
#endif
    decoders_map_.erase(handle);
}

bq::appender_decode_result bq::appender_decoder_manager::decode_single_item(uint32_t handle, const bq::string*& out_decoded_log_text)
{
#if !defined(BQ_TOOLS)
    bq::platform::scoped_mutex lock(mutex_);
#endif
    auto iter = decoders_map_.find(handle);
    if (iter != decoders_map_.end()) {
        auto result = iter->value()->decode();
        if (result == appender_decode_result::success) {
            out_decoded_log_text = &iter->value()->get_decoded_log_text();
        }
        return result;
    }
    return appender_decode_result::failed_invalid_handle;
}
