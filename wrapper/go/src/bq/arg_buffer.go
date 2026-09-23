// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
package bq

import "sync"

// A call owns its buffer until the fused native copy completes. Pools do not
// hold a lock across serialization, native backpressure, or console callbacks.
// Bound retained buffers to 1 MiB; exceptional larger entries are not pooled.
type args_buffer struct {
	data []byte
	pool *sync.Pool
}

var args_pools = func() [5]*sync.Pool {
	var pools [5]*sync.Pool
	for i, size := range [...]int{256, 4096, 16384, 65536, 1048576} {
		capacity := size
		pools[i] = &sync.Pool{New: func() any { return &args_buffer{data: make([]byte, capacity)} }}
	}
	return pools
}()

func acquire_args_buffer(size int) *args_buffer {
	for i, capacity := range [...]int{256, 4096, 16384, 65536, 1048576} {
		if size <= capacity {
			pool := args_pools[i]
			buffer := pool.Get().(*args_buffer)
			buffer.pool = pool
			return buffer
		}
	}
	return &args_buffer{data: make([]byte, size)}
}

func release_args_buffer(buffer *args_buffer) {
	if buffer.pool != nil {
		buffer.pool.Put(buffer)
	}
}
