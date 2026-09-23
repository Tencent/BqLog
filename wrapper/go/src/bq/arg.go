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
	"fmt"
	"math"
	"reflect"
	"unsafe"
)

// Arg is an optional explicit representation of a log parameter. Log methods
// also accept ordinary Go values, so callers do not need these constructors.
// The zero Arg represents null.
type Arg struct {
	typ uint8
	pod uint64
	str string
}

// values mirror bq::log_arg_type_enum
const (
	arg_type_null        uint8 = 1
	arg_type_pointer     uint8 = 2
	arg_type_bool        uint8 = 3
	arg_type_int8        uint8 = 7
	arg_type_uint8       uint8 = 8
	arg_type_int16       uint8 = 9
	arg_type_uint16      uint8 = 10
	arg_type_int32       uint8 = 11
	arg_type_uint32      uint8 = 12
	arg_type_int64       uint8 = 13
	arg_type_uint64      uint8 = 14
	arg_type_float       uint8 = 15
	arg_type_double      uint8 = 16
	arg_type_string_utf8 uint8 = 17
)

func Nil() Arg          { return Arg{typ: arg_type_null} }
func Ptr(v uintptr) Arg { return Arg{typ: arg_type_pointer, pod: uint64(v)} }
func Bool(v bool) Arg {
	if v {
		return Arg{typ: arg_type_bool, pod: 1}
	}
	return Arg{typ: arg_type_bool}
}
func I8(v int8) Arg     { return Arg{typ: arg_type_int8, pod: uint64(uint8(v))} }
func U8(v uint8) Arg    { return Arg{typ: arg_type_uint8, pod: uint64(v)} }
func I16(v int16) Arg   { return Arg{typ: arg_type_int16, pod: uint64(uint16(v))} }
func U16(v uint16) Arg  { return Arg{typ: arg_type_uint16, pod: uint64(v)} }
func I32(v int32) Arg   { return Arg{typ: arg_type_int32, pod: uint64(uint32(v))} }
func U32(v uint32) Arg  { return Arg{typ: arg_type_uint32, pod: uint64(v)} }
func I64(v int64) Arg   { return Arg{typ: arg_type_int64, pod: uint64(v)} }
func U64(v uint64) Arg  { return Arg{typ: arg_type_uint64, pod: v} }
func Int(v int) Arg     { return I64(int64(v)) }
func Uint(v uint) Arg   { return U64(uint64(v)) }
func F32(v float32) Arg { return Arg{typ: arg_type_float, pod: uint64(math.Float32bits(v))} }
func F64(v float64) Arg { return Arg{typ: arg_type_double, pod: math.Float64bits(v)} }
func Str(v string) Arg  { return Arg{typ: arg_type_string_utf8, pod: uint64(len(v)), str: v} }

// Convert each value once, only after level/category filtering. Built-in values
// retain their native types so BqLog still performs formatting asynchronously.
func make_arg(value any) Arg {
	switch v := value.(type) {
	case nil:
		return Nil()
	case Arg:
		return v
	case string:
		return Str(v)
	case bool:
		return Bool(v)
	case int:
		return Int(v)
	case int8:
		return I8(v)
	case int16:
		return I16(v)
	case int32:
		return I32(v)
	case int64:
		return I64(v)
	case uint:
		return Uint(v)
	case uint8:
		return U8(v)
	case uint16:
		return U16(v)
	case uint32:
		return U32(v)
	case uint64:
		return U64(v)
	case uintptr:
		return Ptr(v)
	case float32:
		return F32(v)
	case float64:
		return F64(v)
	case unsafe.Pointer:
		if v == nil {
			return Nil()
		}
		return Ptr(uintptr(v))
	default:
		return make_object_arg(value)
	}
}

func make_object_arg(value any) Arg {
	v := reflect.ValueOf(value)
	switch v.Kind() {
	case reflect.Chan, reflect.Func, reflect.Interface, reflect.Map, reflect.Pointer, reflect.Slice, reflect.UnsafePointer:
		if v.IsNil() {
			// An interface holding a typed nil must not invoke String/Error.
			return Nil()
		}
	}
	switch value.(type) {
	case fmt.Formatter, error, fmt.Stringer:
		// fmt handles conversion panics using its standard diagnostic text.
		return Str(fmt.Sprint(value))
	}
	// Named scalar types (for example an enum) keep their underlying encoding,
	// unless the type explicitly supplies its own textual representation above.
	switch v.Kind() {
	case reflect.String:
		return Str(v.String())
	case reflect.Bool:
		return Bool(v.Bool())
	case reflect.Int:
		return Int(int(v.Int()))
	case reflect.Int8:
		return I8(int8(v.Int()))
	case reflect.Int16:
		return I16(int16(v.Int()))
	case reflect.Int32:
		return I32(int32(v.Int()))
	case reflect.Int64:
		return I64(v.Int())
	case reflect.Uint:
		return Uint(uint(v.Uint()))
	case reflect.Uint8:
		return U8(uint8(v.Uint()))
	case reflect.Uint16:
		return U16(uint16(v.Uint()))
	case reflect.Uint32:
		return U32(uint32(v.Uint()))
	case reflect.Uint64:
		return U64(v.Uint())
	case reflect.Uintptr:
		return Ptr(uintptr(v.Uint()))
	case reflect.Float32:
		return F32(float32(v.Float()))
	case reflect.Float64:
		return F64(v.Float())
	default:
		return Str(fmt.Sprint(value))
	}
}

func align4(n int) int {
	if n == 0 {
		return 0
	}
	return ((n - 1) &^ 3) + 4
}

// serialized size of one arg, same layout as the C#/Java wrappers:
// 1 byte type, payload at offset 2 (<=2 bytes) or 4, strings aligned to 4.
func (a *Arg) size() int {
	switch a.typ {
	case 0, arg_type_null, arg_type_bool, arg_type_int8, arg_type_uint8, arg_type_int16, arg_type_uint16:
		return 4
	case arg_type_int32, arg_type_uint32, arg_type_float:
		return 8
	case arg_type_int64, arg_type_uint64, arg_type_double, arg_type_pointer:
		return 12
	default: // string
		return align4(8 + len(a.str))
	}
}

func args_size(args []Arg) int {
	n := 0
	for i := range args {
		size := args[i].size()
		if size < 0 || n > int(^uint(0)>>1)-size {
			return -1
		}
		n += size
	}
	return n
}

func (a *Arg) write_to(buf []byte) {
	// Only clear padding; the payload below overwrites every other byte.
	// buf is restricted to this argument's serialized size.
	clear(buf[:4])
	buf[0] = a.typ
	switch a.typ {
	case 0:
		buf[0] = arg_type_null
	case arg_type_null:
	case arg_type_bool, arg_type_int8, arg_type_uint8:
		buf[2] = byte(a.pod)
	case arg_type_int16, arg_type_uint16:
		le16(buf[2:], uint16(a.pod))
	case arg_type_int32, arg_type_uint32, arg_type_float:
		le32(buf[4:], uint32(a.pod))
	case arg_type_int64, arg_type_uint64, arg_type_double, arg_type_pointer:
		le64(buf[4:], a.pod)
	default: // string
		le32(buf[4:], uint32(len(a.str)))
		copy(buf[8:], a.str)
		clear(buf[8+len(a.str):])
	}
}

func serialize_args(dst []byte, args []Arg) {
	for i := range args {
		a := &args[i]
		size := a.size()
		a.write_to(dst[:size])
		dst = dst[size:]
	}
}

func le16(b []byte, v uint16) { b[0] = byte(v); b[1] = byte(v >> 8) }
func le32(b []byte, v uint32) {
	b[0] = byte(v)
	b[1] = byte(v >> 8)
	b[2] = byte(v >> 16)
	b[3] = byte(v >> 24)
}
func le64(b []byte, v uint64) {
	b[0] = byte(v)
	b[1] = byte(v >> 8)
	b[2] = byte(v >> 16)
	b[3] = byte(v >> 24)
	b[4] = byte(v >> 32)
	b[5] = byte(v >> 40)
	b[6] = byte(v >> 48)
	b[7] = byte(v >> 56)
}
