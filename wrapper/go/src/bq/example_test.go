// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
package bq_test

import (
	"errors"
	"fmt"
	"strings"

	bq "github.com/Tencent/BqLog/wrapper/go/src/bq"
)

type example_player struct{ name string }

func (p *example_player) String() string { return "player:" + p.name }

func ExampleLog_Info() {
	log := bq.Create_log("example_plain", `
appenders_config.Console.type=console
appenders_config.Console.levels=[all]
appenders_config.Console.enable=false
log.thread_mode=sync
snapshot.buffer_size=65536
snapshot.levels=[all]
`, nil)
	defer log.Force_flush()
	log.Info("ready")
	log.Info("player={} score={} error={} optional={}",
		&example_player{"Alice"}, 42, errors.New("offline"), nil)
	fmt.Println(strings.Contains(log.Take_snapshot("gmt"),
		"player=player:Alice score=42 error=offline optional=null"))
	// Output: true
}

// Package example: Go's example-name convention cannot represent underscores
// in both a type and method name unambiguously.
func Example_category_logging() {
	log := bq.Create_category_log("example_category", `
appenders_config.Console.type=console
appenders_config.Console.levels=[all]
appenders_config.Console.enable=false
log.thread_mode=sync
snapshot.buffer_size=65536
snapshot.levels=[all]
`, []string{"", "Gameplay"})
	defer log.Force_flush()
	log.Info("default category")
	log.Info_c(1, "score={}", 42)
	snapshot := log.Take_snapshot("gmt")
	fmt.Println(strings.Contains(snapshot, "[Gameplay]") && strings.Contains(snapshot, "score=42"))
	// Output: true
}
