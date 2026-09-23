// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
package bq_test

import (
	"fmt"
	"path/filepath"
	"runtime"
	"strings"
	"sync"
	"sync/atomic"
	"testing"

	bq "github.com/Tencent/BqLog/wrapper/go/src/bq"
	"github.com/Tencent/BqLog/wrapper/go/src/bq/def"
)

var next_log atomic.Uint64

func new_log(t testing.TB, mode, extra string, categories []string) *bq.Log {
	t.Helper()
	name := fmt.Sprintf("go_test_%d", next_log.Add(1))
	path := filepath.ToSlash(filepath.Join(t.TempDir(), name))
	config := "appenders_config.File.type=compressed_file\n" +
		"appenders_config.File.file_name=" + path + "\n" +
		"appenders_config.File.levels=[all]\n" +
		"log.thread_mode=" + mode + "\n" +
		"log.buffer_policy_when_full=expand\n" + extra
	log := bq.Create_log(name, config, categories)
	if !log.Is_valid() {
		t.Fatal("Create_log failed")
	}
	log_dirs.Store(log.Get_id(), filepath.Dir(path))
	t.Cleanup(func() {
		log.Force_flush()
		// Reconfiguration closes appenders before TempDir removes their files.
		if !bq.Reset_config(name, "appenders_config.Closed.type=console\nappenders_config.Closed.levels=[]\nlog.thread_mode="+mode+"\n") {
			t.Error("failed to close test appenders")
		}
		log_dirs.Delete(log.Get_id())
	})
	return log
}

func TestConcurrentArguments(t *testing.T) {
	for _, mode := range []string{"sync", "async", "independent"} {
		t.Run(mode, func(t *testing.T) {
			log := new_log(t, mode, "", nil)
			const workers, count = 8, 40
			var failed atomic.Int32
			var wg sync.WaitGroup
			for worker := 0; worker < workers; worker++ {
				wg.Add(1)
				go func(worker int) {
					defer wg.Done()
					for index := 0; index < count; index++ {
						payload := strings.Repeat(string(rune('a'+worker)), 17)
						if index%10 == 0 {
							payload = strings.Repeat(string(rune('a'+worker)), 5000)
						}
						var written bool
						if index%2 == 0 {
							written = log.Info("record|{}|{}|{}|{}", bq.Int(worker), bq.Int(index), bq.I64(-int64(worker*count+index)), bq.Str(payload))
						} else {
							// Exercise pooled serialization concurrently with direct writes.
							written = log.Info("record|{}|{}|{}|{}{}{}", bq.Int(worker), bq.Int(index), bq.I64(-int64(worker*count+index)), bq.Str(payload), bq.Str(""), bq.Str(""))
						}
						if !written {
							failed.Add(1)
						}
					}
				}(worker)
			}
			wg.Wait()
			log.Force_flush()
			if failed.Load() != 0 {
				t.Fatalf("%d writes failed", failed.Load())
			}
			// The base path is absolute, and every test owns its native log files.
			files, err := filepath.Glob(filepath.Join(test_log_dir(log), "*"))
			if err != nil {
				t.Fatal(err)
			}
			seen := make(map[string]bool)
			for _, file := range files {
				decoder := bq.Decoder_create(file, "")
				if !decoder.Is_valid() {
					continue
				}
				for {
					line, ok := decoder.Decode()
					if !ok {
						break
					}
					start := strings.Index(line, "record|")
					if start < 0 {
						continue
					}
					record := strings.TrimRight(line[start:], "\r\n")
					parts := strings.SplitN(record, "|", 5)
					if len(parts) != 5 || seen[record] {
						t.Errorf("duplicate or malformed record: %.100s", record)
					}
					seen[record] = true
				}
				decoder.Destroy()
			}
			for worker := 0; worker < workers; worker++ {
				for index := 0; index < count; index++ {
					size := 17
					if index%10 == 0 {
						size = 5000
					}
					expected := fmt.Sprintf("record|%d|%d|%d|%s", worker, index, -(worker*count + index), strings.Repeat(string(rune('a'+worker)), size))
					if !seen[expected] {
						t.Errorf("missing or corrupted record worker=%d index=%d", worker, index)
					}
				}
			}
			if len(seen) != workers*count {
				t.Errorf("got %d records, want %d", len(seen), workers*count)
			}
		})
	}
}

var log_dirs sync.Map

func test_log_dir(log *bq.Log) string {
	dir, _ := log_dirs.Load(log.Get_id())
	return dir.(string)
}

func TestInvalidCategoryAndLevel(t *testing.T) {
	log := new_log(t, "sync", "snapshot.buffer_size=65536\nsnapshot.levels=[all]\n", []string{"", "ModuleA"})
	cat_log := &bq.Category_log{Log: log}
	for _, category := range []uint32{2, ^uint32(0)} {
		if log.Is_enable_for(def.Info, category) || cat_log.Info_c(category, "invalid") {
			t.Errorf("accepted category %d", category)
		}
	}
	for _, level := range []def.Log_level{-1, 6, 32, 256} {
		if log.Is_enable_for(level, 0) {
			t.Errorf("accepted level %d", level)
		}
	}
	if !cat_log.Info_c(1, "valid") || !strings.Contains(log.Take_snapshot("gmt"), "[ModuleA]") {
		t.Fatal("valid category did not reach snapshot")
	}
}

//go:noinline
func write_stack_probe(log *bq.Log) { log.Error("stack marker {}", bq.Int(42)) }

func TestGoStackLevels(t *testing.T) {
	log := new_log(t, "sync", "log.print_stack_levels=[error]\nsnapshot.buffer_size=65536\nsnapshot.levels=[all]\n", nil)
	log.Info("without stack")
	if strings.Contains(log.Take_snapshot("gmt"), "log_test.go:") {
		t.Fatal("stack emitted for disabled level")
	}
	write_stack_probe(log)
	snapshot := log.Take_snapshot("gmt")
	if !strings.Contains(snapshot, "stack marker 42") || !strings.Contains(snapshot, "write_stack_probe") || !strings.Contains(snapshot, "log_test.go:") {
		t.Fatalf("Go caller stack missing: %s", snapshot)
	}
}

func TestConsoleCallbackCount(t *testing.T) {
	bq.Set_console_buffer_enable(false)
	log := bq.Create_log(fmt.Sprintf("callback_%d", next_log.Add(1)), "appenders_config.Console.type=console\nappenders_config.Console.levels=[all]\nlog.thread_mode=sync\n", nil)
	var count atomic.Int32
	bq.Register_console_callback(func(id uint64, _ int32, level def.Log_level, content string) {
		if id == log.Get_id() {
			count.Add(1)
			if level != def.Info || !strings.HasSuffix(content, "callback 42") {
				t.Errorf("unexpected callback: %s", content)
			}
		}
	})
	defer bq.Register_console_callback(nil)
	if !log.Info("callback {}", bq.Int(42)) || count.Load() != 1 {
		t.Fatalf("callback count=%d, want 1", count.Load())
	}
	bq.Register_console_callback(nil)
}

func TestZeroArgument(t *testing.T) {
	log := new_log(t, "sync", "snapshot.buffer_size=65536\nsnapshot.levels=[all]\n", nil)
	log.Info("zero={}, next={}", bq.Arg{}, bq.Int(42))
	if !strings.Contains(log.Take_snapshot("gmt"), "zero=null, next=42") {
		t.Fatal("zero Arg corrupted the following argument")
	}
}

func TestArgumentTypesAndFallback(t *testing.T) {
	log := new_log(t, "sync", "snapshot.buffer_size=1048576\nsnapshot.levels=[all]\n", nil)
	cases := []struct {
		name string
		args []bq.Arg
		want string
	}{
		{"null", []bq.Arg{bq.Arg{}}, "null"},
		{"bool", []bq.Arg{bq.Bool(false), bq.Bool(true)}, "FALSE|TRUE"},
		{"int8", []bq.Arg{bq.I8(-128), bq.U8(255)}, "-128|255"},
		{"int16", []bq.Arg{bq.I16(-32768), bq.U16(65535)}, "-32768|65535"},
		{"int32", []bq.Arg{bq.I32(-2147483648), bq.U32(4294967295)}, "-2147483648|4294967295"},
		{"int64", []bq.Arg{bq.I64(-9223372036854775808), bq.U64(^uint64(0))}, "-9223372036854775808|18446744073709551615"},
		{"floats", []bq.Arg{bq.F32(1.25), bq.F64(-2.5), bq.Nil()}, "1.2500000|-2.500000000000000|null"},
		{"strings", []bq.Arg{bq.Str(""), bq.Str("中文😀"), bq.Str(strings.Repeat("x", 5000)), bq.Int(42)}, "|中文😀|" + strings.Repeat("x", 5000) + "|42"},
	}
	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			// The same values exercise both direct (1–4) and pooled (>4) paths.
			for _, padding := range []int{0, 5} {
				args := append([]bq.Arg(nil), tc.args...)
				expected := tc.want
				for i := 0; i < padding; i++ {
					args = append(args, bq.Str("pad"))
					expected += "|pad"
				}
				format := tc.name + ":" + strings.TrimSuffix(strings.Repeat("{}|", len(args)), "|")
				if !log.Info(format, args...) {
					t.Fatal("write failed")
				}
				if snapshot := log.Take_snapshot("gmt"); !strings.HasSuffix(snapshot, tc.name+":"+expected+"\n") {
					t.Fatalf("padding=%d want suffix=%q got=%q", padding, tc.name+":"+expected+"\n", snapshot)
				}
			}
		})
	}
}

//go:noinline
func callback_stack(depth int) byte {
	var space [1024]byte
	space[depth%len(space)] = byte(depth)
	if depth == 0 {
		return space[0]
	}
	return space[depth%len(space)] + callback_stack(depth-1)
}

func TestDirectWriteCallbackGC(t *testing.T) {
	bq.Set_console_buffer_enable(false)
	name := fmt.Sprintf("callback_gc_%d", next_log.Add(1))
	log := bq.Create_log(name, "appenders_config.Console.type=console\nappenders_config.Console.levels=[all]\nlog.thread_mode=sync\n", nil)
	if !log.Is_valid() {
		t.Fatal("Create_log failed")
	}
	var calls atomic.Int32
	var failed atomic.Bool
	bq.Register_console_callback(func(id uint64, _ int32, _ def.Log_level, content string) {
		if id == log.Get_id() {
			_ = callback_stack(100)
			if !strings.Contains(content, "dynamic-") {
				failed.Store(true)
			}
			if calls.Add(1)%10 == 0 {
				runtime.GC()
			}
		}
	})
	defer bq.Register_console_callback(nil)
	var wg sync.WaitGroup
	for worker := 0; worker < 4; worker++ {
		wg.Add(1)
		go func(worker int) {
			defer wg.Done()
			for index := 0; index < 20; index++ {
				str := fmt.Sprintf("dynamic-%d-%d", worker, index)
				if !log.Info("{} {} {}", bq.Str(str), bq.Int(index), bq.Str(strings.Clone(str))) {
					failed.Store(true)
				}
			}
		}(worker)
	}
	wg.Wait()
	if failed.Load() || calls.Load() != 80 {
		t.Fatalf("callback failed=%v, count=%d", failed.Load(), calls.Load())
	}
}

func BenchmarkConcurrentArguments(b *testing.B) {
	for _, size := range []int{16, 5000} {
		b.Run(fmt.Sprintf("payload_%d", size), func(b *testing.B) {
			log := new_log(b, "async", "", nil)
			payload := strings.Repeat("x", size)
			b.ReportAllocs()
			b.ResetTimer()
			b.RunParallel(func(pb *testing.PB) {
				for pb.Next() {
					log.Info("{} {}", bq.Int(42), bq.Str(payload))
				}
			})
			b.StopTimer()
			log.Force_flush()
		})
	}
}

func TestSnapshotConcurrency(t *testing.T) {
	log := new_log(t, "async", "snapshot.buffer_size=65536\nsnapshot.levels=[all]\n", nil)
	var wg sync.WaitGroup
	for i := 0; i < 4; i++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			for j := 0; j < 20; j++ {
				log.Info("snapshot {}", bq.Int(j))
				_ = log.Take_snapshot("gmt")
				runtime.Gosched()
			}
		}()
	}
	wg.Wait()
}
