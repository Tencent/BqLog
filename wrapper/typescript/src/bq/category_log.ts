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
import { log } from "./log"
import { log_invoker } from "./impl/log_invoker";

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

    /**
     * Create a category log
     * @param name 
     *          If the log name is an empty string, bqLog will automatically assign you a unique log name. 
     * 			If the log name already exists, it will return the previously existing log object and overwrite the previous configuration with the new config.
     * @param config 
     * 			Log config string
     * @param categories_count 
     * @param categories 
     * @returns 
     * 			A log id, if create failed, 0 will be returned
     */
    protected static call_api_create_category_log(name: string, config: string, categories_count: number, categories: string[] | null): bigint {
        if (!config || config.length == 0) {
            return 0n;
        }
        let log_handle: bigint = log_invoker.__api_create_log(name, config, categories_count, categories);
        return log_handle;
    }
}