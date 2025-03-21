#pragma once
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
/*!
 * \file basic_types.h
 *
 * \author pippocao
 * \date 2022/07/14
 *
 */
namespace bq {
    enum class log_level {
        verbose,
        debug,
        info,
        warning,
        error,
        fatal,

        log_level_max = 32,
    };
}
