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
using System.Runtime.InteropServices;
using System.Collections.Generic;
using System;

namespace bq.def
{
    /// <summary>
    /// Result codes the log buffer returns.
    /// </summary>
    public enum enum_buffer_result_code
    {
        success = 0,
        err_empty_log_buffer,       // no valid data to read in log buffer;
        err_not_enough_space,       // not enough space in log buffer to alloc
        err_wait_and_retry,         // need wait and try again
        err_data_not_contiguous,    // data is not contiguous, this error code is only used for internal statistics within the log_buffer and will not be exposed externally.
        err_alloc_size_invalid,     // invalid alloc size, too big or 0.
        err_buffer_not_inited,      // buffer not initialized
        result_code_count
    };

    /// <summary>
    /// Log levels, in the order the library defines them.
    /// </summary>
    public enum log_level
    {
        verbose,
        debug,
        info,
        warning,
        error,
        fatal,

        log_level_max = 32,        //上限
    }

    /// <summary>
    /// What this platform offers for crash recovery, and what is missing when it is not available.
    /// </summary>
    public enum recovery_capability
    {
        ready = 0,          // recovery can be enabled
        no_memory_map,      // nothing survives a crash without a memory map
        no_instance_lock,   // no cross-process file lock to keep a second process out
    }

    /// <summary>
    /// Bits of log.get_platform_capabilities(): what the linked library was built with.
    /// </summary>
    [Flags]
    public enum platform_capability : uint
    {
        memory_map = 1u << 0,           // files can be mapped; crash recovery needs this
        multi_process = 1u << 1,        // several processes may share one storage directory
        cross_process_file_lock = 1u << 2, // a second process can be kept out of an instance
        crash_handler = 1u << 3,        // buffers can be flushed from a crash handler
        backtrace = 1u << 4,            // stack traces can be collected
        dirent = 1u << 5,               // directories can be enumerated (expire_time, capacity)
        thread_name = 1u << 6,          // threads can be named
        default_base_dir = 1u << 7,     // a writable default directory exists; consoles reset it
        user_counter = 1u << 8,         // a user mode hardware counter is available
    }

    internal enum log_arg_type_enum
    {
        unsupported_type,
        null_type,
        pointer_type,
        bool_type,
        char_type,
        char16_type,
        char32_type,
        int8_type,
        uint8_type,
        int16_type,
        uint16_type,
        int32_type,
        uint32_type,
        int64_type,
        uint64_type,
        float_type,
        double_type,
        string_utf8_type,
        string_utf16_type
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    internal unsafe struct _api_log_buffer_chunk_read_handle
    {
        public byte* data_ptr;
        public enum_buffer_result_code result_code;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    /// <summary>
    /// One reserved buffer chunk: write the entry at format_data_addr and report the result in
    /// result_code.
    /// </summary>
    public unsafe struct _api_log_buffer_chunk_write_handle
    {
        public byte* format_data_addr;
        public enum_buffer_result_code result_code;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 8)]
    internal unsafe struct _log_entry_head_def
    {
        public ulong timestamp_epoch;
        public uint ext_info_offset;
        public uint category_idx;
        public ulong format_hash;
        public byte log_format_str_type;
        public byte level;
        public ushort padding;
        public uint log_format_data_len;
    }


    [StructLayout(LayoutKind.Sequential, Pack = 4)]
    /// <summary>
    /// A string the native side owns: str points at it and len is its length in bytes.
    /// </summary>
    public unsafe struct _api_string_def
    {
        public sbyte* str;
        public uint len;
    }

    [UnmanagedFunctionPointer(CallingConvention.StdCall, CharSet = CharSet.Unicode)]
    /// <summary>
    /// Console callback the native side invokes for every console entry.
    /// </summary>
    public unsafe delegate void type_func_ptr_console_callback(ulong log_id, int category_idx, bq.def.log_level log_level,sbyte* content, int length);
    
    [UnmanagedFunctionPointer(CallingConvention.StdCall, CharSet = CharSet.Unicode)]
    /// <summary>
    /// Console callback used while draining the console buffer; pass_through_param is what the
    /// caller passed to the fetch call.
    /// </summary>
    public unsafe delegate void type_func_ptr_console_buffer_fetch_callback(IntPtr pass_through_param, ulong log_id, int category_idx, bq.def.log_level log_level, sbyte* content, int length);
    
}