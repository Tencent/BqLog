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
#include <inttypes.h>
#include <stdio.h>
#include "template/category_log_template_cpp.h"

namespace bq {
    bq::string category_log_template_cpp::format(const bq::string& template_string, const category_node& root_node) const
    {
        bq::string generated_code = template_string;
        generated_code = replace_with_tab_format(generated_code, "${CLASS_NAME}", class_name_);
        generated_code = replace_with_tab_format(generated_code, "${CATEGORY_NAMES}", get_category_names_code(root_node));
        generated_code = replace_with_tab_format(generated_code, "${CATEGORY_ROOT_CLASS}", get_category_class_root_define_code(root_node));
        generated_code = replace_with_tab_format(generated_code, "${CATEGORIES_COUNT}", uint64_to_string((uint64_t)root_node.get_all_nodes_count()));
        // The module is named after the class it exports, so that `import bq.business_log;`
        // and `bq::business_log` line up.
        generated_code = replace_with_tab_format(generated_code, "${MODULE_NAME}", bq::string("bq.") + class_name_);

        return generated_code;
    }

    bq::string category_log_template_cpp::get_category_names_code_recursive(const category_node& node) const
    {
        bq::string tab = "            ";
        bq::string code = tab;
        if (node.parent()) // empty means it is root node, first value
        {
            code += ", ";
        }
        code += "\"" + node.full_name() + "\"\n";
        for (const auto& child : node.get_all_children()) {
            code += get_category_names_code_recursive(*child);
        }
        return code;
    }

    bq::string category_log_template_cpp::get_category_class_code_recursive(const category_node& node, const bq::string& tab, uint64_t& index) const
    {
        bq::string code;
        // empty means it is root node, default category, needn't generate category.
        if (!node.name().is_empty()) {
            code += tab;
            code += "struct BQ_EBCO : public ";
            code += class_name_;
            code += "_category_base<";
            code += uint64_to_string(index);
            code += "> {\n";
        }
        ++index;
        for (const auto& child : node.get_all_children()) {
            code += get_category_class_code_recursive(*child, tab + "    ", index);
        }
        if (!node.name().is_empty()) {
            code += tab;
            code += "} ";
            code += node.name();
            code += ";    //";
            code += node.full_name();
            if (!node.comment().trim().is_empty()) {
                code += "    //" + node.comment();
            }
            code += "\n";
        }
        return code;
    }

    bq::string category_log_template_cpp::get_category_names_code(const category_node& root_node) const
    {
        size_t total_categories_count = root_node.get_all_nodes_count();
        bq::string code = "const char* names[" + uint64_to_string((uint64_t)total_categories_count) + "] = {\n";
        code += get_category_names_code_recursive(root_node);
        code += "};";
        return code;
    }

    bq::string category_log_template_cpp::get_category_class_root_define_code(const category_node& root_node) const
    {
        bq::string code = "struct BQ_EBCO ";
        code += class_name_;
        code += "_category_root\n";
        code += "{\n";
        uint64_t index = 0;
        code += get_category_class_code_recursive(root_node, "", index);
        code += "};";
        return code;
    }

    bq::string category_log_template_cpp::get_template_content() const
    {
        return
            R"(#pragma once
// clang-format off
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
/*!
 * Generated Wrapper For ${CLASS_NAME}
 *
 * This is a category_log that supports attaching a category to each log entry.
 * Categories can be used to filter logs within the appender settings.
 *
 *  Usage: 
 *  bq::${CLASS_NAME} my_category_log = bq::${CLASS_NAME}::create_log(log_name, log_config);  //create a ${CLASS_NAME} object with config.
 *  my_category_log.info("content");  //this is for empty category
 *  my_category_log.info(my_category_log.cat.moduleA.classB, "content"); //this is a log entry for category ModuleA.ClassB. it is generated by your Category Config File
 */

// The include entry pulls the library in here. The module entry imports it instead and defines
// the export markers below around the class, so one text serves both entries.
#if !defined(BQ_MODULE_EXPORT_BEGIN)
#include "bq_log/bq_log.h"
#endif


namespace bq {
    BQ_MODULE_EXPORT_BEGIN
    class ${CLASS_NAME} : public category_log
    {
    private:
        template<uint32_t CAT_INDEX>
        struct ${CLASS_NAME}_category_base : public bq::log_category_base<CAT_INDEX> {};

#if defined(BQ_CPP_20)
        // The index of a category that belongs to THIS class; a foreign category resolves to 0.
        // Same overload trick as bq::log_category_index, but keyed on this class's own base, so
        // log.fast_info<some_other_log.cat.x, "..."> is rejected instead of silently accepted.
        template <typename CAT>
        struct ${CLASS_NAME}_category_index {
            template <uint32_t N>
            static char (&pick(const ${CLASS_NAME}_category_base<N>*))[N + 1];
            static char (&pick(...))[1];
            static constexpr uint32_t value =
                static_cast<uint32_t>(sizeof(pick(static_cast<const CAT*>(nullptr)))) - 1;
        };
        template <auto CAT>
        struct fast_category_index {
            static constexpr uint32_t value = ${CLASS_NAME}_category_index<bq::decay_t<decltype(CAT)>>::value;
            static_assert(value != 0, "fast_*: this category does not belong to this log class");
        };
#endif

        struct ${CLASS_NAME}_category_config
        {
            ${CATEGORY_NAMES}
        };

        ${CATEGORY_ROOT_CLASS}

        template<typename T>
        struct ${CLASS_NAME}_category_root_holder
        {
            static ${CLASS_NAME}_category_config config_;
            static ${CLASS_NAME}_category_root root_;
        };

        template<typename T>
        struct ${CLASS_NAME}_zero_level_words_holder
        {
            static const uint32_t value[${CATEGORIES_COUNT}];
        };

    public:
#if defined(BQ_CPP_17)
        /// static constexpr so that cat.<category> is a compile-time constant: the fast interfaces
        /// take it as their first template argument and the fast path needs no run-time load for it.
        static constexpr ${CLASS_NAME}_category_root cat {};
#else
        const ${CLASS_NAME}_category_root& cat = ${CLASS_NAME}_category_root_holder<void>::root_;
#endif

    protected:
        template<typename STR>
        struct is_${CLASS_NAME}_format_type
        {
            static constexpr bool value = bq::tools::_is_bq_log_format_type<STR>::value;
        };

    private:
        ${CLASS_NAME}() : category_log()
        {
            set_zero_level_words(${CLASS_NAME}_zero_level_words_holder<void>::value);
        }
        ${CLASS_NAME}(const log& child_inst) : category_log(child_inst)
        {
            set_zero_level_words(${CLASS_NAME}_zero_level_words_holder<void>::value);
        }

    public:
        /// <summary>
        /// Create a ${CLASS_NAME} object
        /// </summary>
        /// <param name="log_name">If the log name is an empty string, bqLog will automatically assign you a unique log name. If the log name already exists, it will return the previously existing log object and overwrite the previous configuration with the new config.</param>
        /// <param name="config_content">Log config string</param>
        /// <returns>A ${CLASS_NAME} object, if create failed, the is_valid() method of it will return false</returns>
        static ${CLASS_NAME} create_log(const bq::string& log_name, const bq::string& config_content);

        /// <summary>
        /// Get a ${CLASS_NAME} object by it's name
        /// </summary>
        /// <param name="log_name">Name of the ${CLASS_NAME} object you want to find</param>
        /// <returns>A ${CLASS_NAME} object, if the ${CLASS_NAME} object with specific name was not found, the is_valid() method of it will return false</returns>
        static ${CLASS_NAME} get_log_by_name(const bq::string& log_name);

        using log::verbose;
        using log::debug;
        using log::info;
        using log::warning;
        using log::error;
        using log::fatal;

#if defined(BQ_CPP_20)
        using bq::log::fast_verbose;
        using bq::log::fast_debug;
        using bq::log::fast_info;
        using bq::log::fast_warning;
        using bq::log::fast_error;
        using bq::log::fast_fatal;

        /// Macro-free fast log. The category is the optional FIRST template argument and the
        /// format string the second:
        ///   log.fast_info<log.cat.ModuleA, "...">(...)   a category of this class
        ///   log.fast_info<"...">(...)                    the default (empty) category
        /// A site is shared by (format, level, category, wire types).
        /// A category of another log class is a compile-time error.
        template <auto CAT, bq::fixed_format F, typename... Args,
            bq::enable_if_t<bq::fast_info_detail::is_category<bq::decay_t<decltype(CAT)>>::value, bool> = true>
        bq_forceinline bool fast_verbose(const Args&... args) const
        {
            return bq::fast_info_detail::fast_impl<F, bq::log_level::verbose, fast_category_index<CAT>::value>(*this, args...);
        }
        template <auto CAT, bq::fixed_format F, typename... Args,
            bq::enable_if_t<bq::fast_info_detail::is_category<bq::decay_t<decltype(CAT)>>::value, bool> = true>
        bq_forceinline bool fast_debug(const Args&... args) const
        {
            return bq::fast_info_detail::fast_impl<F, bq::log_level::debug, fast_category_index<CAT>::value>(*this, args...);
        }
        template <auto CAT, bq::fixed_format F, typename... Args,
            bq::enable_if_t<bq::fast_info_detail::is_category<bq::decay_t<decltype(CAT)>>::value, bool> = true>
        bq_forceinline bool fast_info(const Args&... args) const
        {
            return bq::fast_info_detail::fast_impl<F, bq::log_level::info, fast_category_index<CAT>::value>(*this, args...);
        }
        template <auto CAT, bq::fixed_format F, typename... Args,
            bq::enable_if_t<bq::fast_info_detail::is_category<bq::decay_t<decltype(CAT)>>::value, bool> = true>
        bq_forceinline bool fast_warning(const Args&... args) const
        {
            return bq::fast_info_detail::fast_impl<F, bq::log_level::warning, fast_category_index<CAT>::value>(*this, args...);
        }
        template <auto CAT, bq::fixed_format F, typename... Args,
            bq::enable_if_t<bq::fast_info_detail::is_category<bq::decay_t<decltype(CAT)>>::value, bool> = true>
        bq_forceinline bool fast_error(const Args&... args) const
        {
            return bq::fast_info_detail::fast_impl<F, bq::log_level::error, fast_category_index<CAT>::value>(*this, args...);
        }
        template <auto CAT, bq::fixed_format F, typename... Args,
            bq::enable_if_t<bq::fast_info_detail::is_category<bq::decay_t<decltype(CAT)>>::value, bool> = true>
        bq_forceinline bool fast_fatal(const Args&... args) const
        {
            return bq::fast_info_detail::fast_impl<F, bq::log_level::fatal, fast_category_index<CAT>::value>(*this, args...);
        }
#endif

        ///Core log functions with category param, there are 6 log levels:
        ///verbose, debug, info, warning, error, fatal
        template<typename STR, uint32_t CAT_INDEX, bq::enable_if_t<bq::${CLASS_NAME}::is_${CLASS_NAME}_format_type<STR>::value, bool> = true>
        bool verbose(const ${CLASS_NAME}_category_base<CAT_INDEX>& category, const STR& log_content) const;
        template<typename STR, uint32_t CAT_INDEX, bq::enable_if_t<bq::${CLASS_NAME}::is_${CLASS_NAME}_format_type<STR>::value, bool> = true, typename...Args>
        bool verbose(const ${CLASS_NAME}_category_base<CAT_INDEX>& category, const STR& log_format_content, const Args&... args) const;
        template<typename STR, uint32_t CAT_INDEX, bq::enable_if_t<bq::${CLASS_NAME}::is_${CLASS_NAME}_format_type<STR>::value, bool> = true>
        bool debug(const ${CLASS_NAME}_category_base<CAT_INDEX>& category, const STR& log_content) const;
)"
            R"(        template<typename STR, uint32_t CAT_INDEX, bq::enable_if_t<bq::${CLASS_NAME}::is_${CLASS_NAME}_format_type<STR>::value, bool> = true, typename...Args>
        bool debug(const ${CLASS_NAME}_category_base<CAT_INDEX>& category, const STR& log_format_content, const Args&... args) const;
        template<typename STR, uint32_t CAT_INDEX, bq::enable_if_t<bq::${CLASS_NAME}::is_${CLASS_NAME}_format_type<STR>::value, bool> = true>
        bool info(const ${CLASS_NAME}_category_base<CAT_INDEX>& category, const STR& log_content) const;
        template<typename STR, uint32_t CAT_INDEX, bq::enable_if_t<bq::${CLASS_NAME}::is_${CLASS_NAME}_format_type<STR>::value, bool> = true, typename...Args>
        bool info(const ${CLASS_NAME}_category_base<CAT_INDEX>& category, const STR& log_format_content, const Args&... args) const;
        template<typename STR, uint32_t CAT_INDEX, bq::enable_if_t<bq::${CLASS_NAME}::is_${CLASS_NAME}_format_type<STR>::value, bool> = true>
        bool warning(const ${CLASS_NAME}_category_base<CAT_INDEX>& category, const STR& log_content) const;
        template<typename STR, uint32_t CAT_INDEX, bq::enable_if_t<bq::${CLASS_NAME}::is_${CLASS_NAME}_format_type<STR>::value, bool> = true, typename...Args>
        bool warning(const ${CLASS_NAME}_category_base<CAT_INDEX>& category, const STR& log_format_content, const Args&... args) const;
        template<typename STR, uint32_t CAT_INDEX, bq::enable_if_t<bq::${CLASS_NAME}::is_${CLASS_NAME}_format_type<STR>::value, bool> = true>
        bool error(const ${CLASS_NAME}_category_base<CAT_INDEX>& category, const STR& log_content) const;
        template<typename STR, uint32_t CAT_INDEX, bq::enable_if_t<bq::${CLASS_NAME}::is_${CLASS_NAME}_format_type<STR>::value, bool> = true, typename...Args>
        bool error(const ${CLASS_NAME}_category_base<CAT_INDEX>& category, const STR& log_format_content, const Args&... args) const;
        template<typename STR, uint32_t CAT_INDEX, bq::enable_if_t<bq::${CLASS_NAME}::is_${CLASS_NAME}_format_type<STR>::value, bool> = true>
        bool fatal(const ${CLASS_NAME}_category_base<CAT_INDEX>& category, const STR& log_content) const;
        template<typename STR, uint32_t CAT_INDEX, bq::enable_if_t<bq::${CLASS_NAME}::is_${CLASS_NAME}_format_type<STR>::value, bool> = true, typename...Args>
        bool fatal(const ${CLASS_NAME}_category_base<CAT_INDEX>& category, const STR& log_format_content, const Args&... args) const;
    };
    // Only declarations go inside the export block: a member definition may not sit there
    // (clang: "cannot export 'X' as it is not at namespace scope"), and none of them needs to.
    BQ_MODULE_EXPORT_END

    template<typename T>
    ${CLASS_NAME}::${CLASS_NAME}_category_root ${CLASS_NAME}::${CLASS_NAME}_category_root_holder<T>::root_;
    template<typename T>
    ${CLASS_NAME}::${CLASS_NAME}_category_config ${CLASS_NAME}::${CLASS_NAME}_category_root_holder<T>::config_;
    template<typename T>
    const uint32_t ${CLASS_NAME}::${CLASS_NAME}_zero_level_words_holder<T>::value[${CATEGORIES_COUNT}] = {};

    inline ${CLASS_NAME} ${CLASS_NAME}::create_log(const bq::string& log_name, const bq::string& config_content)
    {
        uint64_t log_id = api::__api_create_log(log_name.c_str(), config_content.c_str(), ${CATEGORIES_COUNT}, ${CLASS_NAME}_category_root_holder<void>::config_.names);
        log result = get_log_by_id(log_id);
        return result;
    }
    
    inline ${CLASS_NAME} ${CLASS_NAME}::get_log_by_name(const bq::string& log_name)
    {
        ${CLASS_NAME} result = log::get_log_by_name(log_name);
        if (!result.is_valid())
        {
            return result;
        }
        //check categories
        if (result.get_categories_count() != ${CATEGORIES_COUNT})
        {
            return ${CLASS_NAME}();
        }
        for (size_t i = 0; i < result.get_categories_count(); ++i)
        {
            if (result.get_categories_name_array()[i] != ${CLASS_NAME}_category_root_holder<void>::config_.names[i])
            {
                return ${CLASS_NAME}();
            }
        }
        return result;
    }

    template<typename STR, uint32_t CAT_INDEX, bq::enable_if_t<bq::${CLASS_NAME}::is_${CLASS_NAME}_format_type<STR>::value, bool>>
    inline bool ${CLASS_NAME}::verbose(const ${CLASS_NAME}::${CLASS_NAME}_category_base<CAT_INDEX>& category, const STR& log_content) const
    {
        (void)category;
        return do_log(CAT_INDEX, log_level::verbose, log_content);
    }
    template<typename STR, uint32_t CAT_INDEX, bq::enable_if_t<bq::${CLASS_NAME}::is_${CLASS_NAME}_format_type<STR>::value, bool>, typename...Args>
    inline bool ${CLASS_NAME}::verbose(const ${CLASS_NAME}::${CLASS_NAME}_category_base<CAT_INDEX>& category, const STR& log_format_content, const Args&... args) const
    {
        (void)category;
        return do_log(CAT_INDEX, log_level::verbose, log_format_content, args...);
    }
    template<typename STR, uint32_t CAT_INDEX, bq::enable_if_t<bq::${CLASS_NAME}::is_${CLASS_NAME}_format_type<STR>::value, bool>>
    inline bool ${CLASS_NAME}::debug(const ${CLASS_NAME}::${CLASS_NAME}_category_base<CAT_INDEX>& category, const STR& log_content) const
    {
        (void)category;
        return do_log(CAT_INDEX, log_level::debug, log_content);
    }
    template<typename STR, uint32_t CAT_INDEX, bq::enable_if_t<bq::${CLASS_NAME}::is_${CLASS_NAME}_format_type<STR>::value, bool>, typename...Args>
    inline bool ${CLASS_NAME}::debug(const ${CLASS_NAME}::${CLASS_NAME}_category_base<CAT_INDEX>& category, const STR& log_format_content, const Args&... args) const
    {
        (void)category;
        return do_log(CAT_INDEX, log_level::debug, log_format_content, args...);
    }
    template<typename STR, uint32_t CAT_INDEX, bq::enable_if_t<bq::${CLASS_NAME}::is_${CLASS_NAME}_format_type<STR>::value, bool>>
    inline bool ${CLASS_NAME}::info(const ${CLASS_NAME}::${CLASS_NAME}_category_base<CAT_INDEX>& category, const STR& log_content) const
    {
        (void)category;
        return do_log(CAT_INDEX, log_level::info, log_content);
    }
    template<typename STR, uint32_t CAT_INDEX, bq::enable_if_t<bq::${CLASS_NAME}::is_${CLASS_NAME}_format_type<STR>::value, bool>, typename...Args>
    inline bool ${CLASS_NAME}::info(const ${CLASS_NAME}::${CLASS_NAME}_category_base<CAT_INDEX>& category, const STR& log_format_content, const Args&... args) const
    {
        (void)category;
        return do_log(CAT_INDEX, log_level::info, log_format_content, args...);
    }
    template<typename STR, uint32_t CAT_INDEX, bq::enable_if_t<bq::${CLASS_NAME}::is_${CLASS_NAME}_format_type<STR>::value, bool>>
    inline bool ${CLASS_NAME}::warning(const ${CLASS_NAME}::${CLASS_NAME}_category_base<CAT_INDEX>& category, const STR& log_content) const
    {
        (void)category;
        return do_log(CAT_INDEX, log_level::warning, log_content);
    }
    template<typename STR, uint32_t CAT_INDEX, bq::enable_if_t<bq::${CLASS_NAME}::is_${CLASS_NAME}_format_type<STR>::value, bool>, typename...Args>
    inline bool ${CLASS_NAME}::warning(const ${CLASS_NAME}::${CLASS_NAME}_category_base<CAT_INDEX>& category, const STR& log_format_content, const Args&... args) const
    {
        (void)category;
        return do_log(CAT_INDEX, log_level::warning, log_format_content, args...);
    }
    template<typename STR, uint32_t CAT_INDEX, bq::enable_if_t<bq::${CLASS_NAME}::is_${CLASS_NAME}_format_type<STR>::value, bool>>
    inline bool ${CLASS_NAME}::error(const ${CLASS_NAME}::${CLASS_NAME}_category_base<CAT_INDEX>& category, const STR& log_content) const
    {
        (void)category;
        return do_log(CAT_INDEX, log_level::error, log_content);
    }
    template<typename STR, uint32_t CAT_INDEX, bq::enable_if_t<bq::${CLASS_NAME}::is_${CLASS_NAME}_format_type<STR>::value, bool>, typename...Args>
    inline bool ${CLASS_NAME}::error(const ${CLASS_NAME}::${CLASS_NAME}_category_base<CAT_INDEX>& category, const STR& log_format_content, const Args&... args) const
    {
        (void)category;
        return do_log(CAT_INDEX, log_level::error, log_format_content, args...);
    }
    template<typename STR, uint32_t CAT_INDEX, bq::enable_if_t<bq::${CLASS_NAME}::is_${CLASS_NAME}_format_type<STR>::value, bool>>
    inline bool ${CLASS_NAME}::fatal(const ${CLASS_NAME}::${CLASS_NAME}_category_base<CAT_INDEX>& category, const STR& log_content) const
    {
        (void)category;
        return do_log(CAT_INDEX, log_level::fatal, log_content);
    }
    template<typename STR, uint32_t CAT_INDEX, bq::enable_if_t<bq::${CLASS_NAME}::is_${CLASS_NAME}_format_type<STR>::value, bool>, typename...Args>
    inline bool ${CLASS_NAME}::fatal(const ${CLASS_NAME}::${CLASS_NAME}_category_base<CAT_INDEX>& category, const STR& log_format_content, const Args&... args) const
    {
        (void)category;
        return do_log(CAT_INDEX, log_level::fatal, log_format_content, args...);
    }
}
// clang-format on
        )";
    }

    bq::string category_log_template_cpp::get_module_template_content() const
    {
        return
            R"(// clang-format off
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
/*!
 * Generated module entry for ${CLASS_NAME}
 *
 * It exports the same class as ${CLASS_NAME}.h, so import it instead of including the header:
 *   import ${MODULE_NAME};
 *
 * The two entries are mutually exclusive: a translation unit may not import and include the
 * same library, so a project picks one of them.
 */
module;

// The header below is written in terms of these; they belong to the global module.
#include <stddef.h>
#include <stdint.h>

// A macro is not exported by a module, so the language-level macros the header uses
// (BQ_CPP_17, BQ_CPP_20, bq_forceinline, BQ_EBCO) come from this macro-only header.
#include "bq_common/platform/macros.h"

export module ${MODULE_NAME};

import bq.log;

// The header carries the whole class. These two markers turn its export block on, and because
// BQ_MODULE_EXPORT_BEGIN is defined while it is read, the header skips its own bq_log.h include:
// the library arrives through the import above. Everything the header defines out of class stays
// outside the export block, which is what C++20 requires.
#define BQ_MODULE_EXPORT_BEGIN export {
#define BQ_MODULE_EXPORT_END }
#include "${CLASS_NAME}.h"
#undef BQ_MODULE_EXPORT_BEGIN
#undef BQ_MODULE_EXPORT_END
// clang-format on
        )";
    }
}
