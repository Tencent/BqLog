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

package bq

import (
	"github.com/Tencent/BqLog/wrapper/go/src/bq/def"
	"github.com/Tencent/BqLog/wrapper/go/src/bq/impl"
)

// Console_callback receives a formatted console log line.
type Console_callback func(log_id uint64, category_idx int32, level def.Log_level, content string)

func wrap_console_callback(callback Console_callback) impl.Console_callback {
	if callback == nil {
		return nil
	}
	return func(log_id uint64, category_idx int32, level int32, content string) {
		callback(log_id, category_idx, def.Log_level(level), content)
	}
}

// Register_console_callback registers a callback invoked for every console
// appender output. Pass nil to unregister.
func Register_console_callback(callback Console_callback) {
	impl.Set_console_hook(wrap_console_callback(callback))
	if callback != nil {
		impl.Register_console_callback()
	} else {
		impl.Unregister_console_callback()
	}
}

// Set_console_buffer_enable toggles the console buffer used by
// Fetch_and_remove_console_buffer.
func Set_console_buffer_enable(enable bool) {
	impl.Set_console_buffer_enable(enable)
}

// Fetch_and_remove_console_buffer fetches buffered console output one by one;
// the callback is invoked for each fetched entry. Returns false when the
// buffer is empty. bqLog requires all fetches to come from the same thread,
// so callers should keep fetching on one goroutine pinned with
// runtime.LockOSThread.
func Fetch_and_remove_console_buffer(callback Console_callback) bool {
	return impl.Fetch_and_remove_console_buffer(wrap_console_callback(callback))
}
