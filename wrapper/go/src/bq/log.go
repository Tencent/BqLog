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
	"unsafe"

	"github.com/Tencent/BqLog/wrapper/go/src/bq/def"
	"github.com/Tencent/BqLog/wrapper/go/src/bq/impl"
)

// Log is a handle to a bqLog instance. The zero value is invalid; create one
// with Create_log / Get_log_by_name.
type Log struct {
	id           uint64
	level_bitmap *uint32
	stack_bitmap *uint32
	masks        *uint8
	cat_count    uint32
}

func (l *Log) refresh(id uint64) {
	l.id = id
	l.level_bitmap = impl.Get_log_merged_log_level_bitmap(id)
	l.stack_bitmap = impl.Get_log_print_stack_level_bitmap(id)
	l.masks = impl.Get_log_category_masks_array(id)
	l.cat_count = impl.Get_log_categories_count(id)
}

// Get_version returns the BqLog library version.
func Get_version() string {
	return impl.Get_log_version()
}

// Enable_auto_crash_handle makes bqLog try to force flush buffered logs on crash.
func Enable_auto_crash_handle() {
	impl.Enable_auto_crash_handler()
}

// Get_file_base_dir returns the absolute base dir for relative-path logs.
func Get_file_base_dir(base_dir_type int32) string {
	return impl.Get_file_base_dir(base_dir_type)
}

// Reset_base_dir changes the base dir used by relative-path logs.
func Reset_base_dir(base_dir_type int32, dir string) {
	impl.Reset_base_dir(base_dir_type, dir)
}

// Create_log creates (or returns and reconfigures an existing) log object.
// category_names may be nil for a plain log. If creation fails, Is_valid()
// of the returned object reports false.
func Create_log(name, config string, category_names []string) *Log {
	if len(config) == 0 {
		return &Log{}
	}
	id := impl.Create_log(name, config, category_names)
	if id == 0 {
		return &Log{}
	}
	l := &Log{}
	l.refresh(id)
	return l
}

// Reset_config overwrites the config of an existing log by name.
func Reset_config(name, config string) bool {
	return impl.Log_reset_config(name, config)
}

// Get_logs_count returns how many log objects exist.
func Get_logs_count() int {
	return int(impl.Get_logs_count())
}

// Get_log_by_name finds an existing log object by name.
func Get_log_by_name(name string) *Log {
	count := impl.Get_logs_count()
	for i := uint32(0); i < count; i++ {
		id := impl.Get_log_id_by_index(i)
		if log_name, ok := impl.Get_log_name_by_id(id); ok && log_name == name {
			l := &Log{}
			l.refresh(id)
			return l
		}
	}
	return &Log{}
}

func (l *Log) Is_valid() bool { return l != nil && l.id != 0 }

func (l *Log) Get_id() uint64 { return l.id }

func (l *Log) Get_categories_count() int {
	return int(impl.Get_log_categories_count(l.id))
}

func (l *Log) Get_category_name(index int) string {
	name, _ := impl.Get_log_category_name_by_index(l.id, uint32(index))
	return name
}

// Is_enable_for reports whether a log with the given level (and category)
// would actually be written, cheap enough to call before building arguments.
func (l *Log) Is_enable_for(level def.Log_level, category_index uint32) bool {
	if !l.Is_valid() || level < def.Verbose || level > def.Fatal || category_index >= l.cat_count {
		return false
	}
	if *l.level_bitmap&(1<<uint32(level)) == 0 {
		return false
	}
	return *(*uint8)(unsafe.Add(unsafe.Pointer(l.masks), category_index)) != 0
}

// do_log is shared by the level and category methods. It is private to package bq.
func (l *Log) do_log(level def.Log_level, category_index uint32, format string, args ...any) bool {
	if !l.Is_enable_for(level, category_index) {
		return false
	}
	if *l.stack_bitmap&(1<<uint32(level)) != 0 {
		format += capture_stack()
	}
	// Native entry lengths are uint32_t, including the header and thread name.
	const max_entry_payload = uint64(1<<32 - 1 - 1024)
	if uint64(len(format)) > max_entry_payload {
		return false
	}
	if len(args) == 0 {
		return impl.Go_log_write(l.id, uint8(level), category_index, format, nil) == 0
	}
	if len(args) <= 4 {
		return l.write_small(level, category_index, format, args) == 0
	}
	return l.write_many(level, category_index, format, args)
}

func (l *Log) write_many(level def.Log_level, category_index uint32, format string, args []any) bool {
	// Convert once, including any String/Error methods, before calculating
	// storage. Isolate this scratch space from the common 0–4 argument path.
	var local [16]Arg
	converted := local[:]
	if len(args) > len(local) {
		converted = make([]Arg, len(args))
	}
	converted = converted[:len(args)]
	for i := range args {
		converted[i] = make_arg(args[i])
	}
	size := args_size(converted)
	if size < 0 || uint64(size)+uint64(len(format)) > uint64(1<<32-1-1024) {
		return false
	}
	buffer := acquire_args_buffer(size)
	defer release_args_buffer(buffer)
	args_data := buffer.data[:size]
	serialize_args(args_data, converted)
	return impl.Go_log_write(l.id, uint8(level), category_index, format, args_data) == 0
}

// Pass string pointers as individual cgo arguments so the Go runtime pins
// them for the call. Never pass a Go array containing strings/pointers to C.
func (l *Log) write_small(level def.Log_level, category_index uint32, format string, args []any) uint32 {
	a0 := make_arg(args[0])
	types := uint32(a0.typ)
	if len(args) == 1 {
		return impl.Go_log_write_1(l.id, uint8(level), category_index, format, types, a0.pod, a0.str)
	}
	a1 := make_arg(args[1])
	types |= uint32(a1.typ) << 8
	if len(args) == 2 {
		return impl.Go_log_write_2(l.id, uint8(level), category_index, format, types, a0.pod, a0.str, a1.pod, a1.str)
	}
	a2 := make_arg(args[2])
	var a3 Arg
	if len(args) == 4 {
		a3 = make_arg(args[3])
	}
	types |= uint32(a2.typ)<<16 | uint32(a3.typ)<<24
	return impl.Go_log_write_4(l.id, uint8(level), category_index, format, uint32(len(args)), types,
		a0.pod, a0.str, a1.pod, a1.str, a2.pod, a2.str, a3.pod, a3.str)
}

func (l *Log) Verbose(format string, args ...any) bool {
	return l.do_log(def.Verbose, 0, format, args...)
}
func (l *Log) Debug(format string, args ...any) bool { return l.do_log(def.Debug, 0, format, args...) }
func (l *Log) Info(format string, args ...any) bool  { return l.do_log(def.Info, 0, format, args...) }
func (l *Log) Warning(format string, args ...any) bool {
	return l.do_log(def.Warning, 0, format, args...)
}
func (l *Log) Error(format string, args ...any) bool { return l.do_log(def.Error, 0, format, args...) }
func (l *Log) Fatal(format string, args ...any) bool { return l.do_log(def.Fatal, 0, format, args...) }

// Force_flush makes bqLog flush buffered logs of this log object.
func (l *Log) Force_flush() {
	impl.Force_flush(l.id)
}

// Take_snapshot returns buffered recent logs as text (requires snapshot config).
func (l *Log) Take_snapshot(time_zone_config string) string {
	return impl.Take_snapshot_string(l.id, time_zone_config)
}

// Console writes one line to the device console directly.
func Console(level def.Log_level, content string) {
	impl.Log_device_console(int32(level), content)
}

// Decoder decodes encrypted/compressed bqLog files.
type Decoder struct {
	handle uint32
	valid  bool
}

// Decoder_create opens a decoder for a bqLog file. Is_valid reports whether
// it succeeded.
func Decoder_create(log_file_path, priv_key string) *Decoder {
	handle, ok := impl.Log_decoder_create(log_file_path, priv_key)
	return &Decoder{handle: handle, valid: ok}
}

func (d *Decoder) Is_valid() bool { return d.valid }

// Decode returns (text, true) while there is content, ("", false) on eof/error.
func (d *Decoder) Decode() (string, bool) {
	if !d.valid {
		return "", false
	}
	return impl.Log_decoder_decode(d.handle)
}

func (d *Decoder) Destroy() {
	if d.valid {
		impl.Log_decoder_destroy(d.handle)
		d.valid = false
	}
}

// Decode_file decodes in_file_path into out_file_path in one shot.
func Decode_file(in_file_path, out_file_path, priv_key string) bool {
	return impl.Log_decode(in_file_path, out_file_path, priv_key)
}
