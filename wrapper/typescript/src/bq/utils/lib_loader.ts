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

/*
* CJS + loader wrapper (no top-level node:* imports).
* - Node CJS: sync auto-load using global require + __dirname (via tools).
*/

import * as tools from "../utils/tools";
import { load_node_sync_core, native_binding } from "./lib_loader_core";

let native_mod_: native_binding | null = null;
const proc: any = (globalThis as any)?.process;

export function native_export(name: string): any {
    const mod = native_mod_;
    if (!mod) throw new Error("native module not loaded");
    const fn = (mod as any)[`__bq_napi_${name}`];
    if (typeof fn !== "function") throw new Error(`native export not found: __bq_napi_${name}`);
    return fn;
}

(function auto_load() {
    try {
        // Robust Node detection
        const is_node =
            typeof proc === "object" &&
            !!(proc as any) &&
            typeof (proc as any).versions === "object" &&
            !!(proc as any).versions?.node;

        if (is_node) {
            // Try multiple strategies to get require
            let req = tools.safe_require();
            if (!req) {
                try {
                    // @ts-ignore try global require
                    req = (typeof require === "function") ? require : null;
                } catch { }
            }
            if (!req) {
                try {
                    // @ts-ignore try module.require
                    const m = (typeof module !== "undefined") ? (module as any) : null;
                    if (m && typeof m.require === "function") req = m.require.bind(m);
                } catch { }
            }

            if (!req) {
                throw new Error("node detected but no require function is available");
            }

            const here_dir = tools.current_dirname(); // __dirname in CJS
            const base_dir = tools.path_join(here_dir, "../../../");
            native_mod_ = load_node_sync_core(req, base_dir);
            return;
        }
        throw new Error("not running under Node.js");
    } catch (e) {
        // eslint-disable-next-line no-console
        console.error("failed to load native module (cjs)");
        // eslint-disable-next-line no-console
        console.error((e as Error)?.message ?? String(e));
    }
})();