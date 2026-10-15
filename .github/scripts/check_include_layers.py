#!/usr/bin/env python3
# Copyright (C) 2026 Tencent.
# BQLOG is licensed under the Apache License, Version 2.0.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
"""Keep the include layers straight.

bq_log reaches the declarations of bq_common through one header only, so that the
module build of bq_log can hand out the interface of both libraries from a single
module. Two exceptions are allowed:

* the macro only headers of bq_common, which declare nothing and therefore cannot
  collide with the module (macros are not exported by a module anyway);
* the implementation files of bq_log, which are free to use the private headers of
  bq_common (the ones outside include/bq_common), because the module only wraps the
  public ones.
"""

import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

ENTRY = 'bq_common/bq_common_public_include.h'
MACRO_ONLY = {
    'bq_common/platform/macros.h',
    'bq_common/platform/build_type.h',
    'bq_common/misc/assert.h',
}

# The C compatible header is compiled as C and cannot include the C++ entry.
FILE_EXCEPTIONS = {
    'include/bq_log/misc/bq_log_c_types.h': {
        'bq_common/platform/macros.h',
        'bq_common/types/basic_types.h',
    },
}

INCLUDE_RE = re.compile(r'#\s*include\s*"(bq_common/[^"]+)"')


def public_bq_common_headers():
    """Headers that live in include/bq_common, i.e. the ones a module wraps."""
    headers = set()
    root = os.path.join(ROOT, 'include', 'bq_common')
    for current, _, files in os.walk(root):
        for name in files:
            path = os.path.join(current, name)
            headers.add(os.path.relpath(path, os.path.join(ROOT, 'include')).replace('\\', '/'))
    return headers


def check(rel_dir, public_headers):
    problems = []
    for current, _, files in os.walk(os.path.join(ROOT, rel_dir, 'bq_log')):
        for name in files:
            if not name.endswith(('.h', '.hpp', '.cpp', '.cc', '.cppm')):
                continue
            path = os.path.join(current, name)
            rel = os.path.relpath(path, ROOT).replace('\\', '/')
            allowed = {ENTRY} | MACRO_ONLY | FILE_EXCEPTIONS.get(rel, set())
            with open(path, 'r', encoding='utf-8', errors='ignore') as handle:
                for number, line in enumerate(handle, 1):
                    match = INCLUDE_RE.search(line)
                    if not match:
                        continue
                    target = match.group(1)
                    # private headers of bq_common are implementation detail of bq_log
                    if rel_dir == 'src' and target not in public_headers:
                        continue
                    if target not in allowed:
                        problems.append('%s:%d: includes %s' % (rel, number, target))
    return problems


def main():
    public_headers = public_bq_common_headers()
    problems = check('include', public_headers) + check('src', public_headers)
    for problem in problems:
        print(problem)
    if problems:
        print('\nbq_log must take the declarations of bq_common from its entry only.')
        return 1
    print('include layers are clean')
    return 0


if __name__ == '__main__':
    sys.exit(main())
