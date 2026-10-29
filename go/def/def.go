/* Copyright (C) 2026 Tencent.
 * BQLOG is licensed under the Apache License, Version 2.0.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */
package def

// Log_level mirrors bq::log_level.
type Log_level int32

const (
	Verbose Log_level = iota
	Debug
	Info
	Warning
	Error
	Fatal
)

// Recovery_capability mirrors bq::recovery_capability: what the platform offers, and what is missing.
type Recovery_capability int32

const (
	Recovery_ready Recovery_capability = iota
	Recovery_no_memory_map
	Recovery_no_instance_lock
)

// Platform_capability is a bit of Get_platform_capabilities: what the linked library was built with.
type Platform_capability uint32

const (
	Platform_capability_memory_map Platform_capability = 1 << iota
	Platform_capability_multi_process
	Platform_capability_cross_process_file_lock
	Platform_capability_crash_handler
	Platform_capability_backtrace
	Platform_capability_dirent
	Platform_capability_thread_name
	Platform_capability_default_base_dir
	Platform_capability_user_counter
)
