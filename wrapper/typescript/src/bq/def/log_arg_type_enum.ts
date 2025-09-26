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

export enum log_arg_type_enum {
    unsupported_type, //means undefined in typescript wrapper
    null_type,
    pointer_type,   //means object in typescript wrapper
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