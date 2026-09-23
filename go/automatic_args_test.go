// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
package bq_test

import (
	"errors"
	"fmt"
	"io"
	"reflect"
	"strings"
	"sync/atomic"
	"testing"
	"unsafe"

	bq "github.com/Tencent/BqLog/go/v2"
)

type test_player struct{ Name string }

func (p *test_player) String() string { return "player:" + p.Name }

type counted_value struct{ calls atomic.Int32 }

func (v *counted_value) String() string { return fmt.Sprintf("call-%d", v.calls.Add(1)) }

type test_enum int32
type named_float float32
type named_string string
type named_bytes []byte
type named_nil_map map[string]int
type named_nil_func func()

func (named_nil_map) String() string  { panic("nil map String must not run") }
func (named_nil_func) String() string { panic("nil func String must not run") }

type panic_stringer struct{}

func (panic_stringer) String() string { panic("broken String") }

type custom_formatter struct{}

func (custom_formatter) Format(state fmt.State, verb rune) { _, _ = io.WriteString(state, "formatted") }

func Test_automatic_arguments(t *testing.T) {
	var nil_player *test_player
	var nil_error *test_error
	var player_interface fmt.Stringer = nil_player
	var nil_map map[string]int
	var nil_slice []int
	var nil_chan chan int
	var nil_func func()
	cases := []struct {
		name  string
		value any
		want  string
	}{
		{"nil", nil, "null"},
		{"nil-pointer", nil_player, "null"},
		{"typed-nil-interface", player_interface, "null"},
		{"typed-nil-error", nil_error, "null"},
		{"nil-map", nil_map, "null"},
		{"nil-slice", nil_slice, "null"},
		{"nil-channel", nil_chan, "null"},
		{"nil-function", nil_func, "null"},
		{"nil-unsafe-pointer", unsafe.Pointer(nil), "null"},
		{"named-nil-map", named_nil_map(nil), "null"},
		{"named-nil-function", named_nil_func(nil), "null"},
		{"string", "中文😀", "中文😀"},
		{"empty-string", "", ""},
		{"bool", true, "TRUE"},
		{"false", false, "FALSE"},
		{"int", int(-123), "-123"},
		{"int8", int8(-128), "-128"},
		{"uint8", uint8(255), "255"},
		{"int16", int16(-32768), "-32768"},
		{"uint16", uint16(65535), "65535"},
		{"int32", int32(-2147483648), "-2147483648"},
		{"uint32", uint32(4294967295), "4294967295"},
		{"int64", int64(-9223372036854775808), "-9223372036854775808"},
		{"uint64", ^uint64(0), "18446744073709551615"},
		{"uint", uint(123), "123"},
		{"float32", float32(1.25), "1.2500000"},
		{"float64", -2.5, "-2.500000000000000"},
		{"named-float", named_float(1.25), "1.2500000"},
		{"enum", test_enum(42), "42"},
		{"named-string", named_string("hello"), "hello"},
		{"stringer", &test_player{Name: "Alice"}, "player:Alice"},
		{"error", errors.New("request failed"), "request failed"},
		{"formatter", custom_formatter{}, "formatted"},
		{"struct", struct{ Score int }{42}, "{42}"},
		{"map", map[string]int{"score": 42}, "map[score:42]"},
		{"slice", []int{1, 2}, "[1 2]"},
		{"empty-slice", []int{}, "[]"},
		{"bytes", []byte("go"), "[103 111]"},
		{"named-bytes", named_bytes("go"), "[103 111]"},
		{"complex", complex(1, 2), "(1+2i)"},
		{"panic-stringer", panic_stringer{}, fmt.Sprint(panic_stringer{})},
		{"explicit-arg", bq.I32(42), "42"},
		{"zero-arg", bq.Arg{}, "null"},
	}
	for _, mode := range []string{"sync", "async"} {
		t.Run(mode, func(t *testing.T) {
			log := new_log(t, mode, "snapshot.buffer_size=1048576\nsnapshot.levels=[all]\n", nil)
			for _, tc := range cases {
				t.Run(tc.name, func(t *testing.T) {
					for _, count := range []int{1, 2, 3, 4, 5, 17} {
						args := make([]any, count)
						args[0] = tc.value
						for i := 1; i < count; i++ {
							args[i] = "pad"
						}
						format := tc.name + ":" + strings.TrimSuffix(strings.Repeat("{}|", count), "|")
						if !log.Info(format, args...) {
							t.Fatal("write failed")
						}
						expected := tc.name + ":" + tc.want + strings.Repeat("|pad", count-1) + "\n"
						if snapshot := log.Take_snapshot("gmt"); !strings.HasSuffix(snapshot, expected) {
							t.Fatalf("count=%d want suffix %q; got %q", count, expected, snapshot)
						}
					}
				})
			}
		})
	}
}

type test_error struct{}

func (*test_error) Error() string { panic("nil Error must not run") }

func Test_automatic_argument_count_and_conversions(t *testing.T) {
	log := new_log(t, "sync", "snapshot.buffer_size=1048576\nsnapshot.levels=[all]\n", nil)
	for _, count := range []int{0, 1, 2, 3, 4, 5, 12, 16, 17, 64, 1024} {
		value := &counted_value{}
		args := make([]any, count)
		for i := range args {
			args[i] = value
		}
		if !log.Info("count:"+strings.Repeat("{}|", count), args...) {
			t.Fatal("write failed")
		}
		if got := value.calls.Load(); got != int32(count) {
			t.Fatalf("count=%d converted %d times", count, got)
		}
		var expected strings.Builder
		expected.WriteString("count:")
		for i := 1; i <= count; i++ {
			fmt.Fprintf(&expected, "call-%d|", i)
		}
		expected.WriteByte('\n')
		if !strings.HasSuffix(log.Take_snapshot("gmt"), expected.String()) {
			t.Fatalf("incorrect output for %d args", count)
		}
	}
}

func Test_filtered_objects_are_not_converted(t *testing.T) {
	value := &counted_value{}
	for _, count := range []int{1, 4, 5, 17} {
		args := make([]any, count)
		for i := range args {
			args[i] = value
		}
		var invalid bq.Log
		if invalid.Info("{}", args...) {
			t.Fatal("invalid logger wrote")
		}
	}
	log := bq.Create_category_log("automatic_filtered",
		"appenders_config.Console.type=console\nappenders_config.Console.levels=[error]\n"+
			"log.categories_mask=[Enabled]\nlog.thread_mode=sync\n", []string{"", "Enabled", "Disabled"})
	if !log.Is_valid() {
		t.Fatal("creation failed")
	}
	log.Info("{}", value)
	log.Info_c(1, "{}", value)
	log.Error_c(2, "{}", value)
	log.Error_c(^uint32(0), "{}", value)
	if value.calls.Load() != 0 {
		t.Fatal("filtered object was converted")
	}
}

func Test_log_methods_accept_ordinary_values(t *testing.T) {
	log := new_log(t, "sync", "snapshot.buffer_size=65536\nsnapshot.levels=[all]\n", []string{"", "Player"})
	cat := &bq.Category_log{Log: log}
	for _, method := range []func(string, ...any) bool{log.Verbose, log.Debug, log.Info, log.Warning, log.Error, log.Fatal} {
		if !method("plain") || !method("auto {} {} {}", 42, "name", nil) {
			t.Fatal("ordinary logging failed")
		}
	}
	for _, method := range []func(uint32, string, ...any) bool{cat.Verbose_c, cat.Debug_c, cat.Info_c, cat.Warning_c, cat.Error_c, cat.Fatal_c} {
		if !method(1, "category {} {}", &test_player{Name: "Alice"}, nil) {
			t.Fatal("category logging failed")
		}
	}
	if !strings.Contains(log.Take_snapshot("gmt"), "player:Alice null") {
		t.Fatal("category conversion missing")
	}
	for _, name := range []string{"Write", "Do_log", "do_log"} {
		if _, exists := reflect.TypeOf(log).MethodByName(name); exists {
			t.Fatalf("internal method exposed: %s", name)
		}
	}
}
