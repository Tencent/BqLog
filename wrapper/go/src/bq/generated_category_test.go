// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
package bq_test

import (
	"reflect"
	"strings"
	"testing"

	categories "github.com/Tencent/BqLog/wrapper/go/src/bq/testdata/generated_category"
)

func Test_generated_category_outside_package(t *testing.T) {
	log := categories.Create_Generated_category("generated_category_test",
		"appenders_config.Console.type=console\nappenders_config.Console.levels=[all]\n"+
			"log.thread_mode=sync\nsnapshot.buffer_size=65536\nsnapshot.levels=[all]\n")
	if !log.Is_valid() {
		t.Fatal("generated logger is invalid")
	}
	// All six generated methods live in a different package from the wrapper.
	cat := log.Cat.ModuleA.SystemA.ClassA
	methods := []func(categories.Generated_category_Category_ref, string, ...any) bool{
		log.Verbose_c, log.Debug_c, log.Info_c, log.Warning_c, log.Error_c, log.Fatal_c,
	}
	for _, method := range methods {
		if !method(cat, "generated {}", 42) {
			t.Fatal("generated category method failed")
		}
	}
	if !log.Info_c(log.Cat.ModuleA.SystemA, "mid level") {
		t.Fatal("generated inner category method failed")
	}
	snapshot := log.Take_snapshot("gmt")
	if strings.Count(snapshot, "[ModuleA.SystemA.ClassA]") != 6 || strings.Count(snapshot, "generated 42") != 6 {
		t.Fatalf("unexpected generated output: %s", snapshot)
	}
	if !strings.Contains(snapshot, "[ModuleA.SystemA]\tmid level") {
		t.Fatalf("unexpected inner category output: %s", snapshot)
	}
	if !categories.Get_Generated_category_by_name("generated_category_test").Is_valid() {
		t.Fatal("generated lookup failed")
	}
	if log.Cat.A_B == log.Cat.A.B {
		t.Fatal("generated category names collide")
	}
	if log.Info_c(categories.Generated_category_Category(^uint32(0)), "invalid") {
		t.Fatal("invalid generated category was accepted")
	}
	for _, name := range []string{"Write", "Write_c", "Do_log", "do_log"} {
		if _, exists := reflect.TypeOf(log).MethodByName(name); exists {
			t.Fatalf("implementation method is publicly exposed: %s", name)
		}
	}
}
