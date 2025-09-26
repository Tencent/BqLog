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
import { lib_def } from "../lib_def";
import { detect_runtime, runtime_kind } from "../utils/env_detector";
import * as tools from "../utils/tools";

export interface native_binding {
  [key: string]: any;
}

function try_require(module_id: string): native_binding | null {
  try {
    // eslint-disable-next-line @typescript-eslint/no-var-requires
    return require(module_id);
  } catch {
    return null;
  }
}

export function load_bq_log_native(): native_binding {
  const kind = detect_runtime();

  const napi_name_env = (typeof process !== "undefined" && process?.env?.BQ_NAPI_NAME) || "";
  const name_candidates = Array.from(
    new Set(
      [
        napi_name_env,
        lib_def.lib_name,
        lib_def.lib_name.toLowerCase(),
        lib_def.lib_name.replace(/_/g, "").toLowerCase(),
        "bq_log",
      ].filter(Boolean)
    )
  );

  if (kind === "node") {
    const node_addon_path = process?.env?.BQ_NODE_ADDON || "";

    // 1) explicit absolute path
    if (node_addon_path) {
      const got = try_require(node_addon_path);
      if (got) {
        return got;
      }
    }

    let search_base_dir: string = tools.path_join(__dirname, "../../../");

    // 2) colocated .node beside js
    for (const base of [lib_def.lib_name, lib_def.lib_name.toLowerCase()]) {
      const local = tools.path_join(search_base_dir, `${base}.node`);
      const got = try_require(local);
      if (got) {
        return got;
      }
    }

    // 3) prebuilt layout candidates
    const platform_name = process.platform;
    const arch_name = process.arch;
    const napi_version = process.versions?.napi;

    const file_names = Array.from(
      new Set(
        [
          `${lib_def.lib_name}.node`,
          `${lib_def.lib_name.toLowerCase()}.node`,
          `node.napi.node`,
          napi_version ? `napi-v${napi_version}.node` : "",
        ].filter(Boolean)
      )
    );

    const dir_candidates: string[] = [];
    // prebuildify style
    dir_candidates.push(
      tools.path_join(search_base_dir, "prebuilds", `${platform_name}-${arch_name}`)
    );
    // custom native layout
    dir_candidates.push(
      tools.path_join(search_base_dir, "native", platform_name, arch_name)
    );
    // node-gyp default
    dir_candidates.push(tools.path_join(search_base_dir, "build", "release"));
    dir_candidates.push(tools.path_join(search_base_dir, "build", "Release"));

    for (const dir of dir_candidates) {
      for (const fname of file_names) {
        const full = tools.path_join(dir, fname);
        const got = try_require(full);
        if (got) {
          return got;
        }
      }
    }

    // 4) bindings fallback
    const bindings = try_require("bindings");
    if (bindings) {
      for (const name of name_candidates) {
        try {
          const got = (bindings as any)(name);
          if (got) {
            return got;
          }
        } catch {
          // continue
        }
      }
    }

    // 5) direct require by name
    for (const name of name_candidates) {
      const got = try_require(name);
      if (got) {
        return got;
      }
    }

    const tried_paths: string[] = [];
    for (const base of [lib_def.lib_name, lib_def.lib_name.toLowerCase()]) {
      tried_paths.push(tools.path_join(search_base_dir, `${base}.node`));
    }
    for (const dir of dir_candidates) {
      for (const fname of file_names) {
        tried_paths.push(tools.path_join(dir, fname));
      }
    }

    throw new Error(
      `failed to load native addon for ${lib_def.lib_name} on node.js\n` +
      `platform=${platform_name} arch=${arch_name}  napi=${napi_version || "-"
      }\n` +
      `tried env BQ_NODE_ADDON and paths:\n` +
      tried_paths.map((p) => `  - ${p}`).join("\n") +
      `\nthen tried bindings(...) and direct require(...) with names: ${name_candidates.join(
        ", "
      )}`
    );
  } else if (kind === "ohos") {
    const g: any = globalThis as any;
    const has_require_napi = typeof g.requireNapi === "function";
    const has_require_native_module = typeof g.requireNativeModule === "function";

    if (has_require_napi) {
      for (const name of name_candidates) {
        try {
          try {
            const got = g.requireNapi(name);
            if (got) {
              return got;
            }
          } catch {
            const got2 = g.requireNapi(name, true);
            if (got2) return got2;
          }
        } catch {
          // continue
        }
      }
    }

    if (has_require_native_module) {
      for (const name of name_candidates) {
        try {
          const got = g.requireNativeModule(name);
          if (got) {
            return got;
          }
        } catch {
          // continue
        }
      }
    }

    throw new Error(
      `failed to load ${lib_def.lib_name} via requireNapi/requireNativeModule on ohos\n` +
      `tried module names: ${name_candidates.join(", ")}`
    );
  }

  throw new Error("unknown runtime: neither node.js nor ohos detected");
}
