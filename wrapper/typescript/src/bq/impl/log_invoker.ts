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

/*
  typescript invoker that mirrors the java log_invoker native signatures.
  - automatic native loading for node.js and openharmony/harmony next
  - function and variable names use snake_case
  - exports __api_* methods that map 1:1 to the c++ napi functions
  - compatible with "__bq_napi_" export name prefix
*/

import { string_holder } from "../def/string_holder"
import { lib_def } from "../lib_def";
import { detect_runtime, runtime_kind } from "../utils/env_detector";
import { load_bq_log_native, native_binding } from "../utils/lib_loader";

export type console_callback = (
    log_id: bigint,
    category_idx: number,
    level: number,
    text: string
) => void;

let _native_mod: native_binding | null = null;

export function native_export(name: string): any {
    return _native_mod?.[`__bq_napi_${name}`];
}

function as_bigint(v: unknown, def: bigint = 0n): bigint {
    return typeof v === "bigint" ? v : def;
}

function dump_native_exports(mod: native_binding, prefix = "__bq_napi_") {
    try {
        const keys = Reflect.ownKeys(mod)
            .filter((k): k is string => typeof k === "string" && k.startsWith(prefix))
            .sort();

        const items = keys.map(k => {
            const v = (mod as any)[k];
            return {
                key: k,
                name: k.slice(prefix.length),
                type: typeof v,
                arity: typeof v === "function" ? v.length : undefined,
            };
        });

        const title = `[bqlog] native exports (${items.length})`;
        if (typeof console.groupCollapsed === "function") console.groupCollapsed(title);
        else console.log(title);

        for (const it of items) {
            console.log(`${it.key} -> ${it.type}${it.arity != null ? ` (arity=${it.arity})` : ""}`);
        }
        if (typeof console.groupEnd === "function") console.groupEnd();

    } catch (err) {
        console.warn("[bqlog] Failed to dump native exports:", err);
    }
}

// auto load on module initialization, similar to java static block
(function auto_load() {
    try {
        _native_mod = load_bq_log_native();
        //dump_native_exports(_native_mod);
    } catch (e) {
        // eslint-disable-next-line no-console
        console.error(`failed to load ${lib_def.lib_name}`);
        // eslint-disable-next-line no-console
        console.error((e as Error)?.message ?? String(e));
        const k: runtime_kind = detect_runtime();
        // eslint-disable-next-line no-console
        console.error(`runtime: ${k}`);
    }
})();

export class log_invoker {
    public static __api_get_log_version(): string {
        return native_export("get_log_version")();
    }

    public static __api_enable_auto_crash_handler(): void {
        native_export("enable_auto_crash_handler")();
    }

    public static __api_create_log(log_name: string, config_content: string, category_count: number, category_names_array: string[] | null): bigint {
        const id = native_export("create_log")(log_name, config_content, category_count, category_names_array);
        return as_bigint(id);
    }

    public static __api_log_reset_config(log_name: string, config_content: string): void {
        native_export("log_reset_config")(log_name, config_content);
    }

    public static __api_attach_log_inst(log_inst: any): void {
        return native_export("attach_log_inst")(log_inst, log_inst['log_id_']);
    }

    public static __api_attach_category_base_inst(category_inst: any): void {
        return native_export("category_base_inst")(category_inst, category_inst['index']);
    }

    public static __api_set_appenders_enable(log_id: bigint, appender_name: string, enable: boolean): void {
        native_export("set_appenders_enable")(log_id, appender_name, !!enable);
    }

    public static __api_get_logs_count(): number {
        return native_export("get_logs_count")();
    }

    public static __api_get_log_id_by_index(index: number): bigint {
        return as_bigint(native_export("get_log_id_by_index")(index));
    }

    public static __api_get_log_name_by_id(log_id: bigint): string | null {
        return native_export("get_log_name_by_id")(log_id);
    }

    public static __api_get_log_categories_count(log_id: bigint): number {
        return native_export("get_log_categories_count")(log_id);
    }

    public static __api_get_log_category_name_by_index(log_id: bigint, category_index: number): string | null {
        return native_export("get_log_category_name_by_index")(log_id, category_index);
    }

    public static __api_get_log_merged_log_level_bitmap_by_log_id(log_id: bigint): DataView | null {
        return native_export("get_log_merged_log_level_bitmap_by_log_id")(log_id);
    }

    public static __api_get_log_category_masks_array_by_log_id(log_id: bigint): DataView | null {
        return native_export("get_log_category_masks_array_by_log_id")(log_id);
    }

    public static __api_get_log_print_stack_level_bitmap_by_log_id(log_id: bigint): DataView | null {
        return native_export("get_log_print_stack_level_bitmap_by_log_id")(log_id);
    }

    public static __api_log_device_console(level: number, content: string): void {
        native_export("log_device_console")(level, content);
    }

    public static __api_force_flush(log_id: bigint): void {
        native_export("force_flush")(log_id);
    }

    public static __api_get_file_base_dir(is_in_sandbox: boolean): string {
        return native_export("get_file_base_dir")(!!is_in_sandbox) as string;
    }

    public static __api_log_decoder_create(log_file_path: string, priv_key: string): bigint {
        return as_bigint(native_export("log_decoder_create")(log_file_path, priv_key));
    }

    public static __api_log_decoder_decode(handle: bigint, out: string_holder): number {
        const obj = native_export("log_decoder_decode")(handle) as {
            code: number;
            text: string;
        };
        if (out && typeof out === "object") {
            out.text = obj?.text ?? "";
        }
        return obj?.code ?? 0;
    }

    public static __api_log_decoder_destroy(handle: bigint): void {
        native_export("log_decoder_destroy")(handle);
    }

    public static __api_log_decode(in_file_path: string, out_file_path: string, priv_key: string): boolean {
        return !!native_export("log_decode")(in_file_path, out_file_path, priv_key);
    }

    public static __api_take_snapshot_string(log_id: bigint, use_gmt_time: boolean): string {
        return native_export("take_snapshot_string")(log_id, !!use_gmt_time) as string;
    }

    public static __api_set_console_callback(enable: boolean, callback?: console_callback): void {
        if (typeof callback !== "function") {
            throw new Error(
                "__api_set_console_callback requires a function callback"
            );
        }
        native_export("set_console_callback")(!!enable, callback);
    }

    public static __api_set_console_buffer_enable(enable: boolean): void {
        native_export("set_console_buffer_enable")(!!enable);
    }

    public static __api_reset_base_dir(in_sandbox: boolean, dir: string): void {
        native_export("reset_base_dir")(!!in_sandbox, dir);
    }

    public static __api_fetch_and_remove_console_buffer(callback: console_callback): boolean {
        if (typeof callback !== "function") {
            throw new Error(
                "__api_fetch_and_remove_console_buffer requires a function callback"
            );
        }
        return !!native_export("fetch_and_remove_console_buffer")(callback);
    }
}
