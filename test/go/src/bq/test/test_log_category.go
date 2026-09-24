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
	"strings"
)

func test_log_category() *test_result {
	result := &test_result{}

	cat_log := Create_test_category_log("cat_test_1",
		"appenders_config.ConsoleAppender.type=console\n"+
			"appenders_config.ConsoleAppender.time_zone=localtime\n"+
			"appenders_config.ConsoleAppender.levels=[all]\n"+
			"log.thread_mode=sync\n"+
			"snapshot.buffer_size=65536\n"+
			"snapshot.levels=[all]\n")

	snapshot_before := cat_log.Take_snapshot("gmt")

	cat_log.Info_c(cat_log.Cat.ModuleA.SystemA, "Hello Category")
	snapshot1 := cat_log.Take_snapshot("gmt")
	result.add_result(
		snapshot1 != snapshot_before &&
			strings.Contains(snapshot1, "[ModuleA.SystemA]") &&
			strings.HasSuffix(snapshot1, "Hello Category\n"),
		"category output test")

	cat_log.Error_c(cat_log.Cat.ModuleA.SystemA.ClassA, "Deep Category")
	snapshot2 := cat_log.Take_snapshot("gmt")
	result.add_result(
		snapshot2 != snapshot1 &&
			strings.Contains(snapshot2, "[ModuleA.SystemA.ClassA]") &&
			strings.HasSuffix(snapshot2, "Deep Category\n"),
		"deep category test")

	cat_log.Info_c(cat_log.Cat.ModuleB, "Param test: {}, {}", "hello", int32(42))
	snapshot3 := cat_log.Take_snapshot("gmt")
	result.add_result(
		snapshot3 != snapshot2 &&
			strings.Contains(snapshot3, "[ModuleB]") &&
			strings.HasSuffix(snapshot3, "Param test: hello, 42\n"),
		"category param test")

	masked_log := Create_test_category_log("cat_test_mask",
		"appenders_config.ConsoleAppender.type=console\n"+
			"appenders_config.ConsoleAppender.time_zone=localtime\n"+
			"appenders_config.ConsoleAppender.levels=[all]\n"+
			"log.thread_mode=sync\n"+
			"log.categories_mask=[ModuleA.SystemA.ClassA,ModuleB]\n"+
			"snapshot.buffer_size=65536\n"+
			"snapshot.levels=[all]\n"+
			"snapshot.categories_mask=[ModuleA.SystemA.ClassA,ModuleB]\n")

	snapshot_mask_before := masked_log.Take_snapshot("gmt")

	masked_log.Info_c(masked_log.Cat.ModuleA.SystemA, "should be filtered")
	snapshot_mask_filtered := masked_log.Take_snapshot("gmt")
	result.add_result(
		snapshot_mask_filtered == snapshot_mask_before,
		"category mask filter test")

	masked_log.Info_c(masked_log.Cat.ModuleA.SystemA.ClassA, "should pass")
	snapshot_mask_pass := masked_log.Take_snapshot("gmt")
	result.add_result(
		snapshot_mask_pass != snapshot_mask_filtered &&
			strings.Contains(snapshot_mask_pass, "should pass"),
		"category mask pass test")

	masked_log.Info_c(masked_log.Cat.ModuleB, "ModuleB pass")
	snapshot_mask_module_b := masked_log.Take_snapshot("gmt")
	result.add_result(
		snapshot_mask_module_b != snapshot_mask_pass &&
			strings.Contains(snapshot_mask_module_b, "ModuleB pass"),
		"category mask ModuleB test")

	return result
}
