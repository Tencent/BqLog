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
#include "bq_log/log/decoder/appender_decoder_base.h"

namespace bq {
    class appender_decoder_raw : public appender_decoder_base {
    protected:
        virtual appender_decode_result init_private() override;

        virtual appender_decode_result decode_private() override;

        virtual uint32_t get_binary_format_version() const override;
    };
}