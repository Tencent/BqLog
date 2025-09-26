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
import { log_invoker, native_export } from "./impl/log_invoker";

export class log {
    private log_id_: bigint = 0n;
    private name_: string = "";
    private merged_log_level_bitmap_: DataView | null = null;
    private categories_mask_array_: DataView | null = null;
    private print_stack_level_bitmap_: DataView | null = null;
    private categories_name_array_: Array<string> = [];

    protected static get_log_by_id(log_id: bigint): log {
        let log_inst: log = new log(log_id);
        let name = log_invoker.__api_get_log_name_by_id(log_id);
        if (null == name) {
            return log_inst;
        }
        log_inst.name_ = name;
        log_inst.merged_log_level_bitmap_ = log_invoker.__api_get_log_merged_log_level_bitmap_by_log_id(log_id);
        log_inst.categories_mask_array_ = log_invoker.__api_get_log_category_masks_array_by_log_id(log_id);
        log_inst.print_stack_level_bitmap_ = log_invoker.__api_get_log_print_stack_level_bitmap_by_log_id(log_id);

        let category_count = log_invoker.__api_get_log_categories_count(log_id);
        log_inst.categories_name_array_ = [];
        for (let i = 0; i < category_count; ++i) {
            let category_item_name = log_invoker.__api_get_log_category_name_by_index(log_id, i);
            if (category_item_name) {
                log_inst.categories_name_array_.push(category_item_name);
            }
        }
        return log_inst;
    }

    /**
     * Create a log object
     * @param name 
     * 		  	If the log name is an empty string, bqLog will automatically assign you a unique log name. 
     * 			If the log name already exists, it will return the previously existing log object and overwrite the previous configuration with the new config.
     * @param config
     * 			Log config string
     * @return
     * 			A log object, if create failed, the is_valid() method of it will return false
     */
    public static create_log(name: string, config: string): log {
        if (!config || config.length == 0) {
            return new log(0n);
        }
        let log_handle: bigint = log_invoker.__api_create_log(name, config, 0, null);
        let result: log = log.get_log_by_id(log_handle);
        return result;
    }

    /**
     * Get a log object by it's name
     * @param log_name
     * 			Name of the log you want to find
     * @return
     * 			A log object, if the log object with specific name was not found, the is_valid() method of it will return false
     */
    public static get_log_by_name(log_name: string): log {
        if (!log_name || log_name.length == 0) {
            return new log(0n);
        }
        let log_count = log_invoker.__api_get_logs_count();
        for (let i = 0; i < log_count; ++i) {
            let id = log_invoker.__api_get_log_id_by_index(i);
            let name = log_invoker.__api_get_log_name_by_id(id);
            if (log_name == name) {
                return log.get_log_by_id(id);
            }
        }
        return new log(0n);
    }

    public constructor(arg?: log | bigint) {
        if (arg instanceof log) {
            this.merged_log_level_bitmap_ = arg.merged_log_level_bitmap_;
            this.name_ = arg.name_;
            this.log_id_ = arg.log_id_;
            this.merged_log_level_bitmap_ = log_invoker.__api_get_log_merged_log_level_bitmap_by_log_id(this.log_id_);
            this.categories_mask_array_ = log_invoker.__api_get_log_category_masks_array_by_log_id(this.log_id_);
            this.print_stack_level_bitmap_ = log_invoker.__api_get_log_print_stack_level_bitmap_by_log_id(this.log_id_);

            let category_count = log_invoker.__api_get_log_categories_count(this.log_id_);
            this.categories_name_array_ = [];
            for (let i = 0; i < category_count; ++i) {
                let category_item_name = log_invoker.__api_get_log_category_name_by_index(this.log_id_, i);
                if (category_item_name) {
                    this.categories_name_array_.push(category_item_name);
                }
            }
        } else if (typeof (arg) === 'bigint') {
            this.log_id_ = arg;
        }
        log_invoker.__api_attach_log_inst(this);
    }

    /**
     * Modify the log configuration, but some fields, such as buffer_size, cannot be modified.
     * @param config 
     */
    public reset_config(config: string): void {
        if (!config || config.length == 0) {
            return;
        }
        log_invoker.__api_log_reset_config(this.name_, config);
    }

    /**
     * Temporarily disable or enable a specific Appender.
     * @param appender_name
     * @param enable 
     */
    public set_appenders_enable(appender_name: string, enable: boolean): void {
        log_invoker.__api_set_appenders_enable(this.log_id_, appender_name, enable);
    }

    /**
     * Synchronously flush the buffer of this log object
     * to ensure that all data in the buffer is processed after the call.
     */
    public force_flush(): void {
        log_invoker.__api_force_flush(this.log_id_);
    }

    /**
     * Get id of this log object
     * @return
     */
    public get_id(): bigint {
        return this.log_id_;
    }

    public declare verbose: (log_format_content: string, ...args: unknown[]) => boolean;
    public declare debug: (log_format_content: string, ...args: unknown[]) => boolean;
    public declare info: (log_format_content: string, ...args: unknown[]) => boolean;
    public declare warning: (log_format_content: string, ...args: unknown[]) => boolean;
    public declare error: (log_format_content: string, ...args: unknown[]) => boolean;
    public declare fatal: (log_format_content: string, ...args: unknown[]) => boolean;
}

function defineNativeMethod(proto: any, name: string) {
    const fn = native_export(name);
    if (typeof fn !== 'function') {
        throw new Error(`Native method "__bq_napi_${name}" not found or not a function`);
    }
    Object.defineProperty(proto, name, {
        value: fn,
        writable: false,
        enumerable: false,
        configurable: false,
    });
}

['verbose', 'debug', 'info', 'warning', 'error', 'fatal'].forEach(n => defineNativeMethod(log.prototype, n));