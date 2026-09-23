// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.

package bq

import (
	"runtime"
	"strconv"
	"strings"
)

func capture_stack() string {
	var pcs [64]uintptr
	count := runtime.Callers(3, pcs[:])
	frames := runtime.CallersFrames(pcs[:count])
	var text strings.Builder
	for {
		frame, more := frames.Next()
		if !strings.HasPrefix(frame.Function, "github.com/Tencent/BqLog/wrapper/go/src/bq.") {
			text.WriteByte('\n')
			text.WriteString(frame.Function)
			text.WriteString("\n\t")
			text.WriteString(frame.File)
			text.WriteByte(':')
			text.WriteString(strconv.Itoa(frame.Line))
		}
		if !more {
			break
		}
	}
	return text.String()
}
