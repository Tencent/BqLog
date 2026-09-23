// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
package main

import (
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"strings"
	"testing"
)

// Exercises the real release script against a disposable local bare remote.
// No network or project Git refs are modified.
func TestPublishLocalRemote(t *testing.T) {
	bash := os.Getenv("BQ_BASH")
	if bash == "" {
		if runtime.GOOS == "windows" {
			t.Skip("set BQ_BASH to test the release shell script on Windows")
		}
		bash = "bash"
	}
	script, err := filepath.Abs("../publish.sh")
	if err != nil {
		t.Fatal(err)
	}
	temp := t.TempDir()
	remote := filepath.Join(temp, "remote.git")
	checkout := filepath.Join(temp, "checkout")
	command := func(dir, name string, args ...string) string {
		t.Helper()
		cmd := exec.Command(name, args...)
		cmd.Dir = dir
		cmd.Env = append(os.Environ(), "GIT_CONFIG_NOSYSTEM=1", "GIT_CONFIG_GLOBAL="+os.DevNull)
		out, err := cmd.CombinedOutput()
		if err != nil {
			t.Fatalf("%s %v: %v\n%s", name, args, err, out)
		}
		return strings.TrimSpace(string(out))
	}
	command(temp, "git", "init", "--bare", remote)
	command(temp, "git", "init", "-b", "main", checkout)
	command(checkout, "git", "config", "user.name", "release-test")
	command(checkout, "git", "config", "user.email", "release-test@example.invalid")
	if err := os.WriteFile(filepath.Join(checkout, "untouched.txt"), []byte("development tree\n"), 0644); err != nil {
		t.Fatal(err)
	}
	command(checkout, "git", "add", ".")
	command(checkout, "git", "commit", "-m", "source")
	source := command(checkout, "git", "rev-parse", "HEAD")
	command(checkout, "git", "remote", "add", "origin", remote)
	command(checkout, "git", "push", "-u", "origin", "main")
	writeModule := func(version string) string {
		t.Helper()
		dir := filepath.Join(temp, "module-"+version)
		if err := put(dir, "go.mod", "module "+modulePath(version)+"\n\ngo 1.21\n"); err != nil {
			t.Fatal(err)
		}
		if err := put(dir, "SOURCE.json", fmt.Sprintf("{\n  \"version\": %q,\n  \"source_commit\": %q\n}\n", version, source)); err != nil {
			t.Fatal(err)
		}
		return dir
	}
	publish := func(dir, version string, allowed bool) error {
		t.Helper()
		cmd := exec.Command(bash, filepath.ToSlash(script), filepath.ToSlash(checkout), filepath.ToSlash(dir), version, source)
		cmd.Dir = temp
		for _, entry := range os.Environ() {
			name, _, _ := strings.Cut(entry, "=")
			if name != "GITHUB_ACTIONS" && name != "GITHUB_WORKFLOW" && name != "GITHUB_EVENT_NAME" {
				cmd.Env = append(cmd.Env, entry)
			}
		}
		cmd.Env = append(cmd.Env, "GIT_CONFIG_NOSYSTEM=1", "GIT_CONFIG_GLOBAL="+os.DevNull)
		if allowed {
			cmd.Env = append(cmd.Env, "GITHUB_ACTIONS=true", "GITHUB_WORKFLOW=Build And Release", "GITHUB_EVENT_NAME=workflow_dispatch")
		}
		out, err := cmd.CombinedOutput()
		if err != nil {
			return fmt.Errorf("%w\n%s", err, out)
		}
		return nil
	}
	first := writeModule("2.4.1")
	if err := publish(first, "2.4.1", false); err == nil {
		t.Fatal("non-release invocation was allowed")
	}
	if err := publish(first, "2.4.1", true); err != nil {
		t.Fatal(err)
	}
	firstCommit := command(temp, "git", "--git-dir="+remote, "rev-parse", "refs/tags/go/v2.4.1")
	if branch := command(temp, "git", "--git-dir="+remote, "rev-parse", "refs/heads/go_dist"); branch != firstCommit {
		t.Fatal("branch and tag are not atomic peers")
	}
	if err := publish(first, "2.4.1", true); err != nil {
		t.Fatalf("idempotent retry: %v", err)
	}
	if err := put(first, "changed.txt", "must not overwrite a release"); err != nil {
		t.Fatal(err)
	}
	if err := publish(first, "2.4.1", true); err == nil {
		t.Fatal("different source overwrote immutable tag")
	}
	second := writeModule("2.4.2")
	if err := publish(second, "2.4.2", true); err != nil {
		t.Fatal(err)
	}
	if parent := command(temp, "git", "--git-dir="+remote, "rev-parse", "refs/heads/go_dist^"); parent != firstCommit {
		t.Fatal("generated branch history was not preserved")
	}
	if err := publish(writeModule("2.4.0"), "2.4.0", true); err == nil {
		t.Fatal("older release rewound distribution branch")
	}
	if command(checkout, "git", "rev-parse", "HEAD") != source || command(checkout, "git", "status", "--porcelain") != "" {
		t.Fatal("publishing modified the development checkout")
	}
}
