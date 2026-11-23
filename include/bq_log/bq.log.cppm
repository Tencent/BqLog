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
// The module entry of the library: import bq.log; hands out the public API the headers declare.
//
// The headers are read in the global module fragment and their names are re-exported below, instead
// of being read inside the module purview. A definition inside the purview is owned by the module:
// clang gives it a strong, non-COMDAT symbol, because an importer emits no copy of it. The library
// itself is always built from the headers, so its objects carry the usual COMDAT copies of the same
// inline functions, and the two cannot be linked together -- which is exactly what a program that
// imports this module and links the library does. Re-exporting textual declarations keeps every
// definition outside the module, so an importer emits ordinary COMDAT copies that merge with the
// library.
//
// A name that is not listed below stays internal to the library. Its header can still be included
// next to the import, because nothing here is attached to the module.

module;

// The system headers the headers below pull in, so that the includes inside them are no-ops, plus
// the switch that tells them so. The list lives in that one file so every entry preincludes the
// same set.
#include "bq_log/module_system_headers.h"

#include "bq_log/bq_log.h"

export module bq.log;

export namespace bq {
// Containers and typedefs the API is written in terms of.
using ::bq::array;
using ::bq::string;
using ::bq::u16string;
using ::bq::u32string;

// Enums that appear in the API, in a config value or in a decoded entry.
using ::bq::appender_decode_result;
using ::bq::enum_buffer_result_code;
using ::bq::log_arg_type_enum;
using ::bq::log_level;
using ::bq::log_memory_policy;
using ::bq::log_thread_mode;
using ::bq::recovery_capability;

// Callback types of the console, the buffered console and the secure random source.
using ::bq::type_func_ptr_console_buffer_fetch_callback;
using ::bq::type_func_ptr_console_callback;
using ::bq::type_func_ptr_secure_random_source;

// Bits of log::get_platform_capabilities(), as the API reference names them.
namespace platform_capability {
using ::bq::platform_capability::backtrace;
using ::bq::platform_capability::crash_handler;
using ::bq::platform_capability::cross_process_file_lock;
using ::bq::platform_capability::default_base_dir;
using ::bq::platform_capability::dirent;
using ::bq::platform_capability::memory_map;
using ::bq::platform_capability::multi_process;
using ::bq::platform_capability::thread_name;
using ::bq::platform_capability::user_counter;
}

// The log classes, and the helpers the fast interface takes as template arguments.
using ::bq::category_log;
using ::bq::fixed_format;
using ::bq::log;
using ::bq::log_category_base;
using ::bq::log_category_index;

// The binary log decoder, and the char type its format strings use.
namespace tools {
using ::bq::tools::_is_bq_log_format_type;
using ::bq::tools::bq_char8_t;
using ::bq::tools::log_decoder;
}

// The helpers the code of the category log generator is written in terms of: that generated header
// is compiled as a module of its own, which imports this one instead of including the headers, so
// the names it spells out have to be part of this surface. Nothing else from the headers' internals
// is exported here; a generated header that starts to name another internal fails the module test
// rather than silently pulling it in.
using ::bq::decay_t;
using ::bq::enable_if_t;

namespace api {
using ::bq::api::__api_create_log;
}

namespace fast_info_detail {
using ::bq::fast_info_detail::fast_impl;
using ::bq::fast_info_detail::is_category;
}
}

// The placement new the containers construct their elements with, and the tag type that tells it
// from the standard one (bq_common/types/type_tools.h). An importer instantiates bq::string and
// bq::array, and the allocation function of a new-expression whose type is still dependent is
// looked up in the translation unit doing the instantiation: gcc 15 does not look back into this
// unit's global module fragment for it. Re-exporting the declarations here makes them part of the
// import, so a consumer needs no include of its own. They are declared at global scope, so unlike
// the names above they are re-exported there.
export {
using ::operator new;
using ::operator new[];
using ::operator delete;
using ::operator delete[];
}
