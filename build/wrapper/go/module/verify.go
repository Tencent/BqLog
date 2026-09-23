// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
package main

import (
	"archive/zip"
	"encoding/json"
	"fmt"
	"io"
	"io/fs"
	"net/url"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
)

func escape_module(path string) string {
	var escaped strings.Builder
	for _, ch := range path {
		if ch >= 'A' && ch <= 'Z' {
			escaped.WriteByte('!')
			escaped.WriteRune(ch + 'a' - 'A')
		} else {
			escaped.WriteRune(ch)
		}
	}
	return escaped.String()
}

func run_go(dir string, env []string, args ...string) error {
	exe := os.Getenv("BQ_GO_EXECUTABLE")
	if exe == "" {
		exe = "go"
	}
	cmd := exec.Command(exe, args...)
	cmd.Dir = dir
	cmd.Env = env
	cmd.Stdout, cmd.Stderr = os.Stdout, os.Stderr
	if err := cmd.Run(); err != nil {
		return fmt.Errorf("go %s: %w", strings.Join(args, " "), err)
	}
	return nil
}

// Validate an actual versioned module download without relying on repository
// relative paths or replace directives. The file proxy is local and disposable.
func verify_module(module_dir string) error {
	module_dir, err := filepath.Abs(module_dir)
	if err != nil {
		return err
	}
	data, err := os.ReadFile(filepath.Join(module_dir, "SOURCE.json"))
	if err != nil {
		return err
	}
	var info manifest
	if err = json.Unmarshal(data, &info); err != nil {
		return err
	}
	if info.Module != module_path(info.Version) {
		return fmt.Errorf("module/version mismatch")
	}
	temp, err := os.MkdirTemp("", "bqlog-go-consumer-")
	if err != nil {
		return err
	}
	defer os.RemoveAll(temp)
	version := "v" + info.Version
	proxy := filepath.Join(temp, "proxy")
	prefix := filepath.ToSlash(filepath.Join(escape_module(info.Module), "@v"))
	gomod, err := read_text(filepath.Join(module_dir, "go.mod"))
	if err != nil {
		return err
	}
	for name, text := range map[string]string{
		"list":            version + "\n",
		version + ".mod":  gomod,
		version + ".info": fmt.Sprintf("{\"Version\":%q,\"Time\":\"2020-01-01T00:00:00Z\"}\n", version),
	} {
		if err = put(proxy, prefix+"/"+name, text); err != nil {
			return err
		}
	}
	archive, err := os.Create(filepath.Join(proxy, filepath.FromSlash(prefix), version+".zip"))
	if err != nil {
		return err
	}
	writer := zip.NewWriter(archive)
	err = filepath.WalkDir(module_dir, func(path string, entry fs.DirEntry, walk_err error) error {
		if walk_err != nil {
			return walk_err
		}
		if entry.IsDir() {
			return nil
		}
		ext := strings.ToLower(filepath.Ext(path))
		if ext == ".exe" || ext == ".dll" || ext == ".so" || ext == ".dylib" || ext == ".a" || ext == ".o" || ext == ".lib" {
			return fmt.Errorf("binary in source module: %s", path)
		}
		relative, _ := filepath.Rel(module_dir, path)
		out, err := writer.Create(info.Module + "@" + version + "/" + filepath.ToSlash(relative))
		if err != nil {
			return err
		}
		in, err := os.Open(path)
		if err != nil {
			return err
		}
		defer in.Close()
		_, err = io.Copy(out, in)
		return err
	})
	close_err := writer.Close()
	archive_err := archive.Close()
	if err != nil {
		return err
	}
	if close_err != nil {
		return close_err
	}
	if archive_err != nil {
		return archive_err
	}
	proxy_path := filepath.ToSlash(proxy)
	if !strings.HasPrefix(proxy_path, "/") {
		proxy_path = "/" + proxy_path
	}
	proxy_url := (&url.URL{Scheme: "file", Path: proxy_path}).String()
	var env []string
	for _, variable := range os.Environ() {
		name, _, _ := strings.Cut(variable, "=")
		switch strings.ToUpper(name) {
		case "GOPROXY", "GOSUMDB", "GOMODCACHE", "GOWORK", "GOFLAGS", "CGO_ENABLED", "CGO_CFLAGS", "CGO_CPPFLAGS", "CGO_CXXFLAGS", "CGO_LDFLAGS", "GONOPROXY", "GONOSUMDB", "GOPRIVATE":
			continue
		}
		env = append(env, variable)
	}
	env = append(env, "GOPROXY="+proxy_url, "GOSUMDB=off", "GOMODCACHE="+filepath.Join(temp, "modcache"),
		"GOWORK=off", "CGO_ENABLED=1", "GOFLAGS=", "GONOPROXY=", "GONOSUMDB=", "GOPRIVATE=")
	consumer := filepath.Join(temp, "consumer")
	if err = put(consumer, "go.mod", "module bqlog-consumer-check\n\ngo 1.21\n"); err != nil {
		return err
	}
	program := fmt.Sprintf(`package main
import (
    "fmt"
    "strings"
    bq %q
)
func main() {
    if bq.Get_version() != %q { panic("wrong native version") }
    log := bq.Create_log("consumer", "appenders_config.Console.type=console\nappenders_config.Console.levels=[all]\nlog.thread_mode=sync\nsnapshot.buffer_size=65536\nsnapshot.levels=[all]\n", nil)
    if !log.Is_valid() || !log.Info("consumer {}", 42) { panic("log failed") }
    if !strings.Contains(log.Take_snapshot("gmt"), "consumer 42") { panic("wrong output") }
    log.Force_flush()
    fmt.Println("Standalone versioned source module passed")
}
`, info.Module, info.Version)
	if err = put(consumer, "main.go", program); err != nil {
		return err
	}
	if err = run_go(consumer, env, "get", info.Module+"@"+version); err != nil {
		return err
	}
	if err = run_go(consumer, env, "test", "-count=1", "-timeout=120s", info.Module+"/..."); err != nil {
		return err
	}
	if err = run_go(consumer, env, "vet", info.Module+"/..."); err != nil {
		return err
	}
	return run_go(consumer, env, "run", ".")
}
