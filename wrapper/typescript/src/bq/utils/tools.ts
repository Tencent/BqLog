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

/**
 * Join path segments in a cross-runtime way (Node, OHOS) without relying on node:path.
 * - Supports POSIX and Windows (drive letters, UNC).
 * - Normalizes multiple separators, resolves "." and ".." where possible.
 * - Chooses separator based on detected root (POSIX "/" or Windows "\" when drive/UNC is detected).
 */
/*
 * Cross-runtime utilities without top-level node:* imports.
 * All names use lower_snake_case; member-like variables end with underscore by convention.
 */

/**
 * Join path segments in a cross-runtime way (Node, OHOS) without relying on node:path.
 */
export function path_join(...parts: Array<string | null | undefined>): string {
  let segments: string[] = [];
  let is_abs = false;
  let drive: string | null = null;
  let is_unc = false;
  let use_backslash = false; // switch to backslash when a Windows root is detected

  for (let i = 0; i < parts.length; i++) {
    const part_raw = parts[i];
    if (!part_raw) continue;

    let part = String(part_raw);
    if (part.length === 0) continue;

    // Detect "C:\..." or "C:/..."
    const m_drive_abs = part.match(/^([a-zA-Z]):[\\/]/);
    if (m_drive_abs) {
      drive = m_drive_abs[1].toUpperCase();
      is_abs = true;
      is_unc = false;
      use_backslash = true;
      segments = [];
      part = part.slice(2);
    } else if (part.startsWith("\\\\") || part.startsWith("//")) {
      // Windows UNC root
      drive = null;
      is_abs = true;
      is_unc = true;
      use_backslash = true;
      segments = [];
      part = part.replace(/^[/\\]+/, "");
    } else if (part[0] === "/" || part[0] === "\\") {
      // POSIX absolute or Windows absolute without drive
      drive = null;
      is_abs = true;
      is_unc = false;
      segments = [];
      part = part.replace(/^[/\\]+/, "");
    } else {
      // Handle "C:foo" (drive-relative)
      const m_drive_rel = part.match(/^([a-zA-Z]):(.*)$/);
      if (m_drive_rel) {
        drive = m_drive_rel[1].toUpperCase();
        is_abs = false;
        is_unc = false;
        use_backslash = true;
        part = m_drive_rel[2] || "";
        if (part.startsWith("/") || part.startsWith("\\")) {
          is_abs = true;
          part = part.replace(/^[/\\]+/, "");
          segments = [];
        }
      }
    }

    const segs = part.split(/[\/\\]+/);
    for (let j = 0; j < segs.length; j++) {
      const s = segs[j];
      if (!s || s === ".") continue;

      if (s === "..") {
        if (segments.length > 0 && segments[segments.length - 1] !== "..") {
          segments.pop();
        } else {
          if (is_abs || is_unc) {
            continue; // do not go above root
          }
          segments.push("..");
        }
      } else {
        segments.push(s);
      }
    }
  }

  const sep = use_backslash ? "\\" : "/";
  let prefix = "";
  if (is_unc) prefix = "\\\\";
  else if (drive) {
    prefix = drive + ":";
    if (is_abs) prefix += "\\";
  } else if (is_abs) prefix = "/";

  const body = segments.join(sep);
  if (!prefix && !body) return ".";
  if (prefix && !body) return prefix;
  return prefix ? prefix + body : body;
}

/**
 * Convert a file:// URL to a local filesystem path (POSIX + Windows).
 */
export function file_url_to_path(file_url: string): string {
  try {
    const u = new URL(file_url);
    if (u.protocol !== "file:") return "";
    let p = decodeURIComponent(u.pathname || "");
    const host = u.host || "";

    // Windows UNC root: file://server/ -> \\server\
    if (host && (p === "" || p === "/")) {
      return "\\\\" + host + "\\";
    }
    if (host) {
      // UNC path: file://host/C:/path -> \\host\C:\path
      let body = p.startsWith("/") ? p.slice(1) : p;
      body = body.replace(/\//g, "\\");
      return "\\\\" + host + "\\" + body;
    }

    // Windows drive: /C:/path -> C:\path
    if (/^\/[A-Za-z]:\//.test(p)) {
      p = p.slice(1).replace(/\//g, "\\");
      return p;
    }

    // POSIX
    return p || "/";
  } catch {
    return "";
  }
}

/**
 * Get current module directory.
 * - Prefer __dirname (CJS).
 * - For ESM, pass import.meta.url to convert to directory.
 */
export function current_dirname(import_meta_url?: string): string {
  try {
    // @ts-ignore
    if (typeof __dirname !== "undefined") return __dirname as string;
  } catch { }
  if (import_meta_url) {
    const p = file_url_to_path(import_meta_url);
    if (p) {
      const idx = Math.max(p.lastIndexOf("/"), p.lastIndexOf("\\"));
      return idx >= 0 ? p.slice(0, idx) : p;
    }
  }
  return "";
}

/**
 * Safely get Node's require function across CJS/ESM.
 * Tries multiple fallbacks before giving up.
 */
export function safe_require(): any | null {
  // 1) eval('require')
  try {
    // eslint-disable-next-line no-eval
    const r = (0, eval)("require");
    if (typeof r === "function") return r;
  } catch { }

  // 2) globalThis.require
  try {
    const r = (globalThis as any)?.require;
    if (typeof r === "function") return r;
  } catch { }

  // 3) module.require (CJS)
  try {
    // @ts-ignore
    const m = typeof module !== "undefined" ? (module as any) : null;
    if (m && typeof m.require === "function") return m.require.bind(m);
  } catch { }

  return null;
}