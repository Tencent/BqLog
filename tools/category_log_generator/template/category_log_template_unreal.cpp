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
#include "template/category_log_template_unreal.h"

namespace bq {
    bq::string category_log_template_unreal::format(const bq::string& template_string, const category_node& root_node) const
    {
        bq::string generated_code = template_string;
        generated_code = replace_with_tab_format(generated_code, "${CLASS_NAME}", class_name_);
        bq::string ue_class_name = class_name_;
        for (char& c : ue_class_name) {
            c = static_cast<char>(toupper(static_cast<int32_t>(c)));
        }
        generated_code = replace_with_tab_format(generated_code, "${UE_CLASS_NAME}", ue_class_name);
        generated_code = replace_with_tab_format(generated_code, "${CATEGORY_NAMES}", get_category_names_code(root_node));
        generated_code = replace_with_tab_format(generated_code, "${CATEGORY_ENUMS}", get_category_enums_code(root_node));
        generated_code = replace_with_tab_format(generated_code, "${CATEGORIES_COUNT}", uint64_to_string((uint64_t)root_node.get_all_nodes_count()));

        return generated_code;
    }

    bq::string category_log_template_unreal::get_category_names_code_recursive(const category_node& node) const
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

    bq::string category_log_template_unreal::get_category_enums_code_recursive(const category_node& node, const bq::string& enum_prefix) const
    {
        bq::string code;
        // empty means it is root node, default category
        bq::string enum_name = node.name().is_empty() ? "Default" : node.name();
        code += (enum_prefix + (enum_prefix.is_empty() ? "" : ".") + enum_name).replace(".", "_") + " " + "UMETA(DisplayName=\"" + (enum_prefix + (enum_prefix.is_empty() ? "" : ".") + node.name()) + "\"),\n";
        for (const auto& child : node.get_all_children()) {
            code += get_category_enums_code_recursive(*child, enum_prefix + (enum_prefix.is_empty() ? "" : ".") + node.name());
        }
        return code;
    }

    bq::string category_log_template_unreal::get_category_names_code(const category_node& root_node) const
    {
        size_t total_categories_count = root_node.get_all_nodes_count();
        bq::string code = "const char* category_names_[" + uint64_to_string((uint64_t)total_categories_count) + "] = {\n";
        code += get_category_names_code_recursive(root_node);
        code += "};";
        return code;
    }

    bq::string category_log_template_unreal::get_category_enums_code(const category_node& root_node) const
    {
        bq::string code = "";
        code += get_category_enums_code_recursive(root_node, "");
        code += "\n";
        return code;
    }

    bq::string category_log_template_unreal::get_template_content() const
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
 *  Create new Data Asset, set its parent class to ${UE_CLASS_NAME},
 *  and then you can reference it in BqLog node in Blue Print.
 */

#include "CoreMinimal.h"
#include "${CLASS_NAME}.h"
#include "BqLog.h"
#include "${CLASS_NAME}_for_UE.generated.h"



UENUM(BlueprintType)
enum class E${UE_CLASS_NAME} : uint8
{
    ${CATEGORY_ENUMS}
};


UCLASS(BlueprintType)
class NO_API U${UE_CLASS_NAME} : public UBqLog
{
    GENERATED_BODY()
public:
    
    virtual bool SupportsCategoryEnum() const override { return true; }
    virtual UEnum* GetCategoryEnum() const override { return StaticEnum<E${UE_CLASS_NAME}>(); }
private:
    ${CATEGORY_NAMES}
protected:
    virtual uint32_t get_category_count() const override {return ${CATEGORIES_COUNT}; }
    virtual const char* const* get_category_names() const override {return category_names_;}
};

// clang-format on
        )";
    }
}
