# Copyright (C) 2026 Tencent.
# BQLOG is licensed under the Apache License, Version 2.0.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
"""
platform_capability bits for BqLog.
"""


class platform_capability:
    """Bits of log.get_platform_capabilities(): what the linked library was built with."""
    memory_map = 1 << 0  # files can be mapped into memory; crash recovery needs this
    multi_process = 1 << 1  # several processes may share one storage directory
    cross_process_file_lock = 1 << 2  # a second process can be kept out of an instance slot
    crash_handler = 1 << 3  # a crash handler can flush the buffers before the process dies
    backtrace = 1 << 4  # stack traces can be collected
    dirent = 1 << 5  # directories can be enumerated; expire_time and capacity_limit need this
    thread_name = 1 << 6  # threads can be named
    default_base_dir = 1 << 7  # a writable default directory exists; consoles reset it first
    user_counter = 1 << 8  # a user mode hardware counter is available for timestamps
