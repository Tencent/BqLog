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
 * Verifies the npm package the way a consumer installs it: the tarball goes into a throwaway
 * project and every phase checks it from the outside.
 *
 *   layout   every file package.json names exists, in the module format its condition implies
 *   resolve  import, require and the package.json subpath resolve as intended under this Node
 *   load     import, require and import() from CommonJS load the native library
 *   types    every TypeScript release and tsgo x module mode x skipLibCheck on and off
 *            type-checks a consumer
 *   attw     @arethetypeswrong/cli on the tarball (needs Node >= 20)
 *
 * Usage: node build/pack_test/npm_verify.cjs [--tgz <file>] [--phases layout,resolve,...]
 *                                            [--node <executable>]... [--keep]
 * Without --tgz, wrapper/typescript is built and packed first. --node repeats resolve and
 * load under other Node executables; the default is the one running this script.
 */

'use strict';

const fs = require('fs');
const os = require('os');
const path = require('path');
const { spawn, spawnSync } = require('child_process');

const repo_root = path.resolve(__dirname, '..', '..');
const wrapper_dir = path.join(repo_root, 'wrapper', 'typescript');
const pkg_name = '@pippocao/bqlog';
const all_phases = ['layout', 'resolve', 'load', 'types', 'attw'];
const attw_version = '0.18.5';
const max_reported = 40;

// The releases where module resolution changed, plus the last of each major. Add new ones here.
// From 7.0 tsc is the native (Go) compiler; tsgo is its final preview, which projects still pin.
const compilers = [
    ...['4.0.8', '4.7.4', '4.9.5', '5.0.4', '5.4.5', '5.9.3', '6.0.3', '7.0.2']
        .map((v) => ({ name: v, version: v, spec: `typescript@${v}`, bin: 'bin/tsc' })),
    { name: 'tsgo', version: '7.0.0', spec: '@typescript/native-preview@7.0.0-dev.20260707.2', bin: 'bin/tsgo' },
];

// Case files, one per way of reaching the package. .mts is ESM and .cts is CommonJS; a .ts
// takes the "type" of the nearest package.json: type_module/ is ESM, the project root is not.
const case_imports = {
    'static.ts': 'static',
    'import_equals.ts': 'import_equals',
    'dynamic_import.ts': 'dynamic',
    'esm_static.mts': 'static',
    'type_module/esm_static.ts': 'static',
    'cjs_static.cts': 'static',
    'cjs_import_equals.cts': 'import_equals',
    'cjs_dynamic_import.cts': 'dynamic',
};
const node_cases = ['esm_static.mts', 'type_module/esm_static.ts', 'static.ts', 'cjs_static.cts',
    'cjs_import_equals.cts', 'cjs_dynamic_import.cts'];

const setups = [
    { name: 'node10', since: '4.0', until: '6.0', flags: ['--module', 'commonjs', '--moduleResolution', 'node'],
        cases: ['static.ts', 'import_equals.ts', 'dynamic_import.ts'] },
    { name: 'node16', since: '4.7', flags: ['--module', 'node16', '--moduleResolution', 'node16'], cases: node_cases },
    { name: 'node18', since: '5.8', flags: ['--module', 'node18'], cases: node_cases },
    { name: 'node20', since: '5.9', flags: ['--module', 'node20'], cases: node_cases },
    { name: 'nodenext', since: '4.7', flags: ['--module', 'nodenext', '--moduleResolution', 'nodenext'], cases: node_cases },
    { name: 'bundler', since: '5.0', flags: ['--module', 'esnext', '--moduleResolution', 'bundler'],
        cases: ['static.ts', 'dynamic_import.ts'] },
    // The documented way off node10, which 6.0 deprecates and 7.0 removes.
    { name: 'cjs_bundler', since: '6.0', flags: ['--module', 'commonjs', '--moduleResolution', 'bundler'],
        cases: ['static.ts', 'import_equals.ts', 'dynamic_import.ts'] },
    { name: 'preserve', since: '5.4', flags: ['--module', 'preserve'],
        cases: ['static.ts', 'import_equals.ts', 'dynamic_import.ts'] },
];

// Statics, an instance, a cross-file return type, a member inherited across files and enums
// from nested modules: all of it must type-check.
const valid_lines = [
    'const version: string = bq.log.get_version();',
    'const lg: ReturnType<typeof bq.log.create_log> = bq.log.create_log("npm_verify", "");',
    'const logged: boolean = lg.info("{}", version);',
    'const level: number = bq.log_level.warning;',
    'const recovery: number = bq.log.get_recovery_capability();',
    'type category_log_t = typeof bq.category_log.prototype;',
    'const snapshot = (c: category_log_t): string => c.take_snapshot();',
    'const count = (c: category_log_t): number => c.get_categories_count();',
];
// Each must fail with TS2322. No error means that type silently decayed to any.
const mismatch_lines = [
    'const class_as_number: number = bq.log;',
    'const static_as_number: number = bq.log.get_version();',
    'const member_as_number: number = lg.get_name();',
    'const inherited_as_number = (c: category_log_t): number => c.take_snapshot();',
    'const enum_as_string: string = bq.log.get_recovery_capability();',
];

function fail(message) {
    throw new Error(message);
}

function read_json(file) {
    return JSON.parse(fs.readFileSync(file, 'utf8'));
}

function write_file(file, text) {
    fs.mkdirSync(path.dirname(file), { recursive: true });
    fs.writeFileSync(file, text);
}

function rel(from, to) {
    return path.relative(from, to).split(path.sep).join('/');
}

function version_at_least(version, min) {
    const a = version.split('.').map(Number);
    const b = min.split('.').map(Number);
    for (let i = 0; i < b.length; ++i) {
        if ((a[i] || 0) !== b[i]) return (a[i] || 0) > b[i];
    }
    return true;
}

function parse_args(argv) {
    const opts = { tgz: null, phases: all_phases, nodes: [], keep: false };
    for (let i = 0; i < argv.length; ++i) {
        const a = argv[i];
        if (a === '--tgz') opts.tgz = path.resolve(argv[++i]);
        else if (a === '--phases') opts.phases = argv[++i].split(',').map((s) => s.trim()).filter(Boolean);
        else if (a === '--node') opts.nodes.push(path.resolve(argv[++i]));
        else if (a === '--keep') opts.keep = true;
        else fail(`unknown argument: ${a}`);
    }
    for (const p of opts.phases) {
        if (!all_phases.includes(p)) fail(`unknown phase "${p}", expected some of ${all_phases.join(',')}`);
    }
    if (opts.tgz && !fs.existsSync(opts.tgz)) fail(`no such tarball: ${opts.tgz}`);
    for (const n of opts.nodes) if (!fs.existsSync(n)) fail(`no such Node executable: ${n}`);
    if (opts.nodes.length === 0) opts.nodes.push(process.execPath);
    return opts;
}

// npm is a .cmd shim on Windows, which Node only starts through a shell.
function npm(args, cwd) {
    const command = ['npm', ...args].join(' ');
    console.log(`[npm_verify] ${command}  (in ${cwd})`);
    const r = spawnSync(command, { cwd, stdio: 'inherit', shell: true });
    if (r.status !== 0) fail(`"${command}" failed with exit ${r.status}`);
}

function pack_wrapper(dest) {
    if (!fs.existsSync(path.join(wrapper_dir, 'node_modules', 'typescript'))) {
        npm(['install', '--no-audit', '--no-fund'], wrapper_dir);
    }
    npm(['run', 'build'], wrapper_dir);
    npm(['pack', '--pack-destination', `"${dest}"`], wrapper_dir);
    const tgz = fs.readdirSync(dest).find((f) => f.endsWith('.tgz'));
    if (!tgz) fail(`npm pack left no tarball in ${dest}`);
    return path.join(dest, tgz);
}

function install(dir, dependencies) {
    write_file(path.join(dir, 'package.json'),
        JSON.stringify({ name: path.basename(dir), private: true, dependencies }, null, 2));
    npm(['install', '--no-audit', '--no-fund', '--no-package-lock', '--loglevel=error'], dir);
}

// How Node and TypeScript see a file: by extension, else by the nearest package.json "type".
function module_format(file) {
    if (/\.(mjs|mts)$/.test(file)) return 'esm';
    if (/\.(cjs|cts)$/.test(file)) return 'cjs';
    if (!/\.(js|ts)$/.test(file)) return null;
    for (let dir = path.dirname(file); ; dir = path.dirname(dir)) {
        const pj = path.join(dir, 'package.json');
        if (fs.existsSync(pj)) return read_json(pj).type === 'module' ? 'esm' : 'cjs';
        if (path.dirname(dir) === dir) return 'cjs';
    }
}

function walk(dir, out) {
    for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
        const p = path.join(dir, entry.name);
        if (entry.isDirectory()) walk(p, out);
        else out.push(p);
    }
    return out;
}

function phase_layout(ctx) {
    const root = ctx.pkg_dir;
    const pj = read_json(path.join(root, 'package.json'));
    const problems = [];
    const targets = [];

    // "import" and "require" fix the format of everything below them, types included.
    (function collect(node, label, format) {
        if (typeof node === 'string') {
            targets.push({ label, target: node, format });
            return;
        }
        for (const [key, sub] of Object.entries(node || {})) {
            const f = key === 'import' ? 'esm' : key === 'require' ? 'cjs' : format;
            collect(sub, `${label} > ${key}`, f);
        }
    })(pj.exports, 'exports', null);
    const main_format = pj.main ? module_format(path.join(root, pj.main)) : null;
    if (pj.main) targets.push({ label: 'main', target: pj.main, format: 'cjs' });
    if (pj.module) targets.push({ label: 'module', target: pj.module, format: 'esm' });
    if (pj.types) targets.push({ label: 'types', target: pj.types, format: main_format });

    for (const t of targets) {
        const file = path.join(root, t.target);
        if (!fs.existsSync(file)) {
            problems.push(`${t.label}: ${t.target} does not exist`);
            continue;
        }
        const actual = module_format(file);
        if (t.format && actual && actual !== t.format) {
            problems.push(`${t.label}: ${t.target} is ${actual}, expected ${t.format}`);
        }
    }
    if (!pj.exports || !pj.exports['./package.json']) problems.push('exports: "./package.json" is not exported');

    // A declaration takes its format from the package.json next to its JavaScript.
    for (const file of walk(path.join(root, 'dist'), [])) {
        if (file.endsWith('.d.ts') && !fs.existsSync(file.slice(0, -'.d.ts'.length) + '.js')) {
            problems.push(`${rel(root, file)} has no .js next to it`);
        }
    }
    return problems;
}

function run_node(node, args, cwd) {
    const r = spawnSync(node, args, { cwd, encoding: 'utf8', timeout: 120000 });
    const output = `${r.stdout || ''}${r.stderr || ''}`.trim();
    if (r.status !== 0) return { error: `exit ${r.status}${r.error ? ` (${r.error.message})` : ''}: ${output}` };
    const last = output.split(/\r?\n/).filter((l) => l.startsWith('{')).pop();
    try {
        return { value: JSON.parse(last) };
    } catch (e) {
        return { error: `no result in output: ${output}` };
    }
}

function node_version(node) {
    return spawnSync(node, ['-p', 'process.versions.node'], { encoding: 'utf8' }).stdout.trim();
}

function write_runtime_probes(dir) {
    write_file(path.join(dir, 'resolve_require.cjs'), [
        `const entry = require.resolve('${pkg_name}');`,
        `const name = require('${pkg_name}/package.json').name;`,
        'console.log(JSON.stringify({ entry, name }));',
        '',
    ].join('\n'));
    // Before Node 18.19 / 20.6, import.meta.resolve is flagged and returns a promise.
    write_file(path.join(dir, 'resolve_import.mjs'), [
        "import { fileURLToPath } from 'url';",
        `const entry = fileURLToPath(await import.meta.resolve('${pkg_name}'));`,
        'console.log(JSON.stringify({ entry }));',
        '',
    ].join('\n'));
    write_file(path.join(dir, 'load_require.cjs'), [
        `const { bq } = require('${pkg_name}');`,
        'console.log(JSON.stringify({ version: bq.log.get_version() }));',
        '',
    ].join('\n'));
    write_file(path.join(dir, 'load_import.mjs'), [
        `import { bq } from '${pkg_name}';`,
        'console.log(JSON.stringify({ version: bq.log.get_version() }));',
        '',
    ].join('\n'));
    // Both builds in one process, sharing the one native library.
    write_file(path.join(dir, 'load_dynamic_import.cjs'), [
        `const cjs = require('${pkg_name}');`,
        `import('${pkg_name}').then((esm) => {`,
        '    console.log(JSON.stringify({ version: esm.bq.log.get_version(), cjs_version: cjs.bq.log.get_version() }));',
        '});',
        '',
    ].join('\n'));
}

function phase_resolve(ctx) {
    const problems = [];
    for (const node of ctx.nodes) {
        const v = node_version(node);
        const unflagged = version_at_least(v, '20.6') || (v.startsWith('18.') && version_at_least(v, '18.19'));
        const esm_flags = unflagged ? [] : ['--experimental-import-meta-resolve'];
        const req = run_node(node, [path.join(ctx.probe_dir, 'resolve_require.cjs')], ctx.consumer_dir);
        const imp = run_node(node, [...esm_flags, path.join(ctx.probe_dir, 'resolve_import.mjs')], ctx.consumer_dir);
        const tag = `node ${v}`;
        if (req.error) problems.push(`${tag}: require: ${req.error}`);
        if (imp.error) problems.push(`${tag}: import: ${imp.error}`);
        if (req.error || imp.error) continue;
        for (const [how, entry, format] of [['require', req.value.entry, 'cjs'], ['import', imp.value.entry, 'esm']]) {
            const actual = module_format(entry);
            const where = rel(ctx.pkg_dir, entry);
            if (where.startsWith('..')) problems.push(`${tag}: ${how} resolved outside the package: ${entry}`);
            else if (actual !== format) problems.push(`${tag}: ${how} resolved ${where}, which is ${actual}, expected ${format}`);
            else console.log(`[npm_verify] resolve, ${tag}: ${how} -> ${where} (${actual})`);
        }
        if (req.value.name !== pkg_name) problems.push(`${tag}: ${pkg_name}/package.json gave name ${req.value.name}`);
    }
    return problems;
}

function phase_load(ctx) {
    const problems = [];
    for (const node of ctx.nodes) {
        const tag = `node ${node_version(node)}`;
        const versions = [];
        for (const probe of ['load_require.cjs', 'load_import.mjs', 'load_dynamic_import.cjs']) {
            const r = run_node(node, [path.join(ctx.probe_dir, probe)], ctx.consumer_dir);
            if (r.error) {
                problems.push(`${tag}: ${probe}: ${r.error}`);
                continue;
            }
            versions.push(r.value.version);
            if (r.value.cjs_version !== undefined) versions.push(r.value.cjs_version);
        }
        const distinct = [...new Set(versions)];
        if (distinct.length > 1 || distinct.some((s) => !s)) problems.push(`${tag}: versions disagree: ${versions.join(', ')}`);
        else if (distinct.length === 1) console.log(`[npm_verify] load, ${tag}: import, require and import() all report ${distinct[0]}`);
    }
    return problems;
}

function case_source(how) {
    const body = [...valid_lines, ...mismatch_lines].map((l) => `    ${l}`);
    let lines;
    if (how === 'static') {
        lines = [`import { bq } from "${pkg_name}";`, '', 'export function run(): void {', ...body, '}'];
    } else if (how === 'import_equals') {
        lines = [`import bqlog = require("${pkg_name}");`, 'const bq = bqlog.bq;', '',
            'export function run(): void {', ...body, '}'];
    } else {
        lines = ['export async function run(): Promise<void> {', `    const { bq } = await import("${pkg_name}");`,
            ...body, '}'];
    }
    const expected = new Set();
    lines.forEach((l, i) => {
        if (mismatch_lines.includes(l.trim())) expected.add(i + 1);
    });
    return { text: lines.join('\n') + '\n', expected };
}

function write_type_cases(dir) {
    const expected = {};
    for (const [file, how] of Object.entries(case_imports)) {
        const c = case_source(how);
        write_file(path.join(dir, file), c.text);
        expected[`cases/${file}`] = c.expected;
    }
    write_file(path.join(dir, 'type_module', 'package.json'), '{ "type": "module" }\n');
    return expected;
}

function run_async(command, args, cwd) {
    return new Promise((resolve) => {
        const child = spawn(command, args, { cwd });
        let output = '';
        child.stdout.on('data', (d) => { output += d; });
        child.stderr.on('data', (d) => { output += d; });
        child.on('close', (status) => resolve({ status, output }));
        child.on('error', (e) => resolve({ status: -1, output: e.message }));
    });
}

async function pool(jobs, width) {
    const results = new Array(jobs.length);
    let next = 0;
    async function worker() {
        while (next < jobs.length) {
            const i = next++;
            results[i] = await jobs[i]();
        }
    }
    await Promise.all(Array.from({ length: width }, worker));
    return results;
}

function judge(run, output, expected) {
    const errors = [];
    for (const line of output.split(/\r?\n/)) {
        const m = line.match(/^(.+?)\((\d+),\d+\): error (TS\d+): (.*)$/);
        if (m) errors.push({ file: m[1].split(path.sep).join('/'), line: Number(m[2]), code: m[3], text: line });
        else if (/error TS\d+/.test(line)) errors.push({ file: null, text: line.trim() });
    }
    const cells = {};
    const details = [];
    for (const file of run.files) {
        const want = expected[file];
        const mine = errors.filter((e) => e.file === file);
        const missing = [...want].filter((l) => !mine.some((e) => e.line === l && e.code === 'TS2322'));
        const extra = mine.filter((e) => !(want.has(e.line) && e.code === 'TS2322'));
        cells[file] = missing.length === 0 && extra.length === 0;
        if (missing.length > 0) details.push(`${file}: no TS2322 on line ${missing.join(', ')} (the types decayed to any)`);
        for (const e of extra) details.push(e.text);
    }
    const foreign = errors.filter((e) => !run.files.includes(e.file));
    if (foreign.length > 0 || (errors.length === 0 && run.status !== 0)) {
        for (const f of run.files) cells[f] = false;
        details.push(...foreign.map((e) => e.text));
        if (errors.length === 0) details.push(`tsc exit ${run.status}: ${output.trim()}`);
    }
    return { cells, details };
}

async function phase_types(ctx) {
    const expected = write_type_cases(path.join(ctx.consumer_dir, 'cases'));
    const runs = [];
    for (const compiler of compilers) {
        const v = compiler.version;
        for (const setup of setups) {
            if (!version_at_least(v, setup.since) || (setup.until && !version_at_least(setup.until, v.split('.').slice(0, 2).join('.')))) continue;
            for (const skip_lib_check of [false, true]) {
                const flags = [...setup.flags];
                if (setup.name === 'node10' && version_at_least(v, '6.0')) flags.push('--ignoreDeprecations', '6.0');
                if (skip_lib_check) flags.push('--skipLibCheck');
                runs.push({ compiler, setup, skip_lib_check, flags, files: setup.cases.map((c) => `cases/${c}`) });
            }
        }
    }
    const results = await pool(runs.map((run) => async () => {
        const tsc = path.join(ctx.consumer_dir, 'node_modules', `tsc-${run.compiler.name}`, run.compiler.bin);
        const args = [tsc, '--noEmit', '--strict', '--target', 'es2020', '--pretty', 'false', ...run.flags, ...run.files];
        const r = await run_async(process.execPath, args, ctx.consumer_dir);
        run.status = r.status;
        return judge(run, r.output, expected);
    }), Math.max(2, os.cpus().length));

    const problems = [];
    for (const skip_lib_check of [false, true]) {
        console.log(`\n[npm_verify] types, skipLibCheck=${skip_lib_check}`);
        console.log(['setup'.padEnd(12), 'case'.padEnd(27), ...compilers.map((t) => t.name.padEnd(6))].join(' '));
        for (const setup of setups) {
            for (const c of setup.cases) {
                const cells = compilers.map((t) => {
                    const i = runs.findIndex((r) => r.compiler === t && r.setup === setup && r.skip_lib_check === skip_lib_check);
                    if (i < 0) return '-'.padEnd(6);
                    return (results[i].cells[`cases/${c}`] ? 'ok' : 'FAIL').padEnd(6);
                });
                console.log([setup.name.padEnd(12), c.padEnd(27), ...cells].join(' '));
            }
        }
    }
    runs.forEach((run, i) => {
        for (const d of results[i].details) {
            problems.push(`${run.compiler.spec}, ${run.setup.name}, skipLibCheck=${run.skip_lib_check}: ${d}`);
        }
    });
    return problems;
}

function phase_attw(ctx) {
    if (!version_at_least(process.versions.node, '20')) return [`@arethetypeswrong/cli needs Node >= 20, this is ${process.versions.node}`];
    const dir = path.join(ctx.work_dir, 'attw');
    install(dir, { '@arethetypeswrong/cli': attw_version });
    const cli_dir = path.join(dir, 'node_modules', '@arethetypeswrong', 'cli');
    const bin = read_json(path.join(cli_dir, 'package.json')).bin;
    const r = spawnSync(process.execPath, [path.join(cli_dir, typeof bin === 'string' ? bin : bin.attw), ctx.tgz,
        '--format', 'ascii', '--no-emoji', '--no-color'], { cwd: dir, stdio: 'inherit' });
    return r.status === 0 ? [] : [`@arethetypeswrong/cli reported problems (exit ${r.status})`];
}

async function main() {
    const opts = parse_args(process.argv.slice(2));
    const work_dir = fs.mkdtempSync(path.join(os.tmpdir(), 'bqlog-npm-verify-'));
    const ctx = { ...opts, work_dir, consumer_dir: path.join(work_dir, 'consumer') };
    const failed = [];
    try {
        ctx.tgz = opts.tgz || pack_wrapper(work_dir);
        console.log(`[npm_verify] tarball: ${ctx.tgz}`);
        console.log(`[npm_verify] phases: ${opts.phases.join(', ')}`);

        const consumer_phases = opts.phases.filter((p) => p !== 'attw');
        if (consumer_phases.length > 0) {
            const deps = { [pkg_name]: `file:${ctx.tgz.split(path.sep).join('/')}` };
            if (opts.phases.includes('types')) {
                for (const c of compilers) deps[`tsc-${c.name}`] = `npm:${c.spec}`;
            }
            install(ctx.consumer_dir, deps);
            ctx.pkg_dir = path.join(ctx.consumer_dir, 'node_modules', ...pkg_name.split('/'));
            ctx.probe_dir = path.join(ctx.consumer_dir, 'probes');
            write_runtime_probes(ctx.probe_dir);
        }

        const phases = { layout: phase_layout, resolve: phase_resolve, load: phase_load, types: phase_types, attw: phase_attw };
        for (const name of opts.phases) {
            let problems;
            try {
                problems = await phases[name](ctx);
            } catch (e) {
                problems = [e.message];
            }
            if (problems.length === 0) {
                console.log(`[npm_verify] ${name}: ok`);
            } else {
                failed.push(name);
                console.error(`[npm_verify] ${name}: FAILED`);
                for (const p of problems.slice(0, max_reported)) console.error(`  - ${p}`);
                if (problems.length > max_reported) console.error(`  ... and ${problems.length - max_reported} more`);
            }
        }
    } catch (e) {
        failed.push('setup');
        console.error(`[npm_verify] ${e.message}`);
    } finally {
        if (opts.keep) console.log(`[npm_verify] kept ${work_dir}`);
        else fs.rmSync(work_dir, { recursive: true, force: true });
    }
    console.log(failed.length === 0 ? '[npm_verify] all phases passed' : `[npm_verify] failed: ${failed.join(', ')}`);
    process.exitCode = failed.length === 0 ? 0 : 1;
}

main();
