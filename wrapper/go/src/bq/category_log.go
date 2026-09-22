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
)

// Category_log attaches a category index to each entry; categories are the
// strings passed to Create_log.
type Category_log struct {
	*Log
}

func Create_category_log(name, config string, category_names []string) *Category_log {
	l := Create_log(name, config, category_names)
	return &Category_log{Log: l}
}

func (l *Category_log) Write_c(level def.Log_level, category_index uint32, format string, args ...Arg) bool {
	return l.Write(level, category_index, format, args...)
}

func (l *Category_log) Verbose_c(category_index uint32, format string, args ...Arg) bool {
	return l.Write(def.Verbose, category_index, format, args...)
}
func (l *Category_log) Debug_c(category_index uint32, format string, args ...Arg) bool {
	return l.Write(def.Debug, category_index, format, args...)
}
func (l *Category_log) Info_c(category_index uint32, format string, args ...Arg) bool {
	return l.Write(def.Info, category_index, format, args...)
}
func (l *Category_log) Warning_c(category_index uint32, format string, args ...Arg) bool {
	return l.Write(def.Warning, category_index, format, args...)
}
func (l *Category_log) Error_c(category_index uint32, format string, args ...Arg) bool {
	return l.Write(def.Error, category_index, format, args...)
}
func (l *Category_log) Fatal_c(category_index uint32, format string, args ...Arg) bool {
	return l.Write(def.Fatal, category_index, format, args...)
}
