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
	logsCount         = 2000000
	characterPoolSize = 1024 * 1024 * 8
	multiFormatCount  = 2048
	multiParamFormat  = "idx:{}, num:{}, This test, {}, {}"
	noParamFormat     = "Empty Log, No Param"
)

type position struct{ start, size int }

var (
	logs         = map[string]*bq.Log{}
	asciiCharset string
	positions    []position
	templates    [multiFormatCount]string
)

func templatePoolSize() int {
	if size, err := strconv.Atoi(os.Getenv("BENCH_POOL_SIZE")); err == nil && size > 0 && size <= logsCount {
		return size
	}
	return 50000
}

func prepareData() {
	data := make([]byte, characterPoolSize)
	for i := range data {
		data[i] = byte(i%95 + 32)
	}
	asciiCharset = string(data)
	positions = make([]position, logsCount)
	for i := range positions {
		start := (i * 9973) % (characterPoolSize - 1)
		size := i % 1024
		if remaining := characterPoolSize - 1 - start; size > remaining {
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

func writeMultiFormat(log *bq.Log, index, value int) bool {
	// C++ bench_log_multi_format takes long long v and an int literal.
	return log.Info(templates[index], bq.I64(int64(value)), bq.I32(int32(index)))
}

func flushAll() { impl.Force_flush(0) }

func runCase(name string, workers int, body func(int) uint64, warmup func()) {
	if warmup != nil {
		warmup()
	}
	fmt.Printf("CASE %s workers=%d entries_per_worker=%d\n", name, workers, logsCount)
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
	flushAll()
	elapsed := time.Since(start)
	fmt.Printf("Time Cost:%d\n", elapsed.Milliseconds())
	fmt.Printf("RESULT case=%s workers=%d entries=%d elapsed_ns=%d failed=%d\n",
		name, workers, uint64(workers)*logsCount, elapsed.Nanoseconds(), failures.Load())
	if failures.Load() != 0 {
		panic("benchmark writes failed")
	}
}

func multiParam(log *bq.Log) func(int) uint64 {
	return func(worker int) uint64 {
		var failed uint64
		for index := 0; index < logsCount; index++ {
			// Match C++ int32_t, int32_t, float, bool exactly.
			if !log.Info(multiParamFormat, bq.I32(int32(worker)), bq.I32(int32(index)), bq.F32(2.4232), bq.Bool(true)) {
				failed++
			}
		}
		return failed
	}
}

func noParam(log *bq.Log) func(int) uint64 {
	return func(_ int) uint64 {
		var failed uint64
		for index := 0; index < logsCount; index++ {
			if !log.Info(noParamFormat) {
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
	for _, entry := range logConfigs {
		log := bq.Create_log(entry.name, entry.config, nil)
		if !log.Is_valid() {
			panic("could not create " + entry.name)
		}
		logs[entry.name] = log
	}
	fmt.Printf("Go=%s GOMAXPROCS=%d BqLog=%s BENCH_POOL_SIZE=%d\n",
		runtime.Version(), runtime.GOMAXPROCS(0), bq.Get_version(), templatePoolSize())
	flushAll()
	prepareData()
	poolSize := templatePoolSize()
	warmTemplates := func() {
		for i := 0; i < multiFormatCount; i++ {
			if !writeMultiFormat(logs["test_multi_format"], i, 0) {
				panic("warmup failed")
			}
		}
		flushAll()
	}
	cases := []struct {
		name   string
		body   func(int) uint64
		warmup func()
	}{
		{"ascii_utf8", func(_ int) uint64 {
			var failed uint64
			log := logs["test_ascii_u8"]
			for index := 0; index < logsCount; index++ {
				p := positions[index%poolSize]
				if !log.Info(asciiCharset[p.start : p.start+p.size]) {
					failed++
				}
			}
			return failed
		}, nil},
		{"compressed_4", multiParam(logs["compress"]), nil},
		{"encrypted_4", multiParam(logs["compress_enc"]), nil},
		{"text_4", multiParam(logs["text"]), nil},
		{"compressed_0", noParam(logs["compress"]), nil},
		{"encrypted_0", noParam(logs["compress_enc"]), nil},
		{"text_0", noParam(logs["text"]), nil},
		{"template_single", func(_ int) uint64 {
			var failed uint64
			for index := 0; index < logsCount; index++ {
				if !writeMultiFormat(logs["test_multi_format"], 0, index) {
					failed++
				}
			}
			return failed
		}, nil},
		{"template_roundrobin", func(_ int) uint64 {
			var failed uint64
			for index := 0; index < logsCount; index++ {
				if !writeMultiFormat(logs["test_multi_format"], index%multiFormatCount, index) {
					failed++
				}
			}
			return failed
		}, warmTemplates},
		{"template_hotwindow", func(_ int) uint64 {
			const window, burst = 8, 64
			var failed uint64
			base, index := 0, 0
			for index < logsCount {
				for b := 0; b < burst && index < logsCount; b++ {
					if !writeMultiFormat(logs["test_multi_format"], base+index%window, index) {
						failed++
					}
					index++
				}
				base += window
				if base > multiFormatCount-window {
					base = 0
				}
			}
			return failed
		}, warmTemplates},
	}
	found := false
	for _, tc := range cases {
		if *selected == "" || *selected == tc.name {
			found = true
			runCase(tc.name, *workers, tc.body, tc.warmup)
		}
	}
	if !found {
		panic("unknown case: " + *selected)
	}
	fmt.Println("SKIP: ASCII/Chinese/mixed UTF16 inputs have no Go public API equivalent.")
}
