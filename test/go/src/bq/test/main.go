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
	"os"

	bq "github.com/Tencent/BqLog/wrapper/go/src/bq"
	"github.com/Tencent/BqLog/wrapper/go/src/bq/def"
)

func main() {
	fmt.Println("Running Go Wrapper Tests...")

	version := bq.Get_version()
	fmt.Println("BqLog Version: " + version)
	if len(version) == 0 {
		fmt.Println("Failed to get version")
		os.Exit(-1)
	}

	start_console_fetcher()

	tests := []struct {
		name string
		fn   func() *test_result
	}{
		{"Test Log Basic", test_log_basic},
		{"Test Log MultiGoroutine", test_log_multi_goroutine},
		{"Test Log Category", test_log_category},
	}

	success := true
	for _, t := range tests {
		func() {
			defer func() {
				if r := recover(); r != nil {
					fmt.Println("test panic:", r)
					success = false
				}
			}()
			result := t.fn()
			result.output(t.name)
			success = success && result.is_all_pass()
		}()
	}

	if success {
		bq.Console(def.Info, "--------------------------------")
		bq.Console(def.Info, "CONGRATULATION!!! ALL TEST CASES IS PASSED")
		bq.Console(def.Info, "--------------------------------")
	} else {
		bq.Console(def.Error, "--------------------------------")
		bq.Console(def.Error, "SORRY!!! TEST CASES FAILED")
		bq.Console(def.Error, "--------------------------------")
		os.Exit(-1)
	}
}
