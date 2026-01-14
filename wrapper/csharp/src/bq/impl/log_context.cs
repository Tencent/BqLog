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
        private uint total_size_;
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
            total_size_ = (uint)sizeof(bq.def._log_entry_head_def) + format_str_storage_size_aligned_ + params_size_;
        }

        

        public unsafe bool begin_copy()
        {
            fixed (char* format_c_style = format_str_)
            {
                byte* format_byte = (byte*)format_c_style;
                handle_ = log_invoker.__api_log_buffer_alloc_with_format_string(log_.get_id(), total_size_, (byte)log_arg_type_enum.string_utf16_type, format_byte, format_str_storage_size_);
            }
            if (handle_.result_code != enum_buffer_result_code.success)
            {
                return false;
            }
            _log_entry_head_def* head = (_log_entry_head_def*)handle_.data_ptr;
            head->category_idx = log_category_base.get_index(category_);
            head->level = (byte)level_;
            log_params_addr_ = handle_.data_ptr + sizeof(_log_entry_head_def) + format_str_storage_size_aligned_;
            return true;
        }

        public unsafe byte* get_params_data_addr()
        {
            return log_params_addr_;
        }

        public unsafe void end_copy()
        {
            log_invoker.__api_log_buffer_commit(log_.get_id(), handle_);
        }
    }

}