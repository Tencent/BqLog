package bq.def;
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

/**
 * Bits of log.get_platform_capabilities(): what the linked library was built with.
 */
public final class platform_capability {
    /** Files can be mapped into memory; crash recovery needs this. */
    public static final int memory_map = 1 << 0;
    /** Several processes may share one storage directory. */
    public static final int multi_process = 1 << 1;
    /** A second process can be kept out of an instance slot. */
    public static final int cross_process_file_lock = 1 << 2;
    /** A crash handler can flush the buffers before the process dies. */
    public static final int crash_handler = 1 << 3;
    /** Stack traces can be collected. */
    public static final int backtrace = 1 << 4;
    /** Directories can be enumerated; expire_time and capacity_limit need this. */
    public static final int dirent = 1 << 5;
    /** Threads can be named. */
    public static final int thread_name = 1 << 6;
    /** A writable default log directory exists; consoles must call reset_base_dir first. */
    public static final int default_base_dir = 1 << 7;
    /** A user mode hardware counter is available for timestamps. */
    public static final int user_counter = 1 << 8;

    private platform_capability() {
    }
}
