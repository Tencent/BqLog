package bq.def;
/*
 * Copyright (C) 2025 Tencent.
 * BQLOG is licensed under the Apache License, Version 2.0.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */

/** Log levels, in the order the library defines them. */
public enum log_level {
    /** Most detailed level, usually the first one switched off. */
    verbose,
    /** Development diagnostics. */
    debug,
    /** Normal progress messages. */
    info,
    /** Something unexpected the program carried on from. */
    warning,
    /** An operation failed. */
    error,
    /** The program is about to stop. */
    fatal,
}
