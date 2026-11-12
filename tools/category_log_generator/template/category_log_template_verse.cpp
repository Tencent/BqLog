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
#include "template/category_log_template_verse.h"

namespace bq {
    namespace {
        // Names a category field cannot have, because Verse does not let a field shadow a
        // definition in scope (as of UE 6.0):
        //   - the keywords and reserved words of the Verse compiler (VerseCompiler's
        //     uLang/Parser/VerseGrammar.h and uLang/Parser/ReservedSymbols.inl);
        //   - the other built-in types and specifiers;
        //   - what /Verse.org/Verse defines, which every Verse file sees: the compiler's
        //     built-ins and Engine/Plugins/Verse/Verse/Source/Verse/Verse/Verse;
        //   - index, which a category with categories below it inherits from category_base;
        //   - the definitions of the generated module, bq (the module of /Tencent.com/BqLog,
        //     which the generated file uses) and BqLog itself, which code under /Tencent.com
        //     sees.
        // The name of the generated module is taken too; see is_taken.
        const char* const verse_taken_names[] = {
            "_", "abstract", "abstracts", "alias", "allocates", "and", "any", "array", "as", "assert", "at",
            "await", "backslash", "bag", "batch", "block", "branch", "break", "case", "castable_concrete_subtype",
            "castable_subtype", "catch", "char16", "char32", "char8", "class", "closed", "collection", "computes",
            "concrete_subtype", "const", "constructor", "continue", "contravariant", "converges", "covariant",
            "decides", "defer", "dictate", "do", "element", "else", "embargo", "ensure", "enum", "external",
            "fails", "false", "final", "find", "first", "float", "float128", "float16", "float32", "float64",
            "fold", "for", "function", "generator", "given", "guard", "if", "implies", "import", "in", "instance",
            "int", "int16", "int32", "int64", "int8", "interacts", "interface", "intrinsic", "introspects",
            "invariant", "is", "iterates", "known", "last", "let", "live", "local", "logic", "loop", "map",
            "markup", "module", "mutable", "nat", "nat16", "nat32", "nat64", "nat8", "native", "native_callable",
            "next", "not", "of", "open", "option", "or", "over", "permutation", "persistable", "profile",
            "provided", "race", "random", "reads", "ref", "repeat", "return", "rush", "scoped", "Self",
            "sequence", "set", "spawn", "string", "string16", "string32", "string8", "struct", "subclass",
            "subtype", "succeeds", "super", "suspends", "sync", "syntax", "then", "throws", "to", "trait",
            "transacts", "true", "truth", "try", "tuple", "type", "union", "until", "upon", "using", "var",
            "varies", "verify", "void", "when", "where", "while", "with", "writes", "yield",

            "castable", "char", "comparable", "concrete", "final_super", "internal", "localizes", "override",
            "predicts", "private", "protected", "public", "rational", "self", "task", "unique", "weak_map",

            "Abs", "BitAnd", "BitNot", "BitOr", "BitShiftLeft", "BitShiftRight", "BitXor", "ConcatenateMaps",
            "FitsInPlayerMap", "Inf", "NaN",
            "accessor", "ArcCos", "ArCosh", "ArcSin", "ArcTan", "ArSinh", "ArTanh", "CanCallerAccessEpicInternal",
            "cancelable", "Ceil", "Clamp", "classifiable_subset", "classifiable_subset_key",
            "classifiable_subset_var", "clock", "Concatenate", "Cos", "Cosh", "diagnosable", "diagnostic",
            "disposable", "Easing", "enableable", "Err", "error_result", "event", "event_subscription", "Exp",
            "Floor", "GetCastableFinalSuperClass", "GetCastableFinalSuperClassFromType", "GetSecondsSinceEpoch",
            "Int", "invalidatable", "IsAlmostEqual", "Join", "Lerp", "listenable", "Ln", "locale",
            "localizable_float", "localizable_int", "localizable_message", "localizable_string",
            "localizable_value", "Localize", "Log", "MakeClassifiableSubset", "MakeClassifiableSubsetVar",
            "MakeError", "MakeLocalizableValue", "MakeMessageLiteral", "MakeProxyListenable", "MakeSuccess", "Max",
            "message", "Min", "Mod", "PiFloat", "playable", "playable_stop_reason", "Pow", "proxy_listenable",
            "Quotient", "result", "Round", "seconds_since_epoch_clock", "SecondsSinceEpochClock", "Sgn",
            "showable", "signalable", "Sin", "Sinh", "Sqrt", "subscribable", "success_result", "Tan", "Tanh",
            "ToDiagnostic", "ToString",

            "index",

            "create_log", "get_log_by_name", "log", "category_base", "category_root", "category_names", "categories",
            "bq", "BqLog"
        };

        // Whether name reads as a category class name, category_<length>_<segment>... as
        // node_class_name makes them, so that it may be the class of another category.
        bool has_node_class_form(const bq::string& name)
        {
            const bq::string prefix = "category";
            if (name.size() == prefix.size() || !name.begin_with(prefix)) {
                return false;
            }
            size_t pos = prefix.size();
            while (pos < name.size()) {
                if (name[pos] != '_') {
                    return false;
                }
                ++pos;
                const size_t digits = pos;
                size_t length = 0;
                while (pos < name.size() && name[pos] >= '0' && name[pos] <= '9' && length <= name.size()) {
                    length = length * 10U + static_cast<size_t>(name[pos] - '0');
                    ++pos;
                }
                if (pos == digits || pos == name.size() || name[pos] != '_') {
                    return false;
                }
                ++pos;
                if (length == 0 || length > name.size() - pos) {
                    return false;
                }
                pos += length;
            }
            return true;
        }

        bq::string strip_trailing_newline(bq::string code)
        {
            while (!code.is_empty() && code[code.size() - 1] == '\n') {
                code = code.substr(0, code.size() - 1);
            }
            return code;
        }

        // block with each line that has text in it moved one level (four spaces) right.
        bq::string indent(const bq::string& block)
        {
            bq::string result;
            size_t start = 0;
            while (start < block.size()) {
                size_t end = block.find("\n", start);
                if (end == bq::string::npos) {
                    end = block.size();
                }
                if (end > start) {
                    result += "    ";
                }
                result += block.substr(start, end - start);
                if (end < block.size()) {
                    result += "\n";
                }
                start = end + 1;
            }
            return result;
        }
    }

    bool category_log_template_verse::is_taken(const bq::string& name) const
    {
        if (name == class_name_) {
            return true;
        }
        for (const char* taken : verse_taken_names) {
            if (name == taken) {
                return true;
            }
        }
        return has_node_class_form(name);
    }

    bq::array<bq::string> category_log_template_verse::child_field_names(const category_node& node) const
    {
        bq::array<bq::string> names;
        for (const auto& child : node.get_all_children()) {
            bq::string name = child->name();
            for (;;) {
                bool taken = is_taken(name);
                for (const auto& used : names) {
                    taken = taken || (used == name);
                }
                if (!taken) {
                    break;
                }
                name += "_";
            }
            names.push_back(name);
        }
        return names;
    }

    bq::string category_log_template_verse::node_class_name(const category_node& node) const
    {
        // Length-prefixed segments keep A.B_C and A_B.C apart.
        bq::string name = "category";
        for (const auto& segment : node.full_name().split(".")) {
            name += "_" + uint64_to_string(static_cast<uint64_t>(segment.size())) + "_" + segment;
        }
        return name;
    }

    bq::string category_log_template_verse::field_type(const category_node& node) const
    {
        return node.get_all_children().size() > 0 ? node_class_name(node) : bq::string("category_base");
    }

    bq::string category_log_template_verse::field_comment(const category_node& node, const bq::string& field_name)
    {
        bq::string comment;
        if (field_name != node.name() || !node.comment().trim().is_empty()) {
            comment = " # " + node.full_name();
            bq::string text = node.comment().trim();
            if (!text.is_empty()) {
                // Keep the comment from opening or closing a block comment.
                comment += " - " + text.replace("<#", "< #").replace("#>", "# >");
            }
        }
        return comment;
    }

    bq::string category_log_template_verse::category_names(const category_node& node)
    {
        bq::string code = "\"" + node.full_name() + "\"\n";
        for (const auto& child : node.get_all_children()) {
            code += category_names(*child);
        }
        return code;
    }

    bq::string category_log_template_verse::category_classes(const category_node& node) const
    {
        const auto& children = node.get_all_children();
        bq::string code;
        if (!node.parent()) {
            code += "# The top-level categories of " + class_name_ + ".\n";
            code += "category_root<public> := class<computes><internal>";
        } else {
            code += "# Category " + node.full_name() + ", with the categories below it.\n";
            code += node_class_name(node) + "<public> := class<computes><internal>(category_base)";
        }
        if (children.size() == 0) {
            return code + "{}\n";
        }
        code += ":\n";
        const bq::array<bq::string> names = child_field_names(node);
        for (size_t i = 0; i < children.size(); ++i) {
            const category_node& child = *children[i];
            code += "    " + names[i] + "<public>:" + field_type(child) + field_comment(child, names[i]) + "\n";
        }
        for (const auto& child : children) {
            if (child->get_all_children().size() > 0) {
                code += "\n" + category_classes(*child);
            }
        }
        return code;
    }

    bq::string category_log_template_verse::category_values(const category_node& node, uint64_t& index) const
    {
        const auto& children = node.get_all_children();
        const bq::array<bq::string> names = child_field_names(node);
        bq::string code;
        for (size_t i = 0; i < children.size(); ++i) {
            const category_node& child = *children[i];
            const bq::string child_index = uint64_to_string(++index);
            if (child.get_all_children().size() == 0) {
                code += names[i] + " := category_base{index := " + child_index + "}\n";
                continue;
            }
            code += names[i] + " := " + node_class_name(child) + ":\n";
            code += indent("index := " + child_index + "\n" + category_values(child, index));
        }
        return code;
    }

    bq::string category_log_template_verse::format(const bq::string& template_string, const category_node& root_node) const
    {
        bq::string root_value = "categories:category_root = category_root{}";
        if (root_node.get_all_children().size() > 0) {
            uint64_t index = 0;
            root_value = "categories:category_root = category_root:\n" + strip_trailing_newline(indent(category_values(root_node, index)));
        }

        bq::string code = template_string;
        code = replace_with_tab_format(code, "${CATEGORY_NAMES}", strip_trailing_newline(category_names(root_node)));
        code = replace_with_tab_format(code, "${CATEGORY_CLASSES}", strip_trailing_newline(category_classes(root_node)));
        code = replace_with_tab_format(code, "${CATEGORY_VALUES}", root_value);
        code = code.replace("${CLASS_NAME}", class_name_);

        // Blank lines inside indented replacements come out as spaces; drop trailing blanks.
        bq::string result;
        size_t start = 0;
        while (start < code.size()) {
            size_t end = code.find("\n", start);
            const bool has_newline = (end != bq::string::npos);
            if (!has_newline) {
                end = code.size();
            }
            size_t stop = end;
            while (stop > start && (code[stop - 1] == ' ' || code[stop - 1] == '\t')) {
                --stop;
            }
            result += code.substr(start, stop - start);
            if (has_newline) {
                result += "\n";
            }
            start = end + 1;
        }
        return result;
    }

    bq::string category_log_template_verse::get_template_content() const
    {
        return R"(# Code generated by BqLog_CategoryLogGenerator; DO NOT EDIT.
# Copyright (C) 2025 Tencent.
# BQLOG is licensed under the Apache License, Version 2.0.
#
# Category log ${CLASS_NAME} for Verse, on UE6 with the BqLogVerse plugin. Put this file in a
# Verse folder of your project or plugin; the module ${CLASS_NAME} is then part of the Verse
# module of that folder.
#
# Usage:
#     if (MyLog := ${CLASS_NAME}.create_log["MyLog", Config]):
#         MyLog.info("in the default category")
#         MyLog.info(MyLog.cat.ModuleA.SystemA, "in category ModuleA.SystemA")
#
#     # Elsewhere, the same log, whoever created it: Verse, C++ (${CLASS_NAME}::create_log),
#     # Blueprint or a BqLog asset.
#     if (MyLog := ${CLASS_NAME}.get_log_by_name["MyLog"]):
#         MyLog.warning(MyLog.cat.ModuleA, "found it")
#
# ${CLASS_NAME}.log is a bq.category_log of /Tencent.com/BqLog, so every function of bq.log
# works on it too. A category whose name is already taken in Verse gets a field name ending in
# "_"; the comment beside the field names the category. Do not call a variable Log: the editor
# reports it as ambiguous with the function Log of /Verse.org/Verse.

using { /Tencent.com/BqLog }

${CLASS_NAME}<public> := module:

    # Creates the log named Name from Config, with the categories of ${CLASS_NAME}, like
    # ${CLASS_NAME}::create_log in C++. If a log of that name exists with the same categories,
    # Config is applied to it instead. An empty Name gets a generated one. Fails if Config
    # cannot be parsed, or if a log of that name exists with other categories.
    create_log<public>(Name:string, Config:string)<transacts><decides>:log =
        log{}.create_and_bind_categories[Name, Config]

    # The log named Name, whoever created it. Fails if there is none, or if its categories are
    # not those of ${CLASS_NAME}.
    get_log_by_name<public>(Name:string)<reads><decides>:log =
        log{}.find_and_bind_categories[Name]

    # A ${CLASS_NAME} log. The level functions inherited from bq.log write in the default
    # category; those below write in the category given first.
    log<public> := class<computes><internal>(bq.category_log):

        # The categories: MyLog.info(MyLog.cat.ModuleA.SystemA, "...") writes in ModuleA.SystemA.
        cat<public>:category_root = categories

        # The levels are those of bq::log_level: 0 verbose ... 5 fatal.
        verbose<public>(Category:category_base, Message:string)<computes>:void = do_log(Category.index, 0, Message)
        debug<public>(Category:category_base, Message:string)<computes>:void = do_log(Category.index, 1, Message)
        info<public>(Category:category_base, Message:string)<computes>:void = do_log(Category.index, 2, Message)
        warning<public>(Category:category_base, Message:string)<computes>:void = do_log(Category.index, 3, Message)
        error<public>(Category:category_base, Message:string)<computes>:void = do_log(Category.index, 4, Message)
        fatal<public>(Category:category_base, Message:string)<computes>:void = do_log(Category.index, 5, Message)

        create_and_bind_categories<internal>(Name:string, Config:string)<transacts><decides>:log =
            create_and_bind[Name, Config, category_names]
            Self

        find_and_bind_categories<internal>(Name:string)<reads><decides>:log =
            find_and_bind[Name, category_names]
            Self

    # A category of ${CLASS_NAME}, one of those in log.cat.
    category_base<public> := class<computes><internal>:
        index<internal>:int

    ${CATEGORY_CLASSES}

    # Every category in index order, the default category "" first.
    category_names:[]string = array:
        ${CATEGORY_NAMES}

    ${CATEGORY_VALUES}
)";
    }
}
