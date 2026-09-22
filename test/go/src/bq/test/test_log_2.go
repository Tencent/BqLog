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
	"strings"
	"sync"
	"sync/atomic"
	"time"

	bq "github.com/Tencent/BqLog/wrapper/go/src/bq"
	"github.com/Tencent/BqLog/wrapper/go/src/bq/def"
)

func test_log_multi_goroutine() *test_result {
	appender := strings.Repeat("a", 32)
	log_inst_sync := bq.Create_log("sync_log", "appenders_config.FileAppender.type=compressed_file\n"+
		"appenders_config.FileAppender.time_zone=localtime\n"+
		"appenders_config.FileAppender.max_file_size=100000000\n"+
		"appenders_config.FileAppender.file_name=Output/sync_log\n"+
		"appenders_config.FileAppender.levels=[info, info, error,info]\n"+
		"\n"+
		"log.thread_mode=sync\n", nil)
	log_inst_async := bq.Create_log("async_log", "appenders_config.FileAppender.type=compressed_file\n"+
		"appenders_config.FileAppender.time_zone=localtime\n"+
		"appenders_config.FileAppender.max_file_size=100000000\n"+
		"appenders_config.FileAppender.file_name=Output/async_log\n"+
		"appenders_config.FileAppender.levels=[error,info]\n"+
		"\n", nil)
	result := &test_result{}

	var live_thread atomic.Int32
	left_thread := int32(100)
	var wg sync.WaitGroup
	for left_thread > 0 || live_thread.Load() > 0 {
		if left_thread > 0 && live_thread.Load() < 5 {
			wg.Add(1)
			live_thread.Add(1)
			left_thread--
			go func() {
				defer wg.Done()
				defer live_thread.Add(-1)
				log_content := ""
				for i := 0; i < 128; i++ {
					log_content += appender
					log_inst_sync.Info(log_content)
				}
			}()
		} else {
			time.Sleep(time.Millisecond)
		}
	}
	wg.Wait()

	fmt.Println("Sync Test Finished")
	left_thread = 32
	for left_thread > 0 || live_thread.Load() > 0 {
		if left_thread > 0 && live_thread.Load() < 5 {
			wg.Add(1)
			live_thread.Add(1)
			left_thread--
			go func() {
				defer wg.Done()
				defer live_thread.Add(-1)
				log_content := ""
				for i := 0; i < 2048; i++ {
					log_content += appender
					log_inst_async.Info(log_content)
				}
			}()
		} else {
			time.Sleep(time.Millisecond)
		}
	}
	wg.Wait()

	log_inst_console := bq.Create_log("console_log", "appenders_config.Appender1.type=console\n"+
		"appenders_config.Appender1.time_zone=localtime\n"+
		"appenders_config.Appender1.levels=[all]\n"+
		"log.thread_mode=sync\n", nil)
	bq.Set_console_buffer_enable(false)
	bq.Register_console_callback(func(log_id uint64, category_idx int32, level def.Log_level, content string) {
		if log_id != 0 {
			result.add_result(log_id == log_inst_console.Get_id(), "console callback test 1")
			result.add_result(level == def.Debug, "console callback test 2")
			result.add_result(strings.HasSuffix(content, "ConsoleTest"), "console callback test 3")
		}
	})
	log_inst_console.Debug("ConsoleTest")
	bq.Register_console_callback(nil)
	log_inst_async.Force_flush()
	result.add_result(true, "")
	return result
}
