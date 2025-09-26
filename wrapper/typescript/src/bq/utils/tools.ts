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

export function path_join(...parts: Array<string | null | undefined>): string {
  let segments: string[] = [];
  let is_abs = false;
  let drive: string | null = null;
  let is_unc = false;

  // separator decision: default to posix, switch to windows when a win root is detected
  let use_backslash = false;

  for (let i = 0; i < parts.length; i++) {
    const part_raw = parts[i];
    if (!part_raw) continue;

    let part = String(part_raw);
    if (part.length === 0) continue;

    // detect windows drive absolute like "C:\..." or "C:/..."
    const m_drive_abs = part.match(/^([a-zA-Z]):[\\/]/);
    if (m_drive_abs) {
      drive = m_drive_abs[1].toUpperCase();
      is_abs = true;
      is_unc = false;
      use_backslash = true;
      segments = [];
      part = part.slice(2); // keep the separator for trimming below
    } else if (part.startsWith("\\\\") || part.startsWith("//")) {
      // windows UNC root
      drive = null;
      is_abs = true;
      is_unc = true;
      use_backslash = true;
      segments = [];
      // trim all leading slashes/backslashes
      part = part.replace(/^[/\\]+/, "");
    } else if (part[0] === "/" || part[0] === "\\") {
      // posix absolute or windows absolute without drive
      drive = null;
      is_abs = true;
      is_unc = false;
      // keep posix style for such roots
      // do not force backslash unless a drive/unc is seen
      segments = [];
      part = part.replace(/^[/\\]+/, "");
    } else {
      // handle "C:foo" (drive-relative) as a drive prefix without absolute
      const m_drive_rel = part.match(/^([a-zA-Z]):(.*)$/);
      if (m_drive_rel) {
        drive = m_drive_rel[1].toUpperCase();
        is_abs = false;
        is_unc = false;
        use_backslash = true;
        part = m_drive_rel[2] || "";
        if (part.startsWith("/") || part.startsWith("\\")) {
          // treat like absolute drive if a separator follows (rare case)
          is_abs = true;
          part = part.replace(/^[/\\]+/, "");
          segments = [];
        }
      }
    }

    // split by both separators
    const segs = part.split(/[\/\\]+/);
    for (let j = 0; j < segs.length; j++) {
      const s = segs[j];
      if (!s || s === ".") continue;

      if (s === "..") {
        if (segments.length > 0 && segments[segments.length - 1] !== "..") {
          segments.pop();
        } else {
          if (is_abs || is_unc) {
            // at root, do not go above
            continue;
          }
          // drive-relative or fully relative: keep ".."
          segments.push("..");
        }
      } else {
        segments.push(s);
      }
    }
  }

  // choose separator
  const sep = use_backslash ? "\\" : "/";

  // build prefix/root
  let prefix = "";
  if (is_unc) {
    prefix = "\\\\";
  } else if (drive) {
    prefix = drive + ":";
    if (is_abs) prefix += "\\";
  } else if (is_abs) {
    prefix = "/";
  }

  // join segments
  let body = segments.join(sep);

  // ensure we do not return empty unless representing the root
  if (!prefix && !body) return ".";

  // if absolute root only, return prefix
  if (prefix && !body) return prefix;

  // normal case
  return prefix ? prefix + body : body;
}
