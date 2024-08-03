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
using bq.def;
using System;
using System.Diagnostics;

namespace bq.impl
{

    internal unsafe struct log_context
    {
        private uint format_size_;
        private uint params_size_;
        private uint total_size_;
        private log_category_base category_;
        private log_level level_;
        private _log_api_ring_buffer_write_handle handle_;
        private byte* log_params_addr_;
        private param_wrapper format_str_;
        private log log_;


        public log_context(log log, log_category_base category, log_level level, string format_str, uint params_size)
        {
            log_ = log;
            category_ = category;
            level_ = level;
            handle_ = new _log_api_ring_buffer_write_handle();
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
            format_size_ = format_str_.aligned_size - 4;  //no typeinfo
            params_size_ = params_size;
            total_size_ = (uint)sizeof(bq.def._log_head_def) + format_size_ + params_size_;
        }

        

        public unsafe bool begin_copy()
        {
            handle_ = log_invoker.__api_log_buffer_alloc(log_.get_id(), total_size_);
            if (handle_.result_code != enum_buffer_result_code.success)
            {
                return false;
            }
            _log_head_def* head = (_log_head_def*)handle_.data_ptr;
            head->category_idx = log_category_base.get_index(category_);
            //head->category_hash = log_category_base.get_value(category_);
            head->level = (byte)level_;
            head->log_format_str_type = (byte)log_arg_type_enum.string_utf16_type;
            head->log_args_offset = (ushort)(sizeof(_log_head_def) + format_size_);
            var log_format_content_addr = handle_.data_ptr + sizeof(_log_head_def);
            uint str_size = format_str_.storage_size - 4 - sizeof(UInt32);
            *(uint*)(log_format_content_addr) = str_size;
            log_format_content_addr += 4;
            if (format_str_.string_value != null)
            {
                utils.str_memcpy(log_format_content_addr, format_str_.string_value, str_size);
            }
            log_params_addr_ = handle_.data_ptr + head->log_args_offset;
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