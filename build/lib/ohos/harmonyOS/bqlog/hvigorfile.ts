// Copy full tree from ../../../../../wrapper/typescript to src/main/ets/wrapper.
// Destination is cleared first. Existing files with the same name are overwritten.
// All comments are in English and all names use lower_snake_case.

import * as fs from 'fs';
import * as path from 'path';
import { harTasks } from '@ohos/hvigor-ohos-plugin';

/** resolve important paths */
const project_root_dir = __dirname;
const wrapper_src_dir = path.resolve(project_root_dir, '../../../../../wrapper/typescript/src');
const wrapper_dst_dir = path.resolve(project_root_dir, 'src/main/ets/wrapper');

/** remove a directory recursively if it exists */
function remove_dir_recursive(dir_path: string): void {
  if (fs.existsSync(dir_path)) {
    fs.rmSync(dir_path, { recursive: true, force: true });
  }
}

/** ensure a directory exists */
function ensure_dir(dir_path: string): void {
  fs.mkdirSync(dir_path, { recursive: true });
}

/** copy directory recursively from src to dst, overwriting files */
function copy_dir_recursive(src_dir: string, dst_dir: string): void {
  const stat = fs.statSync(src_dir);
  if (!stat.isDirectory()) {
    throw new Error(`source is not a directory: ${src_dir}`);
  }
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
      // resolve symlink and copy target content
      const link_target = fs.realpathSync(src_entry_path);
      const link_stat = fs.statSync(link_target);
      if (link_stat.isDirectory()) {
        copy_dir_recursive(link_target, dst_entry_path);
      } else if (link_stat.isFile()) {
        ensure_dir(path.dirname(dst_entry_path));
        fs.copyFileSync(link_target, dst_entry_path);
      }
    }
    // other special file types are ignored
  }
}

/** perform the sync (clear then copy) */
function sync_wrapper_into_module(): void {
  if (!fs.existsSync(wrapper_src_dir)) {
    throw new Error(`wrapper source directory not found: ${wrapper_src_dir}`);
  }
  remove_dir_recursive(wrapper_dst_dir);
  ensure_dir(wrapper_dst_dir);
  copy_dir_recursive(wrapper_src_dir, wrapper_dst_dir);
}

/** run copy early on file load to avoid relying on hvigor task APIs or argv */
try {
  sync_wrapper_into_module();
} catch (e) {
  // fail fast to ensure wrapper is bundled as required
  throw e;
}

/** export default har tasks (for HAR library module) */
export default {
  system: harTasks,
  plugins: []
};