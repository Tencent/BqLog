// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
package main

import (
	"flag"
	"fmt"
	"os"
	"runtime"
	"strconv"
	"sync"
	"sync/atomic"
	"time"

	bq "github.com/Tencent/BqLog/wrapper/go/src/bq"
	"github.com/Tencent/BqLog/wrapper/go/src/bq/impl"
)

const (
	logs_count          = 2000000
	character_pool_size = 1024 * 1024 * 8
	multi_format_count  = 2048
	multi_param_format  = "idx:{}, num:{}, This test, {}, {}"
	no_param_format     = "Empty Log, No Param"
)

type position struct{ start, size int }

var (
	logs          = map[string]*bq.Log{}
	ascii_charset string
	positions     []position
	templates     [multi_format_count]string
)

func template_pool_size() int {
	if size, err := strconv.Atoi(os.Getenv("BENCH_POOL_SIZE")); err == nil && size > 0 && size <= logs_count {
		return size
	}
	return 50000
}

func prepare_data() {
	data := make([]byte, character_pool_size)
	for i := range data {
		data[i] = byte(i%95 + 32)
	}
	ascii_charset = string(data)
	positions = make([]position, logs_count)
	for i := range positions {
		start := (i * 9973) % (character_pool_size - 1)
		size := i % 1024
		if remaining := character_pool_size - 1 - start; size > remaining {
			size = remaining
		}
		positions[i] = position{start, size}
	}
	for i := range templates {
		// C++ uses the corresponding literals in multi_format_templates.h.
		// Format construction is outside every timed region.
		templates[i] = fmt.Sprintf("bqbench multi-format tmpl %04d value={} extra={}", i)
	}
}

func write_multi_format(log *bq.Log, index, value int) bool {
	// C++ bench_log_multi_format takes long long v and an int literal.
	return log.Info(templates[index], int64(value), int32(index))
}

func flush_all() { impl.Force_flush(0) }

func run_case(name string, workers int, body func(int) uint64, warmup func()) {
	if warmup != nil {
		warmup()
	}
	fmt.Printf("CASE %s workers=%d entries_per_worker=%d\n", name, workers, logs_count)
	var wg sync.WaitGroup
	var failures atomic.Uint64
	// Like C++, timing includes worker creation, writes, joins, and force flush.
	start := time.Now()
	fmt.Println("Now Begin, each worker will write 2000000 log entries, please wait the result...")
	for worker := 0; worker < workers; worker++ {
		wg.Add(1)
		go func(worker int) {
			defer wg.Done()
			failures.Add(body(worker))
		}(worker)
	}
	wg.Wait()
	flush_all()
	elapsed := time.Since(start)
	fmt.Printf("Time Cost:%d\n", elapsed.Milliseconds())
	fmt.Printf("RESULT case=%s workers=%d entries=%d elapsed_ns=%d failed=%d\n",
		name, workers, uint64(workers)*logs_count, elapsed.Nanoseconds(), failures.Load())
	if failures.Load() != 0 {
		panic("benchmark writes failed")
	}
}

func multi_param(log *bq.Log) func(int) uint64 {
	return func(worker int) uint64 {
		var failed uint64
		for index := 0; index < logs_count; index++ {
			// Match C++ int32_t, int32_t, float, bool exactly.
			if !log.Info(multi_param_format, int32(worker), int32(index), float32(2.4232), true) {
				failed++
			}
		}
		return failed
	}
}

func no_param(log *bq.Log) func(int) uint64 {
	return func(_ int) uint64 {
		var failed uint64
		for index := 0; index < logs_count; index++ {
			if !log.Info(no_param_format) {
				failed++
			}
		}
		return failed
	}
}

func main() {
	workers := flag.Int("threads", 1, "concurrent Go writers (C++ uses OS threads)")
	selected := flag.String("case", "", "run only this case; empty runs all ten supported cases")
	flag.Parse()
	if *workers <= 0 {
		panic("threads must be positive")
	}
	for _, entry := range log_configs {
		log := bq.Create_log(entry.name, entry.config, nil)
		if !log.Is_valid() {
			panic("could not create " + entry.name)
		}
		logs[entry.name] = log
	}
	fmt.Printf("Go=%s GOMAXPROCS=%d BqLog=%s BENCH_POOL_SIZE=%d\n",
		runtime.Version(), runtime.GOMAXPROCS(0), bq.Get_version(), template_pool_size())
	flush_all()
	prepare_data()
	pool_size := template_pool_size()
	warm_templates := func() {
		for i := 0; i < multi_format_count; i++ {
			if !write_multi_format(logs["test_multi_format"], i, 0) {
				panic("warmup failed")
			}
		}
		flush_all()
	}
	cases := []struct {
		name   string
		body   func(int) uint64
		warmup func()
	}{
		{"ascii_utf8", func(_ int) uint64 {
			var failed uint64
			log := logs["test_ascii_u8"]
			for index := 0; index < logs_count; index++ {
				p := positions[index%pool_size]
				if !log.Info(ascii_charset[p.start : p.start+p.size]) {
					failed++
				}
			}
			return failed
		}, nil},
		{"compressed_4", multi_param(logs["compress"]), nil},
		{"encrypted_4", multi_param(logs["compress_enc"]), nil},
		{"text_4", multi_param(logs["text"]), nil},
		{"compressed_0", no_param(logs["compress"]), nil},
		{"encrypted_0", no_param(logs["compress_enc"]), nil},
		{"text_0", no_param(logs["text"]), nil},
		{"template_single", func(_ int) uint64 {
			var failed uint64
			for index := 0; index < logs_count; index++ {
				if !write_multi_format(logs["test_multi_format"], 0, index) {
					failed++
				}
			}
			return failed
		}, nil},
		{"template_roundrobin", func(_ int) uint64 {
			var failed uint64
			for index := 0; index < logs_count; index++ {
				if !write_multi_format(logs["test_multi_format"], index%multi_format_count, index) {
					failed++
				}
			}
			return failed
		}, warm_templates},
		{"template_hotwindow", func(_ int) uint64 {
			const window, burst = 8, 64
			var failed uint64
			base, index := 0, 0
			for index < logs_count {
				for b := 0; b < burst && index < logs_count; b++ {
					if !write_multi_format(logs["test_multi_format"], base+index%window, index) {
						failed++
					}
					index++
				}
				base += window
				if base > multi_format_count-window {
					base = 0
				}
			}
			return failed
		}, warm_templates},
	}
	found := false
	for _, tc := range cases {
		if *selected == "" || *selected == tc.name {
			found = true
			run_case(tc.name, *workers, tc.body, tc.warmup)
		}
	}
	if !found {
		panic("unknown case: " + *selected)
	}
	fmt.Println("SKIP: ASCII/Chinese/mixed UTF16 inputs have no Go public API equivalent.")
}
