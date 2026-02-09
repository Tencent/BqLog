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

import { log_invoker } from "../impl/log_invoker";

export class log_category_base {
    protected index: number = 0;

    protected constructor(index: number) {
        this.index = index;
        log_invoker.__api_attach_category_base_inst(this);
    }
}