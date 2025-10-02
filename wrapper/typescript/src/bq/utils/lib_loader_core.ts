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
 * Shared core for loading native addon across Node (CJS/ESM).
 * No top-level node:* imports. Business logic is written once here.
 */

import { lib_def } from "../lib_def";
import * as tools from "../utils/tools";

export interface native_binding {
  [key: string]: any;
}
const proc: any = (globalThis as any)?.process;

function try_require_with(req: any, id: string): native_binding | null {
  if (!req) return null;
  try { return req(id); } catch { return null; }
}

function build_candidates(base_dir: string, plat: string, arch: string, napi_ver?: string) {
  const file_names = Array.from(new Set([
    `${lib_def.lib_name}.node`,
    `${lib_def.lib_name.toLowerCase()}.node`,
    napi_ver ? `napi-v${napi_ver}.node` : "",
    `node.napi.node`,
  ].filter(Boolean)));

  const dir_candidates: string[] = [];
  dir_candidates.push(tools.path_join(base_dir, "prebuilds", `${plat}-${arch}`)); // prebuildify-like
  dir_candidates.push(tools.path_join(base_dir, "native", plat, arch));           // custom layout
  dir_candidates.push(tools.path_join(base_dir, "build", "release"));             // node-gyp defaults
  dir_candidates.push(tools.path_join(base_dir, "build", "Release"));
  return { dir_candidates, file_names };
}

function name_candidates_from_env(): string[] {
  const env_name = (typeof proc !== "undefined" && proc?.env?.BQ_NAPI_NAME) || "";
  return Array.from(new Set([
    env_name,
    lib_def.lib_name,
    lib_def.lib_name.toLowerCase(),
    lib_def.lib_name.replace(/_/g, "").toLowerCase(),
    "bq_log",
  ].filter(Boolean)));
}

/**
 * Build a list of base directories to probe:
 * - base_dir (as provided by wrapper)
 * - base_dir/.. and base_dir/../.. (common packaging layouts)
 * - proc.cwd() (when available)
 * - optional extra dirs from env BQ_NODE_SEARCH_DIRS (sep: ; or :)
 */
function gather_base_dirs(base_dir: string): string[] {
  const bases: string[] = [];
  const push = (p?: string | null) => {
    if (!p) return;
    if (!bases.includes(p)) bases.push(p);
  };

  push(base_dir);
  push(tools.path_join(base_dir, ".."));
  push(tools.path_join(base_dir, "../.."));

  try {
    // @ts-ignore
    if (typeof proc !== "undefined" && typeof proc.cwd === "function") {
      push(proc.cwd());
    }
  } catch { }

  try {
    const raw = (typeof proc !== "undefined" && proc?.env?.BQ_NODE_SEARCH_DIRS) || "";
    const extra: string = typeof raw === "string" ? raw : "";
    if (extra) {
      const parts = extra.split(/[;:]/g).map(s => s.trim()).filter(Boolean);
      for (const p of parts) push(p);
    }
  } catch { }

  return bases;
}

/**
 * Core sync probing on Node: env path -> colocated -> prebuilds -> bindings -> byName.
 * - req: a require-like function (CJS require or createRequire(...))
 * - base_dir: base directory to start probing (compiled JS root)
 * On failure, throws with all tried files and base directory list.
 */
export function load_node_sync_core(req: any, base_dir: string): native_binding {
  const plat = proc.platform;
  const arch = proc.arch;
  const napi_ver = proc.versions?.napi;
  const names = name_candidates_from_env();

  // Aggregate diagnostics
  const tried_files: string[] = [];
  const base_dirs = gather_base_dirs(base_dir);

  // 1) explicit absolute path from env
  const addon_env = (typeof proc !== "undefined" && proc?.env?.BQ_NODE_ADDON) || "";
  if (addon_env) {
    const got = try_require_with(req, addon_env);
    if (got) return got;
    tried_files.push(addon_env);
  }

  for (const root of base_dirs) {
    // 2) colocated .node beside compiled JS roots
    for (const base of [lib_def.lib_name, lib_def.lib_name.toLowerCase()]) {
      const full = tools.path_join(root, `${base}.node`);
      const got = try_require_with(req, full);
      if (got) return got;
      tried_files.push(full);
    }

    // 3) prebuilds and fallbacks
    const { dir_candidates, file_names } = build_candidates(root, plat, arch, napi_ver);
    for (const d of dir_candidates) {
      for (const f of file_names) {
        const full = tools.path_join(d, f);
        const got = try_require_with(req, full);
        if (got) return got;
        tried_files.push(full);
      }
    }
  }

  // 4) bindings fallback (optional)
  const bindings = try_require_with(req, "bindings");
  if (bindings) {
    for (const n of names) {
      try {
        const got = (bindings as any)(n);
        if (got) return got;
      } catch { }
    }
  }

  // 5) by module name
  for (const n of names) {
    const got = try_require_with(req, n);
    if (got) return got;
  }

  // Error with full diagnostics
  throw new Error(
    [
      `failed to load native addon for ${lib_def.lib_name} (node sync)`,
      `platform=${plat} arch=${arch} napi=${napi_ver || "-"}`,
      `base_dirs:`,
      ...base_dirs.map(p => `  - ${p}`),
      `tried_files (${tried_files.length}):`,
      ...tried_files.map(p => `  - ${p}`),
      `also tried bindings(...) and direct require(...) with names: ${names.join(", ")}`
    ].join("\n")
  );
}
