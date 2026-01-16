/*
 * Copyright (C) 2025 Tencent.
 * BQLOG is licensed under the Apache License, Version 2.0.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */
using bq.def;
using System;
using System.Diagnostics;
using System.Security.Cryptography;

namespace bq.impl
{

    internal unsafe struct log_context
    {
        private uint params_size_;
        private log_category_base category_;
        private log_level level_;
        private _api_log_buffer_chunk_write_handle handle_;
        private byte* log_params_addr_;
        private string format_str_;
        private uint format_str_storage_size_;
        private uint format_str_storage_size_aligned_;
        private log log_;


        public log_context(log log, log_category_base category, log_level level, string format_str, uint params_size)
        {
            log_ = log;
            category_ = category;
            level_ = level;
            handle_ = new _api_log_buffer_chunk_write_handle();
            log_params_addr_ = null;
            if(null == format_str)
            {
                format_str = "null";
            }
            if (bq.log.is_stack_trace_enable_for(log, level))
            {
                format_str += "\n" + new StackTrace(2, true);
            }
            format_str_ = format_str;
            format_str_storage_size_ = (uint)format_str_.Length << 1;
            format_str_storage_size_aligned_ = utils.align4(format_str_storage_size_);
            params_size_ = params_size;
        }

        

        public unsafe bool begin_copy()
        {
            fixed (char* format_c_style = format_str_)
            {
                byte* format_byte = (byte*)format_c_style;
                handle_ = log_invoker.__api_log_write_begin(log_.get_id(), (byte)level_, log_category_base.get_index(category_), (byte)log_arg_type_enum.string_utf16_type, format_str_storage_size_, format_byte, params_size_);
            }
            if (handle_.result_code != enum_buffer_result_code.success)
            {
                return false;
            }
            log_params_addr_ = handle_.format_data_addr + format_str_storage_size_aligned_;
            return true;
        }

        public unsafe byte* get_params_data_addr()
        {
            return log_params_addr_;
        }

        public unsafe void end_copy()
        {
            log_invoker.__api_log_write_finish(log_.get_id(), handle_);
        }
    }

}