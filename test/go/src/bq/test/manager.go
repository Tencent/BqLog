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
package main

import (
	"fmt"
	"runtime"
	"sync"
	"sync/atomic"
	"time"

	bq "github.com/Tencent/BqLog/wrapper/go/src/bq"
	"github.com/Tencent/BqLog/wrapper/go/src/bq/def"
)

var console_output atomic.Value // string
var fetch_count atomic.Int64

func start_console_fetcher() {
	bq.Set_console_buffer_enable(true)
	go func() {
		runtime.LockOSThread() // bqLog requires console buffer fetches to come from one thread
		var last_fetch_count int64
		for {
			new_fetch_count := fetch_count.Load()
			for {
				fetched := bq.Fetch_and_remove_console_buffer(func(log_id uint64, category_idx int32, level def.Log_level, content string) {
					console_output.Store(content)
				})
				if !fetched {
					break
				}
			}
			if new_fetch_count != last_fetch_count {
				new_fetch_count++
				fetch_count.Store(new_fetch_count)
			}
			last_fetch_count = new_fetch_count
			time.Sleep(time.Millisecond)
		}
	}()
}

func get_console_output() (string, bool) {
	prev := fetch_count.Load()
	fetch_count.Store(prev + 1)
	for fetch_count.Load() != prev+2 {
		runtime.Gosched()
	}
	v := console_output.Load()
	if v == nil {
		return "", false
	}
	return v.(string), true
}

type test_result struct {
	success_count atomic.Int64
	total_count   atomic.Int64
	mu           sync.Mutex
	failed_infos  []string
}

func (r *test_result) add_result(success bool, format string, args ...interface{}) {
	r.total_count.Add(1)
	if success {
		r.success_count.Add(1)
		return
	}
	r.mu.Lock()
	defer r.mu.Unlock()
	if len(r.failed_infos) < 128 {
		r.failed_infos = append(r.failed_infos, fmt.Sprintf(format, args...))
	} else if len(r.failed_infos) == 128 {
		r.failed_infos = append(r.failed_infos, "... Too many test case errors. A maximum of 128 can be displayed, and the rest are omitted. ....")
	}
}

func (r *test_result) check_log_output_end_with(end_with string, format string, args ...interface{}) {
	output, ok := get_console_output()
	if !ok {
		r.add_result(false, format, args...)
		return
	}
	r.add_result(len(output) >= len(end_with) && output[len(output)-len(end_with):] == end_with, format, args...)
}

func (r *test_result) is_all_pass() bool {
	return r.success_count.Load() == r.total_count.Load()
}

func (r *test_result) output(type_name string) {
	level := def.Info
	if !r.is_all_pass() {
		level = def.Error
	}
	bq.Console(level, fmt.Sprintf("test case %s result: %d/%d", type_name, r.success_count.Load(), r.total_count.Load()))
	for _, info := range r.failed_infos {
		bq.Console(level, "\t"+info)
	}
	bq.Console(def.Info, " ")
}
