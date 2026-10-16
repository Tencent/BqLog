package bq.def;
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

/**
 * Type tag of one packed log argument. The tags mirror the C++ bq::log argument types: the
 * wrapper writes the ordinal of the tag in front of every value it packs.
 */
public enum log_arg_type_enum {
	/** Reserved; the wrapper never writes it. */
	unsupported_type,
    /** A null reference. */
    null_type,
    /** A raw pointer; the C++ wrapper produces it, the Java wrapper does not. */
    pointer_type,
    /** Boolean. */
    bool_type,
    /** Narrow character. */
    char_type,
    /** 16-bit character, what a Java char packs as. */
    char16_type,
    /** 32-bit character. */
    char32_type,
    /** 8-bit signed integer. */
    int8_type,
    /** 8-bit unsigned integer. */
    uint8_type,
    /** 16-bit signed integer. */
    int16_type,
    /** 16-bit unsigned integer. */
    uint16_type,
    /** 32-bit signed integer. */
    int32_type,
    /** 32-bit unsigned integer. */
    uint32_type,
    /** 64-bit signed integer. */
    int64_type,
    /** 64-bit unsigned integer. */
    uint64_type,
    /** 32-bit floating point. */
    float_type,
    /** 64-bit floating point. */
    double_type,
    /** UTF-8 string, what the C API packs. */
    string_utf8_type,
    /** UTF-16 string, what a Java String packs as. */
    string_utf16_type,
    /** UTF-32 string. */
    string_utf32_type,
    /** Mixed-width string (std::wstring, or wchar_t on Windows). */
    string_utf_mixed_type
}
