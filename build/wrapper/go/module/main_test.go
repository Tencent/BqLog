// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
package main

import (
	"os"
	"path/filepath"
	"strings"
	"testing"
)

func Test_prepare_source_module(t *testing.T) {
	repo, err := filepath.Abs("../../../..")
	if err != nil {
		t.Fatal(err)
	}
	source, err := read_text(filepath.Join(repo, "src/bq_log/global/version.cpp"))
	if err != nil {
		t.Fatal(err)
	}
	version := version_pattern.FindStringSubmatch(source)[1]
	out := filepath.Join(t.TempDir(), "go")
	info, err := prepare(repo, out, version, strings.Repeat("a", 40))
	if err != nil {
		t.Fatal(err)
	}
	if info.Module != module_path(version) || info.Version != version {
		t.Fatalf("wrong module identity: %#v", info)
	}
	invoker, _ := read_text(filepath.Join(out, "impl/invoker.go"))
	if strings.Contains(invoker, "-lBqLog") || strings.Contains(invoker, "../../") {
		t.Fatal("module still depends on the repository or a prebuilt library")
	}
	// Do not rewrite libc headers to similarly named BqLog headers.
	header, _ := read_text(filepath.Join(out, "impl", source_name("include/bq_common/types/type_tools.h")))
	if !strings.Contains(header, "#include <string.h>") {
		t.Fatal("system string.h was relocated")
	}
	header, _ = read_text(filepath.Join(out, "impl", source_name("include/bq_common/misc/assert.h")))
	if !strings.Contains(header, "#include <assert.h>") {
		t.Fatal("system assert.h was relocated")
	}
	generated, err := read_text(filepath.Join(out, "testdata/generated_category/generated_category.go"))
	if err != nil || !strings.Contains(generated, info.Module) || strings.Contains(generated, ".Write(") {
		t.Fatal("generated category fixture is missing or uses an inaccessible API")
	}
	if _, err := os.Stat(filepath.Join(out, "impl/native_src_bq_log_api_bq_log_go.cpp")); err != nil {
		t.Fatal("Go native API source is missing")
	}
	if _, err := prepare(repo, out, version, "other"); err == nil {
		t.Fatal("existing output must not be overwritten")
	}
}

func Test_module_major_version(t *testing.T) {
	for _, tc := range []struct{ version, want string }{
		{"1.2.3", "github.com/Tencent/BqLog/go"},
		{"2.0.0", "github.com/Tencent/BqLog/go/v2"},
		{"3.0.0-rc.1", "github.com/Tencent/BqLog/go/v3"},
	} {
		if got := module_path(tc.version); got != tc.want {
			t.Errorf("%s: got %s want %s", tc.version, got, tc.want)
		}
	}
}
