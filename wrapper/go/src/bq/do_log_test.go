// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
package bq

import (
	"testing"

	"github.com/Tencent/BqLog/wrapper/go/src/bq/def"
)

func Test_do_log_rejects_invalid_level(t *testing.T) {
	log := Create_log("invalid_level_private_test",
		"appenders_config.Console.type=console\nappenders_config.Console.levels=[all]\nlog.thread_mode=sync\n", nil)
	if !log.Is_valid() {
		t.Fatal("could not create log")
	}
	for _, level := range []def.Log_level{-1, 6, 32, 256} {
		if log.do_log(level, 0, "invalid") {
			t.Fatalf("accepted invalid level %d", level)
		}
	}
}
