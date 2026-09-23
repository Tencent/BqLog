// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
package main

import (
	"os"
	"regexp"
	"strconv"
	"strings"
	"testing"
)

func TestCppParity(t *testing.T) {
	source, err := os.ReadFile("../cpp/main.cpp")
	if err != nil {
		t.Fatal(err)
	}
	configRE := regexp.MustCompile("(?s)static bq::log \\w+ = bq::log::create_log\\(\"([^\"]+)\", R\"\\((.*?)\\)\"\\);")
	configs := make(map[string]string)
	for _, m := range configRE.FindAllStringSubmatch(string(source), -1) {
		configs[m[1]] = m[2]
	}
	normalize := func(s string) string {
		var lines []string
		for _, line := range strings.Split(s, "\n") {
			if line = strings.TrimSpace(line); line != "" {
				lines = append(lines, line)
			}
		}
		return strings.Join(lines, "\n")
	}
	if len(configs) != len(logConfigs) {
		t.Fatalf("C++ has %d logs; Go has %d", len(configs), len(logConfigs))
	}
	for _, entry := range logConfigs {
		if normalize(entry.config) != normalize(configs[entry.name]) {
			t.Errorf("config differs: %s", entry.name)
		}
	}
	for _, expected := range []string{multiParamFormat, noParamFormat, "logs_count = 2000000", "hot_window_size = 8", "burst_size = 64", "return 50000;"} {
		if !strings.Contains(string(source), expected) {
			t.Errorf("C++ workload changed: %s", expected)
		}
	}
	prepareData()
	header, err := os.ReadFile("../cpp/multi_format_templates.h")
	if err != nil {
		t.Fatal(err)
	}
	templateRE := regexp.MustCompile("case (\\d+): log_obj.info\\(\"([^\"]+)\", v, (\\d+)\\)")
	matches := templateRE.FindAllStringSubmatch(string(header), -1)
	if len(matches) != multiFormatCount {
		t.Fatalf("C++ template count: %d", len(matches))
	}
	for _, m := range matches {
		index, _ := strconv.Atoi(m[1])
		if templates[index] != m[2] || m[1] != m[3] {
			t.Errorf("template differs: %d", index)
		}
	}
}
