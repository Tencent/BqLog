// 这是唯一的对外入口。只导出 bq 对象（包含 log 类）。
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

const fs = require('fs');
const path = require('path');

const root = path.resolve(__dirname, '..', 'dist', 'esm');
function needsFix(spec) {
    return (/^\.{1,2}\//.test(spec)) && !/\.[a-zA-Z0-9]+$/.test(spec);
}
function rewrite(content) {
    return content.replace(
        /(import\s+[^'"]*?from\s+|export\s+[^'"]*?from\s+|import\s*\()\s*(['"])(\.{1,2}\/[^'"]*?)\2/g,
        (m, prefix, quote, spec) => `${prefix}${quote}${needsFix(spec) ? spec + '.js' : spec}${quote}`
    );
}
function walk(dir) {
    for (const name of fs.readdirSync(dir)) {
        const p = path.join(dir, name);
        const st = fs.statSync(p);
        if (st.isDirectory()) walk(p);
        else if (st.isFile() && p.endsWith('.js')) {
            const src = fs.readFileSync(p, 'utf8');
            const out = rewrite(src);
            if (out !== src) fs.writeFileSync(p, out);
        }
    }
}
walk(root);