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
import { log } from "./log"

export class category_log extends log {
    protected constructor(arg: category_log | bigint) {
        super(arg);
    }

    /**
     * Get log categories count
     * @return
     */
    public get_categories_count(): number {
        return this.categories_name_array_.length;
    }

    /**
     * Get names of all categories
     * @return
     */
    public get_categories_name_array(): string[] {
        return this.categories_name_array_;
    }
}