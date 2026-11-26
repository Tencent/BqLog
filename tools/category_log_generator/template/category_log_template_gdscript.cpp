/*
 * Copyright (C) 2026 Tencent.
 * BQLOG is licensed under the Apache License, Version 2.0.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */
#include "template/category_log_template_gdscript.h"

namespace bq {

    bq::string category_log_template_gdscript::get_node_type_name(const category_node& node) const
    {
        bq::string mangled = node.full_name().replace(".", "_");
        return "_Cat_" + mangled;
    }

    bq::string category_log_template_gdscript::format(const bq::string& template_string, const category_node& root_node) const
    {
        bq::string generated_code = template_string;
        generated_code = replace_with_tab_format(generated_code, "${CLASS_NAME}", class_name_);
        generated_code = replace_with_tab_format(generated_code, "${CATEGORY_NAMES}", get_category_names_code(root_node));
        generated_code = replace_with_tab_format(generated_code, "${CATEGORY_CLASSES}", get_category_classes_code(root_node));
        return generated_code;
    }

    bq::string category_log_template_gdscript::get_category_names_code_recursive(const category_node& node) const
    {
        bq::string code = "\t";
        if (node.parent()) {
            code += ", ";
        } else {
            code += "  ";
        }
        code += "\"" + node.full_name() + "\"\n";
        for (const auto& child : node.get_all_children()) {
            code += get_category_names_code_recursive(*child);
        }
        return code;
    }

    bq::string category_log_template_gdscript::get_category_names_code(const category_node& root_node) const
    {
        bq::string code = "PackedStringArray([\n";
        code += get_category_names_code_recursive(root_node);
        code += "])";
        return code;
    }

    bq::string category_log_template_gdscript::get_category_classes_code_recursive(const category_node& node, uint64_t& index) const
    {
        // Pre-order index assignment must match get_category_names_code_recursive.
        const uint64_t my_index = ++index;

        // Emit child classes first so every parent class can reference child types safely in GDScript.
        bq::string code;
        for (const auto& child : node.get_all_children()) {
            code += get_category_classes_code_recursive(*child, index);
        }

        const bq::string type_name = get_node_type_name(node);
        code += "class " + type_name + " extends BqLogCategory:\n";
        for (const auto& child : node.get_all_children()) {
            const bq::string child_type = get_node_type_name(*child);
            code += "\tvar " + child->name() + ": " + child_type + " = " + child_type + ".new()";
            if (!child->comment().trim().is_empty()) {
                code += " # " + child->comment().trim();
            }
            code += "\n";
        }
        code += "\tfunc _init() -> void:\n";
        code += "\t\tset_index(" + uint64_to_string(my_index) + ")\n\n";
        return code;
    }

    bq::string category_log_template_gdscript::get_category_classes_code(const category_node& root_node) const
    {
        bq::string code;
        uint64_t index = 0;
        for (const auto& child : root_node.get_all_children()) {
            code += get_category_classes_code_recursive(*child, index);
        }

        code += "class _CategoryRoot extends RefCounted:\n";
        if (root_node.get_all_children().is_empty()) {
            code += "\tpass\n";
        } else {
            for (const auto& child : root_node.get_all_children()) {
                const bq::string child_type = get_node_type_name(*child);
                code += "\tvar " + child->name() + ": " + child_type + " = " + child_type + ".new()";
                if (!child->comment().trim().is_empty()) {
                    code += " # " + child->comment().trim();
                }
                code += "\n";
            }
        }
        return code;
    }

    bq::string category_log_template_gdscript::get_template_content() const
    {
        return R"(# Copyright (C) 2026 Tencent.
# BQLOG is licensed under the Apache License, Version 2.0.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#
# Generated Wrapper For ${CLASS_NAME} (GDScript / Godot 4)
#
# Usage:
#   var my_log := ${CLASS_NAME}.create_log("my_log", config)
#   my_log.info("default category")
#   my_log.info(my_log.cat.ModuleA.SystemA, "with category {}", "ok")

class_name ${CLASS_NAME}
extends BqCategoryLog

${CATEGORY_CLASSES}

static var _category_names: PackedStringArray = ${CATEGORY_NAMES}

var cat: _CategoryRoot = _CategoryRoot.new()


func _init(p_log_id: int = 0) -> void:
	if p_log_id > 0:
		init_with_log_id(p_log_id)


static func create_log(p_name: String, p_config: String) -> ${CLASS_NAME}:
	var count := _category_names.size()
	var log_id := BqCategoryLog.call_api_create_category_log(p_name, p_config, count, _category_names)
	var result := ${CLASS_NAME}.new(log_id)
	if result.get_categories_count() != count:
		return ${CLASS_NAME}.new(0)
	return result


static func get_log_by_name(p_log_name: String) -> ${CLASS_NAME}:
	var base_log := BqLog.get_log_by_name(p_log_name)
	if base_log == null or not base_log.is_valid():
		return ${CLASS_NAME}.new(0)
	var result := ${CLASS_NAME}.new(base_log.get_id())
	if result.get_categories_count() != _category_names.size():
		return ${CLASS_NAME}.new(0)
	return result
)";
    }
}
