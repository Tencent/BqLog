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

export type runtime_kind = "node" | "ohos" | "unknown";
const proc: any = (globalThis as any)?.process;
export function detect_runtime(): runtime_kind {
  const is_node = typeof proc !== "undefined"
      && !!(proc as any).versions
      && typeof (proc as any).versions.node === "string";
  if (is_node) {
    return "node";
  }
  const has_require_napi = typeof (globalThis as any).requireNapi === "function";
  const has_require_native_module = typeof (globalThis as any).requireNativeModule === "function";
  if (has_require_napi || has_require_native_module) {
    return "ohos";
  }
  return "unknown";
}
