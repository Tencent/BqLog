/*
 * Copyright (C) 2026 Tencent.
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
 * Builds dist/esm and dist/cjs, each holding its JavaScript and its declarations side by side.
 * src/ is shared with the HarmonyOS HAR, whose consumers compile it with ArkTS, so it is never
 * written to: each flavour compiles a staged copy, and file extensions are added to the output.
 */

'use strict';

const fs = require('fs');
const path = require('path');
const { spawnSync } = require('child_process');

const pkg_root = path.resolve(__dirname, '..');
const src_dir = path.join(pkg_root, 'src');
const stage_root = path.join(pkg_root, '.build');
const dist_root = path.join(pkg_root, 'dist');
const loader_rel = path.join('bq', 'utils', 'lib_loader.ts');

const flavours = [
    { name: 'esm', type: 'module', loader: 'lib_loader.esm.txt' },
    { name: 'cjs', type: 'commonjs', loader: 'lib_loader.cjs.txt' },
];

// from "x" | import("x") | import "x" | require("x"), relative specifiers only.
const specifier_re = /(\bfrom\s*|\bimport\s*\(\s*|\bimport\s+|\brequire\s*\(\s*)(['"])(\.{1,2}\/[^'"\r\n]*)\2/g;

function fail(message) {
    throw new Error(message);
}

function remove(p) {
    fs.rmSync(p, { recursive: true, force: true });
}

function copy_tree(from, to) {
    fs.mkdirSync(to, { recursive: true });
    for (const entry of fs.readdirSync(from, { withFileTypes: true })) {
        const s = path.join(from, entry.name);
        const d = path.join(to, entry.name);
        if (entry.isDirectory()) copy_tree(s, d);
        else if (entry.isFile()) fs.copyFileSync(s, d);
    }
}

function walk(dir, out) {
    for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
        const p = path.join(dir, entry.name);
        if (entry.isDirectory()) walk(p, out);
        else if (p.endsWith('.d.ts') || p.endsWith('.js')) out.push(p);
    }
    return out;
}

// The tsc command line rather than the JS API, which TypeScript 7 no longer ships;
// typescript/package.json is reachable in every release, unlike bin/ under 7.x "exports".
function tsc_bin() {
    const pkg_json = require.resolve('typescript/package.json', { paths: [pkg_root] });
    return path.join(path.dirname(pkg_json), 'bin', 'tsc');
}

// The declaration of a module sits next to its .js, so a .d.ts resolves its
// imports against .d.ts files and a .js against .js files.
function target_ext(file) {
    return file.endsWith('.d.ts') ? '.d.ts' : '.js';
}

function resolve_specifier(file, spec) {
    const dir = path.dirname(file);
    const ext = target_ext(file);
    if (/\.(c|m)?js$/.test(spec)) {
        const base = spec.replace(/\.(c|m)?js$/, '');
        return fs.existsSync(path.resolve(dir, base + ext)) ? spec : null;
    }
    if (fs.existsSync(path.resolve(dir, spec + ext))) return `${spec}.js`;
    if (fs.existsSync(path.resolve(dir, spec, `index${ext}`))) return `${spec}/index.js`;
    return null;
}

// Node's ESM loader and TypeScript's node16/nodenext need fully specified relative imports.
// A specifier that does not resolve against the output stops the build instead of shipping.
function rewrite_specifiers(root) {
    for (const file of walk(root, [])) {
        const text = fs.readFileSync(file, 'utf8');
        const out = text.replace(specifier_re, (m, prefix, quote, spec) => {
            const fixed = resolve_specifier(file, spec);
            if (!fixed) fail(`${path.relative(pkg_root, file)}: cannot resolve "${spec}"`);
            return `${prefix}${quote}${fixed}${quote}`;
        });
        if (out !== text) fs.writeFileSync(file, out);
    }
}

function verify(root) {
    let count = 0;
    for (const file of walk(root, [])) {
        const text = fs.readFileSync(file, 'utf8');
        for (const m of text.matchAll(specifier_re)) {
            const spec = m[3];
            if (!spec.endsWith('.js') || resolve_specifier(file, spec) !== spec) {
                fail(`${path.relative(pkg_root, file)}: "${spec}" is not a fully specified relative path`);
            }
            ++count;
        }
    }
    return count;
}

function build_flavour(f) {
    const stage_dir = path.join(stage_root, f.name);
    const out_dir = path.join(dist_root, f.name);

    copy_tree(src_dir, path.join(stage_dir, 'src'));
    fs.copyFileSync(path.join(src_dir, 'bq', 'utils', f.loader), path.join(stage_dir, 'src', loader_rel));

    const stage_config = {
        extends: path.relative(stage_dir, path.join(pkg_root, `tsconfig.${f.name}.json`)).split(path.sep).join('/'),
        compilerOptions: {
            rootDir: './src',
            outDir: path.relative(stage_dir, out_dir).split(path.sep).join('/'),
        },
        include: ['./src/**/*.ts'],
    };
    const config_path = path.join(stage_dir, 'tsconfig.json');
    fs.writeFileSync(config_path, JSON.stringify(stage_config, null, 2));

    const r = spawnSync(process.execPath, [tsc_bin(), '-p', config_path], { stdio: 'inherit' });
    if (r.status !== 0) fail(`tsc failed for ${f.name} (exit ${r.status})`);

    // Gives both the .js and the .d.ts files below it their module format.
    fs.writeFileSync(path.join(out_dir, 'package.json'), JSON.stringify({ type: f.type }, null, 2) + '\n');

    rewrite_specifiers(out_dir);
    const n = verify(out_dir);
    console.log(`[build] ${f.name}: ok, ${n} relative specifiers verified`);
}

// dist/ may also hold a locally staged BqLog.node; only the outputs owned
// here are cleared. dist/types is where declarations used to be emitted.
for (const d of ['esm', 'cjs', 'types']) remove(path.join(dist_root, d));
remove(stage_root);
try {
    for (const f of flavours) build_flavour(f);
} catch (e) {
    console.error(`[build] ${e.message}`);
    process.exitCode = 1;
} finally {
    remove(stage_root);
}
