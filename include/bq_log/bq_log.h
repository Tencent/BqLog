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
/*!
 * This is the only header file you need include in your project.
 *
 * \brief
 *
 * \author pippocao
 * \date 2022.08.03
 */
#include <stdint.h>
#include "bq_common/bq_common_public_include.h"
#include "bq_log/adapters/adapters.h"
#include "bq_log/misc/bq_log_def.h"
#include "bq_log/misc/bq_log_api.h"
#include "bq_log/misc/bq_log_wrapper_tools.h"

namespace bq {
    class log {
    private:
        const uint32_t* merged_log_level_bitmap_;
        const uint8_t* categories_mask_array_;
        const uint32_t* print_stack_level_bitmap_;
        uint64_t log_id_;
        bq::string name_;

    protected:
        bq::array<bq::string> categories_name_array_;

        template <typename STR>
        struct is_bq_log_format {
            static constexpr bool value = bq::tools::_is_bq_log_format_type<STR>::value;
        };

    protected:
        log()
            : merged_log_level_bitmap_(nullptr)
            , categories_mask_array_(nullptr)
            , print_stack_level_bitmap_(nullptr)
            , log_id_(0)
        {
        }

        template <typename STR>
        bq::enable_if_t<is_bq_log_format<STR>::value, bool> do_log(uint32_t category_index, bq::log_level level, const STR& log_format_content) const;

        template <typename STR, typename... Args>
        bq::enable_if_t<is_bq_log_format<STR>::value, bool> do_log(uint32_t category_index, bq::log_level level, const STR& log_format_content, const Args&... args) const;

        static log get_log_by_id(uint64_t log_id);

        bool is_enable_for(uint32_t category_index, bq::log_level level) const;

    public:
        /// <summary>
        /// get bqLog lib version
        /// </summary>
        /// <returns></returns>
        static bq::string get_version();

        /// <summary>
        /// If bqLog is asynchronous, a crash in the program may cause the logs in the buffer not to be persisted to disk.
        /// If this feature is enabled, bqLog will attempt to perform a forced flush of the logs in the buffer in the event of a crash. However,
        /// this functionality does not guarantee success, and only support POSIX systems.
        /// </summary>
        static void enable_auto_crash_handle();

        /// <summary>
        /// If bqLog is stored in a relative path, it will choose whether the relative path is within the sandbox or not.
        /// This will return the absolute paths corresponding to both scenarios.
        /// </summary>
        /// <param name="is_in_sandbox"></param>
        /// <returns></returns>
        static bq::string get_file_base_dir(bool is_in_sandbox);

        /// <summary>
        /// Create a log object
        /// </summary>
        /// <param name="log_name">If the log name is an empty string, bqLog will automatically assign you a unique log name. If the log name already exists, it will return the previously existing log object and overwrite the previous configuration with the new config.</param>
        /// <param name="config_content">Log config string</param>
        /// <returns>A log object, if create failed, the is_valid() method of it will return false</returns>
        static log create_log(const bq::string& log_name, const bq::string& config_content);

        /// <summary>
        /// Get a log object by it's name
        /// </summary>
        /// <param name="log_name">Name of the log object you want to find</param>
        /// <returns>A log object, if the log object with specific name was not found, the is_valid() method of it will return false</returns>
        static log get_log_by_name(const bq::string& log_name);

        /// <summary>
        /// Synchronously flush the buffer of all log objects
        /// to ensure that all data in the buffer is processed after the call.
        /// </summary>
        static void force_flush_all_logs();

        /// <summary>
        /// Register a callback that will be invoked whenever a console log message is output.
        /// This can be used for an external system to monitor console log output.
        /// </summary>
        /// <param name="callback"></param>
        static void register_console_callback(bq::type_func_ptr_console_callback callback);

        /// <summary>
        /// Unregister a console callback.
        /// </summary>
        /// <param name="callback"></param>
        static void unregister_console_callback(bq::type_func_ptr_console_callback callback);

        /// <summary>
        /// Enable or disable the console appender buffer.
        /// Since our wrapper may run in both C# and Java virtual machines, and we do not want to directly invoke callbacks from a native thread,
        /// we can enable this option. This way, all console outputs will be saved in the buffer until we fetch them.
        /// </summary>
        /// <param name="enable"></param>
        /// <returns></returns>
        static void set_console_buffer_enable(bool enable);

        /// <summary>
        /// Fetch and remove a log entry from the console appender buffer in a thread-safe manner.
        /// If the console appender buffer is not empty, the on_console_callback function will be invoked for this log entry.
        /// Please ensure not to output synchronized BQ logs within the callback function.
        /// </summary>
        /// <param name="on_console_callback">A callback function to be invoked for the fetched log entry if the console appender buffer is not empty</param>
        /// <returns>True if the console appender buffer is not empty and a log entry is fetched; otherwise False is returned.</returns>
        static bool fetch_and_remove_console_buffer(bq::type_func_ptr_console_callback on_console_callback);

        /// <summary>
        /// Output to console with log_level.
        /// Important: This is not log entry, and can not be caught by console callback which was registered by register_console_callback or fetch_and_remove_console_buffer.
        /// </summary>
        /// <typeparam name="STR">c style char*(Only utf-8 string supported)</typeparam>
        /// <param name="level"></param>
        /// <param name="str"></param>
        /// <returns></returns>
        template <typename STR>
        static bq::enable_if_t<bq::is_same<bq::decay_t<bq::remove_cv_t<STR>>, char*>::value || bq::is_same<bq::decay_t<bq::remove_cv_t<STR>>, const char*>::value> console(bq::log_level level, const STR& str);

        /// <summary>
        /// Output to console with log_level.
        /// Important: This is not log entry, and can not be caught by console callback which was registered by register_console_callback or fetch_and_remove_console_buffer
        /// </summary>
        /// <typeparam name="STR">std::string or bq::string(Only utf-8 string supported)</typeparam>
        /// <param name="level"></param>
        /// <param name="str"></param>
        /// <returns></returns>
        template <typename STR>
        static bq::enable_if_t<!(bq::is_same<bq::decay_t<bq::remove_cv_t<STR>>, char*>::value || bq::is_same<bq::decay_t<bq::remove_cv_t<STR>>, const char*>::value)> console(bq::log_level level, const STR& str);

    public:
        /// <summary>
        /// copy constructor
        /// </summary>
        /// <param name="rhs"></param>
        log(const bq::log& rhs);

        /// <summary>
        /// assign
        /// </summary>
        /// <param name="rhs"></param>
        log& operator=(const bq::log& rhs);

        /// <summary>
        /// Modify the log configuration, but some fields, such as buffer_size, cannot be modified.
        /// </summary>
        /// <param name="config_content"></param>
        /// <returns></returns>
        bool reset_config(const bq::string& config_content);

        /// <summary>
        /// Temporarily disable or enable a specific Appender.
        /// </summary>
        /// <param name="appender_name"></param>
        /// <param name="enable"></param>
        void set_appenders_enable(const bq::string& appender_name, bool enable);

        /// <summary>
        /// Synchronously flush the buffer of this log object
        /// to ensure that all data in the buffer is processed after the call.
        /// </summary>
        void force_flush();

        /// <summary>
        /// get id of this log object
        /// </summary>
        /// <returns></returns>
        uint64_t get_id() const
        {
            return log_id_;
        }

        /// <summary>
        /// whether a log object is valid
        /// </summary>
        /// <returns></returns>
        bool is_valid() const
        {
            return get_id() != 0;
        }

        /// <summary>
        /// get the name of a log
        /// </summary>
        /// <returns></returns>
        const bq::string& get_name() const
        {
            return name_;
        }

        /// <summary>
        /// Works only when snapshot is configured.
        /// It will decode the snapshot buffer to text.
        /// </summary>
        /// <param name="use_gmt_time">whether the timestamp of each log is GMT time or local time</param>
        /// <returns>the decoded snapshot buffer</returns>
        bq::string take_snapshot(bool use_gmt_time) const;

    public:
        /// Core log functions, there are 6 log levels:
        /// verbose, debug, info, warning, error, fatal
        template <typename STR>
        bq::enable_if_t<is_bq_log_format<STR>::value, bool> verbose(const STR& log_content) const;
        template <typename STR, typename... Args>
        bq::enable_if_t<is_bq_log_format<STR>::value, bool> verbose(const STR& log_format_content, const Args&... args) const;
        template <typename STR>
        bq::enable_if_t<is_bq_log_format<STR>::value, bool> debug(const STR& log_content) const;
        template <typename STR, typename... Args>
        bq::enable_if_t<is_bq_log_format<STR>::value, bool> debug(const STR& log_format_content, const Args&... args) const;
        template <typename STR>
        bq::enable_if_t<is_bq_log_format<STR>::value, bool> info(const STR& log_content) const;
        template <typename STR, typename... Args>
        bq::enable_if_t<is_bq_log_format<STR>::value, bool> info(const STR& log_format_content, const Args&... args) const;
        template <typename STR>
        bq::enable_if_t<is_bq_log_format<STR>::value, bool> warning(const STR& log_content) const;
        template <typename STR, typename... Args>
        bq::enable_if_t<is_bq_log_format<STR>::value, bool> warning(const STR& log_format_content, const Args&... args) const;
        template <typename STR>
        bq::enable_if_t<is_bq_log_format<STR>::value, bool> error(const STR& log_content) const;
        template <typename STR, typename... Args>
        bq::enable_if_t<is_bq_log_format<STR>::value, bool> error(const STR& log_format_content, const Args&... args) const;
        template <typename STR>
        bq::enable_if_t<is_bq_log_format<STR>::value, bool> fatal(const STR& log_content) const;
        template <typename STR, typename... Args>
        bq::enable_if_t<is_bq_log_format<STR>::value, bool> fatal(const STR& log_format_content, const Args&... args) const;
    };

    class category_log : public bq::log {
        // Implement in generated classes.
    protected:
        category_log();
        category_log(const log& child_inst);

    public:
        /// <summary>
        /// get log categories count
        /// </summary>
        /// <returns></returns>
        size_t get_categories_count() const;

        /// <summary>
        /// get names of all categories
        /// </summary>
        /// <returns></returns>
        const bq::array<bq::string>& get_categories_name_array() const;
    };

    namespace tools {
        // This is a utility class for decoding binary log formats.
        // To use it, first create a log_decoder object,
        // then call its decode function to decode.
        // After each successful call,
        // you can use get_last_decoded_log_entry() to retrieve the decoded result.
        // Each call decodes one log entry.
        struct log_decoder {
        private:
            bq::string decode_text_;
            bq::appender_decode_result result_ = bq::appender_decode_result::success;
            uint32_t handle_ = 0;

        public:
            /// <summary>
            /// Create a log_decoder object, with each log_decoder object corresponding to a binary log file.
            /// </summary>
            /// <param name="log_file_path">the path of a binary log file, is can be relative path or absolute path</param>
            /// <param name="priv_key">private key generated by "ssh-keygen" to decrypt encrypted log file, left it to empty when log file is not encrypted.</param>
            log_decoder(const bq::string& log_file_path, const bq::string& priv_key = "");
            ~log_decoder();
            /// <summary>
            /// Decode a log entry. each call of this function will decode only 1 log entry
            /// </summary>
            /// <returns>decode result, appender_decode_result::eof means the whole log file was decoded</returns>
            bq::appender_decode_result decode();
            /// <summary>
            /// get the last decode result
            /// </summary>
            /// <returns></returns>
            bq::appender_decode_result get_last_decode_result() const;
            /// <summary>
            /// get the last decode log entry content
            /// </summary>
            /// <returns></returns>
            const bq::string& get_last_decoded_log_entry() const;
            /// <summary>
            /// Direct decode a log file to a text file.
            /// </summary>
            /// <param name="log_file_path"></param>
            /// <param name="output_file"></param>
            /// <param name="priv_key">private key generated by "ssh-keygen" to decrypt encrypted log file, left it to empty when log file is not encrypted.</param>
            /// <returns>success or not</returns>
            static bool decode_file(const bq::string& log_file_path, const bq::string& output_file, const bq::string& priv_key = "");
        };
    }
}

#include "bq_log/misc/bq_log_impl.h"
