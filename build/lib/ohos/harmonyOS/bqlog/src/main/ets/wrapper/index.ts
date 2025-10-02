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
import { log_level } from "./bq/def/log_level";
import { log } from "./bq/log";
import { category_log } from "./bq/category_log";
import { log_category_base } from "./bq/def/log_category_base"
import { log_decoder, appender_decode_result } from "./bq/tools/log_decoder"

export const bq = {
    log: log,
    category_log: category_log,
    log_category_base: log_category_base,
    log_level: log_level,
    log_decoder: log_decoder,
    appender_decode_result: appender_decode_result
} as const;