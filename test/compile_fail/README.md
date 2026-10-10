# fast_info compile-fail cases

Each `.cpp` here must NOT compile; `run_compile_fail.py` builds every one and checks that
the compiler rejects it with the expected message. They cover the `fast_info` diagnostics
that cannot be checked at run time:

| case | expected diagnostic |
|---|---|
| `width16_format.cpp` | `only narrow or u8 string literals` |
| `foreign_category.cpp` | `does not belong to this log class` |
| `plain_log_category.cpp` | `does not belong to this log class` |
| `non_utf8_charset.cpp` | `bq_fast_format_requires_utf8_execution_charset` (needs `-fexec-charset=GBK`) |

Run: `python test/compile_fail/run_compile_fail.py` (C++20, GCC/Clang). CI runs it in the
ubuntu and windows-mingw jobs; see the report for the exact invocation.
