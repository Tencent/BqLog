// Copy full tree from ../../../../../wrapper/typescript to src/main/ets/wrapper.
// Destination is cleared first. Existing files with the same name are overwritten.
// All comments are in English and all names use lower_snake_case.

import * as fs from 'fs';
import * as path from 'path';
import { harTasks } from '@ohos/hvigor-ohos-plugin';

function log(...args: any[]) { console.log('[hvigor]', ...args);  }

function exists(p: string): boolean { try { return fs.existsSync(p); } catch { return false; } }
function real(p: string): string { try { return fs.realpathSync(p); } catch { return p; } }

/** attempt list helper: return first existing path */
function pick_existing(paths: string[]): string | null {
  for (const p of paths) {
    if (p && exists(p)) return p;
  }
  return null;
}

/** find up from a start dir until root, testing a predicate each level */
function find_up(startDir: string, test: (dir: string) => string | null): string | null {
  let dir = path.resolve(startDir);
  while (true) {
    const hit = test(dir);
    if (hit) return hit;
    const parent = path.dirname(dir);
    if (parent === dir) break;
    dir = parent;
  }
  return null;
}

/** resolve important roots */
const file_dir = __dirname;
const cwd_dir = process.cwd();
log('file:', __filename);
log('dir:', file_dir);
log('cwd:', cwd_dir, 'real:', real(cwd_dir));

/** try to locate project root (module root containing oh-package.json5) */
const module_root = find_up(file_dir, (d) => {
  const p = path.join(d, 'oh-package.json5');
  return exists(p) ? d : null;
}) || file_dir;
log('module_root:', module_root);

/** compute wrapper src dir:
priority:
1) env override
2) sibling repo layout: <repo>/wrapper/typescript/src (search upward)
3) fallback to previous relative guess (may or may not exist)
 */
const wrapper_env = process.env.BQ_WRAPPER_SRC || '';
const wrapper_up = find_up(module_root, (d) => {
  const p = path.join(d, 'wrapper', 'typescript', 'src');
  return exists(p) ? p : null;
});
const wrapper_guess = path.resolve(module_root, '../../../../../wrapper/typescript/src');

const wrapper_src_dir = pick_existing([wrapper_env, wrapper_up || '', wrapper_guess]);
if (!wrapper_src_dir) {
  console.error('[hvigor] wrapper source not found. Tried:');
  console.error('  env BQ_WRAPPER_SRC =', wrapper_env || '(unset)');
  console.error('  upward search from', module_root, '=> wrapper/typescript/src');
  console.error('  guess =', wrapper_guess);
  throw new Error('wrapper source directory not found');
}
log('wrapper_src_dir:', wrapper_src_dir, 'real:', real(wrapper_src_dir));

/** destination */
const project_root_dir = module_root; // keep old naming for downstream
const wrapper_dst_dir = path.resolve(project_root_dir, 'src/main/ets/wrapper');
const oh_pkg_json5_path = path.resolve(project_root_dir, 'oh-package.json5');

/** version.cpp location */
const version_env = '';
// try common locations upward
const version_up = find_up(module_root, (d) => {
  for (const rel of [
    path.join('src', 'bq_log', 'global', 'version.cpp'),
    path.join('src', 'version.cpp'),
    path.join('version.cpp'),
  ]) {
    const p = path.join(d, rel);
    if (exists(p)) return p;
  }
  return null;
});
const version_guess = path.resolve(module_root, '../../../../../src/bq_log/global/version.cpp');

const version_cpp_path = pick_existing([version_env, version_up || '', version_guess]);
if (!version_cpp_path) {
  log('version.cpp not found; you can set BQ_VERSION_CPP to an absolute path.');
} else {
  log('version_cpp_path:', version_cpp_path, 'real:', real(version_cpp_path));
}

/** fs helpers */
function remove_dir_recursive(dir_path: string): void {
  if (exists(dir_path)) fs.rmSync(dir_path, { recursive: true, force: true });
}
function ensure_dir(dir_path: string): void {
  fs.mkdirSync(dir_path, { recursive: true });
}
function copy_dir_recursive(src_dir: string, dst_dir: string): void {
  const stat = fs.statSync(src_dir);
  if (!stat.isDirectory()) throw new Error(`source is not a directory: ${src_dir}`);
  ensure_dir(dst_dir);

  const entries = fs.readdirSync(src_dir, { withFileTypes: true });
  for (const entry of entries) {
    const src_entry_path = path.join(src_dir, entry.name);
    const dst_entry_path = path.join(dst_dir, entry.name);

    if (entry.isDirectory()) {
      copy_dir_recursive(src_entry_path, dst_entry_path);
    } else if (entry.isFile()) {
      ensure_dir(path.dirname(dst_entry_path));
      fs.copyFileSync(src_entry_path, dst_entry_path);
    } else if (entry.isSymbolicLink()) {
      const link_target = fs.realpathSync(src_entry_path);
      const link_stat = fs.statSync(link_target);
      if (link_stat.isDirectory()) {
        copy_dir_recursive(link_target, dst_entry_path);
      } else if (link_stat.isFile()) {
        ensure_dir(path.dirname(dst_entry_path));
        fs.copyFileSync(link_target, dst_entry_path);
      }
    }
  }
}

/** perform the sync (clear then copy) */
function sync_wrapper_into_module(): void {
  log('sync wrapper ->', wrapper_dst_dir);
  remove_dir_recursive(wrapper_dst_dir);
  ensure_dir(wrapper_dst_dir);
  copy_dir_recursive(wrapper_src_dir!, wrapper_dst_dir);
  const ohos_loader = `${wrapper_dst_dir}/bq/utils/lib_loader.ohos.txt`;
  const loader_ts = `${wrapper_dst_dir}/bq/utils/lib_loader.ts`;
  if (exists(ohos_loader)) {
    fs.copyFileSync(ohos_loader, loader_ts);
    log('loader selected:', ohos_loader, '->', loader_ts);
  } else {
    console.warn('[hvigor] loader template not found:', ohos_loader);
  }
}

/** read version string from version.cpp */
function read_version_from_cpp(p: string): string | null {
  try {
    const text = fs.readFileSync(p, 'utf8');
    let m = text.match(/BQ_LOG_VERSION\s*=\s*"([^"]+)"/);
    if (m?.[1]) return m[1];
    m = text.match(/#\s*define\s+BQ_LOG_VERSION\s+"([^"]+)"/);
    if (m?.[1]) return m[1];
    m = text.match(/get_bq_log_version\s*\([^)]*\)\s*\{[^}]*return\s+"([^"]+)"/s);
    if (m?.[1]) return m[1];
    return null;
  } catch {
    return null;
  }
}

/** update oh-package.json5 version */
function update_oh_package_version(newVersion: string): void {
  if (!exists(oh_pkg_json5_path)) {
    console.warn('[hvigor] oh-package.json5 not found at', oh_pkg_json5_path);
    return;
  }
  const content = fs.readFileSync(oh_pkg_json5_path, 'utf8');
  const re = /(["']version["']\s*:\s*["'])([^"']*)(["'])/;
  if (!re.test(content)) {
    console.warn('[hvigor] no "version" field found in oh-package.json5');
    return;
  }
  const updated = content.replace(re, `$1${newVersion}$3`);
  if (updated !== content) {
    fs.writeFileSync(oh_pkg_json5_path, updated, 'utf8');
    console.log(`[hvigor] updated oh-package.json5 version -> ${newVersion}`);
  } else {
    log('version unchanged.');
  }
}

/** run tasks early on file load to avoid relying on hvigor task APIs or argv */
try {
  // 1) ensure wrapper files are in place
  sync_wrapper_into_module();

  // 2) sync version
  if (version_cpp_path) {
    const v = read_version_from_cpp(version_cpp_path);
    if (v) update_oh_package_version(v);
    else console.warn('[hvigor] failed to parse version from', version_cpp_path);
  }
} catch (e) {
  throw e;
}

/** export default har tasks (for HAR library module) */
export default {
  system: harTasks,
  plugins: []
};