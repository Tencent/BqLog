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
import { log_level } from "./def/log_level";
import { log_invoker } from "./impl/log_invoker";
// IMPORTANT: this is a build-alias file generated per build.
// For ESM build it is a copy of lib_loader.esm.txt,
// for CJS and OHOS build it is lib_loader.ts.
import { native_export } from "./utils/lib_loader"

export type console_callback = (
    log_id: bigint,
    category_idx: number,
    level: number,
    text: string
) => void;
export class log {
    private log_id_: bigint = 0n;
    private name_: string = "";
    protected categories_name_array_: Array<string> = [];
    protected static callback_: console_callback | null = null;

    /**
     * Get bqLog lib version
     * @return
     */
    public static get_version(): string {
        return log_invoker.__api_get_log_version();
    }

    /**
     * Reset the base dir
     * @param in_sandbox
     * @param dir
     */
    public static reset_base_dir(in_sandbox: boolean, dir: string): void {
        log_invoker.__api_reset_base_dir(in_sandbox, dir);
    }

    /**
     * If bqLog is asynchronous, a crash in the program may cause the logs in the buffer not to be persisted to disk. 
     * If this feature is enabled, bqLog will attempt to perform a forced flush of the logs in the buffer in the event of a crash. However, 
     * this functionality does not guarantee success.
     */
    public static enable_auto_crash_handle(): void {
        log_invoker.__api_enable_auto_crash_handler();
    }

    /**
     * If bqLog is stored in a relative path, it will choose whether the relative path is within the sandbox or not.
     * This will return the absolute paths corresponding to both scenarios.
     * @param is_in_sandbox
     * @return
     */
    public static get_file_base_dir(is_in_sandbox: boolean): string {
        return log_invoker.__api_get_file_base_dir(is_in_sandbox);
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
        let log_id: bigint = log_invoker.__api_create_log(name, config, 0, null);
        return new log(log_id);
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
                return new log(id);
            }
        }
        return new log(0n);
    }

    /**
     * Synchronously flush the buffer of all log objects
     * to ensure that all data in the buffer is processed after the call.
     */
    public static force_flush_all_logs(): void {
        log_invoker.__api_force_flush(0n);
    }
    /**
     * Register a callback that will be invoked whenever a console log message is output. 
     * This can be used for an external system to monitor console log output.
     * @param callback
     */
    public static register_console_callback(callback: console_callback): void {
        log_invoker.__api_set_console_callback(callback);
        log.callback_ = callback;
    }

    /**
     * Unregister a previously registered console callback.
     * @param callback
     */
    public static unregister_console_callback(callback: console_callback): void {
        if (log.callback_ == callback) {
            log_invoker.__api_set_console_callback();
        }
    }

    /**
     * Enable or disable the console appender buffer. 
     * Since our wrapper may run in both C# and Java virtual machines, and we do not want to directly invoke callbacks from a native thread, 
     * we can enable this option. This way, all console outputs will be saved in the buffer until we fetch them.
     * @param enable
     */
    public static set_console_buffer_enable(enable: boolean): void {
        log_invoker.__api_set_console_buffer_enable(enable);
    }

    /**
     * Fetch and remove a log entry from the console appender buffer in a thread-safe manner. 
     * If the console appender buffer is not empty, the on_console_callback function will be invoked for this log entry. 
     * Please ensure not to output synchronized BQ logs within the callback function.
     * @param on_console_callback
     *        A callback function to be invoked for the fetched log entry if the console appender buffer is not empty
     * @return
     *        True if the console appender buffer is not empty and a log entry is fetched; otherwise False is returned.
     */
    public static fetch_and_remove_console_buffer(on_console_callback: console_callback): boolean {
        return log_invoker.__api_fetch_and_remove_console_buffer(on_console_callback);
    }

    /**
    * Output to console with log_level.
    * Important: This is not log entry, and can not be caught by console callback which was registered by register_console_callback or fetch_and_remove_console_buffer
    * @param level
    * @param str
    */
    public static console(level: log_level, str: string): void {
        log_invoker.__api_log_device_console(level, str);
    }

    protected constructor(arg: log | bigint) {
        if (arg instanceof log) {
            this.name_ = arg.name_;
            this.log_id_ = arg.log_id_;

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
            let name = log_invoker.__api_get_log_name_by_id(this.log_id_);
            if (!name) {
                this.log_id_ = 0n;
                return;
            }
            this.name_ = name;
            let category_count = log_invoker.__api_get_log_categories_count(this.log_id_);
            this.categories_name_array_ = [];
            for (let i = 0; i < category_count; ++i) {
                let category_item_name = log_invoker.__api_get_log_category_name_by_index(this.log_id_, i);
                this.categories_name_array_.push(category_item_name as string);
            }
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

    /**
     * Whether a log object is valid
     * @return
     */
    public is_valid(): boolean {
        return this.get_id() != 0n;
    }

    /**
     * Get the name of a log
     * @return
     */
    public get_name(): string {
        return this.name_;
    }

    /**
     * Works only when snapshot is configured.
     * It will decode the snapshot buffer to text.
     * @param use_gmt_time
     * 			Whether the timestamp of each log is GMT time or local time
     * @return
     * 			The decoded snapshot buffer
     */
    public take_snapshot(use_gmt_time: boolean): string {
        return log_invoker.__api_take_snapshot_string(this.log_id_, use_gmt_time);
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