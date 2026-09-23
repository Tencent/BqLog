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
package impl

/*
#cgo CFLAGS: -I${SRCDIR}/../../../../../include -DBQ_GO
#cgo LDFLAGS: -lBqLog
#cgo windows LDFLAGS: -static-libgcc
#include <stdlib.h>
#include <bq_log/misc/bq_log_c_api.h>

extern void bq_go_console_callback(uint64_t log_id, int32_t category_idx, int32_t log_level, char* content, int32_t length);
extern void bq_go_console_fetch_callback(uintptr_t param, uint64_t log_id, int32_t category_idx, int32_t log_level, char* content, int32_t length);

static void BQ_STDCALL bq_on_console_callback(uint64_t log_id, int32_t category_idx, bq_log_level log_level_value, const char* content, int32_t length)
{
	bq_go_console_callback(log_id, category_idx, (int32_t)log_level_value, (char*)content, length);
}

static void BQ_STDCALL bq_on_console_fetch_callback(void* param, uint64_t log_id, int32_t category_idx, bq_log_level log_level_value, const char* content, int32_t length)
{
	bq_go_console_fetch_callback(*(const uintptr_t*)param, log_id, category_idx, (int32_t)log_level_value, (char*)content, length);
}

static bq_console_callback bq_on_console_callback_ptr(void) { return bq_on_console_callback; }
static bool bq_fetch_console(uintptr_t handle) {
	return __api_fetch_and_remove_console_buffer(bq_on_console_fetch_callback, &handle);
}
*/
import "C"

import (
	"runtime"
	"runtime/cgo"
	"sync"
	"unsafe"
)

// Console_callback is the Go-side hook invoked from native callbacks.
type Console_callback func(log_id uint64, category_idx int32, level int32, content string)

var (
	callback_mu  sync.RWMutex
	console_hook Console_callback
)

func Set_console_hook(cb Console_callback) {
	callback_mu.Lock()
	console_hook = cb
	callback_mu.Unlock()
}

//export bq_go_console_callback
func bq_go_console_callback(log_id C.uint64_t, category_idx C.int32_t, log_level C.int32_t, content *C.char, length C.int32_t) {
	callback_mu.RLock()
	cb := console_hook
	callback_mu.RUnlock()
	if cb != nil {
		cb(uint64(log_id), int32(category_idx), int32(log_level), C.GoStringN(content, length))
	}
}

//export bq_go_console_fetch_callback
func bq_go_console_fetch_callback(handle C.uintptr_t, log_id C.uint64_t, category_idx C.int32_t, log_level C.int32_t, content *C.char, length C.int32_t) {
	cb := cgo.Handle(handle).Value().(Console_callback)
	if cb != nil {
		cb(uint64(log_id), int32(category_idx), int32(log_level), C.GoStringN(content, length))
	}
}

func Get_log_version() string {
	return C.GoString(C.__api_get_log_version())
}

func Enable_auto_crash_handler() {
	C.__api_enable_auto_crash_handler()
}

func Create_log(name, config string, category_names []string) uint64 {
	c_name := C.CString(name)
	defer C.free(unsafe.Pointer(c_name))
	c_config := C.CString(config)
	defer C.free(unsafe.Pointer(c_config))
	var cat_array **C.char
	if len(category_names) > 0 {
		cat_array = (**C.char)(C.malloc(C.size_t(len(category_names)) * C.size_t(unsafe.Sizeof(uintptr(0)))))
		defer C.free(unsafe.Pointer(cat_array))
		slots := unsafe.Slice(cat_array, len(category_names))
		for i, s := range category_names {
			slots[i] = C.CString(s)
		}
		defer func() {
			for _, p := range slots {
				C.free(unsafe.Pointer(p))
			}
		}()
	}
	return uint64(C.__api_create_log(c_name, c_config, C.uint32_t(len(category_names)), cat_array))
}

func Log_reset_config(name, config string) bool {
	c_name := C.CString(name)
	defer C.free(unsafe.Pointer(c_name))
	c_config := C.CString(config)
	defer C.free(unsafe.Pointer(c_config))
	return bool(C.__api_log_reset_config(c_name, c_config))
}

// Go_log_write is the fused begin/copy/finish entry; args_data is the
// serialized args blob produced by the bq package.
func Go_log_write(log_id uint64, level uint8, category_index uint32, format string, args_data []byte) uint32 {
	var fmt_ptr unsafe.Pointer
	if len(format) > 0 {
		fmt_ptr = unsafe.Pointer(unsafe.StringData(format))
	}
	var args_ptr unsafe.Pointer
	if len(args_data) > 0 {
		args_ptr = unsafe.Pointer(&args_data[0])
	}
	return uint32(C.__api_go_log_write(C.uint64_t(log_id), C.uint8_t(level), C.uint32_t(category_index),
		C.uint32_t(len(format)), fmt_ptr, C.uint32_t(len(args_data)), args_ptr))
}

// Go_log_write_1 passes values and string pointers without a temporary Go buffer.
func Go_log_write_1(log_id uint64, level uint8, category_index uint32, format string, types uint32, value0 uint64, string0 string) uint32 {
	return uint32(C.__api_go_log_write_1(C.uint64_t(log_id), C.uint8_t(level), C.uint32_t(category_index),
		C.uint32_t(len(format)), unsafe.Pointer(unsafe.StringData(format)), C.uint32_t(types),
		C.uint64_t(value0), (*C.char)(unsafe.Pointer(unsafe.StringData(string0)))))
}

// Go_log_write_2 passes values and string pointers without a temporary Go buffer.
func Go_log_write_2(log_id uint64, level uint8, category_index uint32, format string, types uint32, value0 uint64, string0 string, value1 uint64, string1 string) uint32 {
	return uint32(C.__api_go_log_write_2(C.uint64_t(log_id), C.uint8_t(level), C.uint32_t(category_index),
		C.uint32_t(len(format)), unsafe.Pointer(unsafe.StringData(format)), C.uint32_t(types),
		C.uint64_t(value0), (*C.char)(unsafe.Pointer(unsafe.StringData(string0))),
		C.uint64_t(value1), (*C.char)(unsafe.Pointer(unsafe.StringData(string1)))))
}

// Go_log_write_4 passes values and string pointers without a temporary Go buffer.
func Go_log_write_4(log_id uint64, level uint8, category_index uint32, format string, count uint32, types uint32, value0 uint64, string0 string, value1 uint64, string1 string, value2 uint64, string2 string, value3 uint64, string3 string) uint32 {
	return uint32(C.__api_go_log_write_4(C.uint64_t(log_id), C.uint8_t(level), C.uint32_t(category_index),
		C.uint32_t(len(format)), unsafe.Pointer(unsafe.StringData(format)), C.uint32_t(count), C.uint32_t(types),
		C.uint64_t(value0), (*C.char)(unsafe.Pointer(unsafe.StringData(string0))),
		C.uint64_t(value1), (*C.char)(unsafe.Pointer(unsafe.StringData(string1))),
		C.uint64_t(value2), (*C.char)(unsafe.Pointer(unsafe.StringData(string2))),
		C.uint64_t(value3), (*C.char)(unsafe.Pointer(unsafe.StringData(string3)))))
}

func Log_device_console(level int32, content string) {
	c_content := C.CString(content)
	defer C.free(unsafe.Pointer(c_content))
	C.__api_log_device_console(C.bq_log_level(level), c_content)
}

func Force_flush(log_id uint64) {
	C.__api_force_flush(C.uint64_t(log_id))
}

func Get_file_base_dir(base_dir_type int32) string {
	return C.GoString(C.__api_get_file_base_dir(C.int32_t(base_dir_type)))
}

func Reset_base_dir(base_dir_type int32, dir string) {
	c_dir := C.CString(dir)
	defer C.free(unsafe.Pointer(c_dir))
	C.__api_reset_base_dir(C.int32_t(base_dir_type), c_dir)
}

func Get_logs_count() uint32 {
	return uint32(C.__api_get_logs_count())
}

func Get_log_id_by_index(index uint32) uint64 {
	return uint64(C.__api_get_log_id_by_index(C.uint32_t(index)))
}

func Get_log_name_by_id(log_id uint64) (string, bool) {
	var name_def C.bq_api_string_def
	if C.__api_get_log_name_by_id(C.uint64_t(log_id), &name_def) {
		return C.GoStringN(name_def.str, C.int(name_def.len)), true
	}
	return "", false
}

func Get_log_categories_count(log_id uint64) uint32 {
	return uint32(C.__api_get_log_categories_count(C.uint64_t(log_id)))
}

func Get_log_category_name_by_index(log_id uint64, category_index uint32) (string, bool) {
	var name_def C.bq_api_string_def
	if C.__api_get_log_category_name_by_index(C.uint64_t(log_id), C.uint32_t(category_index), &name_def) {
		return C.GoStringN(name_def.str, C.int(name_def.len)), true
	}
	return "", false
}

// The returned pointers reference native memory owned by the log object;
// reads from Go are plain memory loads with no cgo call.
func Get_log_merged_log_level_bitmap(log_id uint64) *uint32 {
	return (*uint32)(unsafe.Pointer(C.__api_get_log_merged_log_level_bitmap_by_log_id(C.uint64_t(log_id))))
}

func Get_log_print_stack_level_bitmap(log_id uint64) *uint32 {
	return (*uint32)(unsafe.Pointer(C.__api_get_log_print_stack_level_bitmap_by_log_id(C.uint64_t(log_id))))
}

func Get_log_category_masks_array(log_id uint64) *uint8 {
	return (*uint8)(unsafe.Pointer(C.__api_get_log_category_masks_array_by_log_id(C.uint64_t(log_id))))
}

func Take_snapshot_string(log_id uint64, time_zone_config string) string {
	// Native snapshot locks must not outlive the OS-thread affinity of a call.
	runtime.LockOSThread()
	defer runtime.UnlockOSThread()
	c_tz := C.CString(time_zone_config)
	defer C.free(unsafe.Pointer(c_tz))
	var snapshot C.bq_api_string_def
	C.__api_take_snapshot_string(C.uint64_t(log_id), c_tz, &snapshot)
	if snapshot.str == nil {
		return ""
	}
	defer C.__api_release_snapshot_string(C.uint64_t(log_id), &snapshot)
	return C.GoStringN(snapshot.str, C.int(snapshot.len))
}

func Log_decoder_create(log_file_path, priv_key string) (uint32, bool) {
	c_path := C.CString(log_file_path)
	defer C.free(unsafe.Pointer(c_path))
	var c_key *C.char
	if len(priv_key) > 0 {
		c_key = C.CString(priv_key)
		defer C.free(unsafe.Pointer(c_key))
	}
	var handle C.uint32_t
	rc := C.__api_log_decoder_create(c_path, c_key, &handle)
	return uint32(handle), rc == 0
}

func Log_decoder_decode(handle uint32) (string, bool) {
	var out C.bq_api_string_def
	rc := C.__api_log_decoder_decode(C.uint32_t(handle), &out)
	if rc != 0 {
		return "", false
	}
	return C.GoStringN(out.str, C.int(out.len)), true
}

func Log_decoder_destroy(handle uint32) {
	C.__api_log_decoder_destroy(C.uint32_t(handle))
}

func Log_decode(in_file_path, out_file_path, priv_key string) bool {
	c_in := C.CString(in_file_path)
	defer C.free(unsafe.Pointer(c_in))
	c_out := C.CString(out_file_path)
	defer C.free(unsafe.Pointer(c_out))
	var c_key *C.char
	if len(priv_key) > 0 {
		c_key = C.CString(priv_key)
		defer C.free(unsafe.Pointer(c_key))
	}
	return bool(C.__api_log_decode(c_in, c_out, c_key))
}

func Register_console_callback() {
	C.__api_register_console_callbacks(C.bq_on_console_callback_ptr())
}

func Unregister_console_callback() {
	C.__api_unregister_console_callbacks(C.bq_on_console_callback_ptr())
}

func Set_console_buffer_enable(enable bool) {
	C.__api_set_console_buffer_enable(C.bool(enable))
}

func Fetch_and_remove_console_buffer(callback Console_callback) bool {
	handle := cgo.NewHandle(callback)
	defer handle.Delete()
	return bool(C.bq_fetch_console(C.uintptr_t(handle)))
}
