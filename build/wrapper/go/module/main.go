// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
// Prepare the standalone source module; publishing is owned by release.yml.
package main

import (
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"flag"
	"fmt"
	"io/fs"
	"os"
	"os/exec"
	"path/filepath"
	"regexp"
	"sort"
	"strings"
)

const development_import = "github.com/Tencent/BqLog/wrapper/go/src/bq"

var (
	version_pattern = regexp.MustCompile(`BQ_LOG_VERSION\s*=\s*"([0-9]+\.[0-9]+\.[0-9]+(?:-[0-9A-Za-z.-]+)?)"`)
	include_pattern = regexp.MustCompile(`(?m)^(\s*#\s*include\s*)[<"]([^>"\r\n]+)[>"]`)
)

type source_file struct {
	Path   string `json:"path"`
	SHA256 string `json:"sha256"`
}

type manifest struct {
	Version string        `json:"version"`
	Module  string        `json:"module"`
	Commit  string        `json:"source_commit"`
	Sources []source_file `json:"sources"`
}

func read_text(path string) (string, error) {
	data, err := os.ReadFile(path)
	return strings.ReplaceAll(strings.TrimPrefix(string(data), "\ufeff"), "\r\n", "\n"), err
}

func module_path(version string) string {
	major := strings.Split(version, ".")[0]
	if major == "0" || major == "1" {
		return "github.com/Tencent/BqLog/go"
	}
	return "github.com/Tencent/BqLog/go/v" + major
}

func put(root, relative, content string) error {
	path := filepath.Join(root, filepath.FromSlash(relative))
	if err := os.MkdirAll(filepath.Dir(path), 0755); err != nil {
		return err
	}
	return os.WriteFile(path, []byte(content), 0644)
}

func source_name(path string) string {
	name := "native_" + strings.ReplaceAll(path, "/", "_")
	if strings.HasSuffix(name, ".inc") {
		// cgo tracks headers in the package directory, including this entry list.
		name = strings.TrimSuffix(name, ".inc") + "_inc.h"
	}
	return name
}

// Generate the distribution README from the Go section of the integration guide.
func module_readme(guide, module, version, commit string) (string, error) {
	const start_marker = "<!-- go-module:start -->"
	const end_marker = "<!-- go-module:end -->"
	start, end := strings.Index(guide, start_marker), strings.Index(guide, end_marker)
	if start < 0 || end <= start || strings.Count(guide, start_marker) != 1 || strings.Count(guide, end_marker) != 1 {
		return "", fmt.Errorf("integration guide must mark one Go section")
	}
	chapter := strings.TrimSpace(guide[start+len(start_marker) : end])
	chapter = strings.ReplaceAll(chapter, "github.com/Tencent/BqLog/go/v2", module)
	return fmt.Sprintf("# BqLog for Go %s\n\n<!-- Source commit: %s -->\n\n%s\n",
		version, commit, chapter), nil
}

func prepare(repo, output, expected_version, commit string) (manifest, error) {
	var result manifest
	repo, err := filepath.Abs(repo)
	if err != nil {
		return result, err
	}
	output, err = filepath.Abs(output)
	if err != nil {
		return result, err
	}
	if _, err := os.Stat(output); !os.IsNotExist(err) {
		return result, fmt.Errorf("output must be a new directory: %s", output)
	}
	version_source, err := read_text(filepath.Join(repo, "src/bq_log/global/version.cpp"))
	if err != nil {
		return result, err
	}
	match := version_pattern.FindStringSubmatch(version_source)
	if len(match) != 2 || (expected_version != "" && match[1] != expected_version) {
		return result, fmt.Errorf("BqLog source version does not match requested release %q", expected_version)
	}
	version := match[1]
	result = manifest{Version: version, Module: module_path(version), Commit: commit}

	// Only source/header files enter the release. Every translation unit remains
	// separate; the development tree does not acquire forwarding .cc files.
	native := make(map[string]string)
	for _, tree := range []string{"include", "src"} {
		err = filepath.WalkDir(filepath.Join(repo, tree), func(path string, entry fs.DirEntry, walk_err error) error {
			if walk_err != nil {
				return walk_err
			}
			if entry.Type()&os.ModeSymlink != 0 {
				return fmt.Errorf("unexpected source symlink: %s", path)
			}
			if entry.IsDir() {
				return nil
			}
			ext := filepath.Ext(path)
			if ext != ".h" && ext != ".hpp" && ext != ".inc" && ext != ".cpp" {
				return nil
			}
			relative, _ := filepath.Rel(repo, path)
			relative = filepath.ToSlash(relative)
			native[relative] = source_name(relative)
			return nil
		})
		if err != nil {
			return result, err
		}
	}
	// The macOS implementation contains ordinary C++, despite its .mm suffix.
	native["src/bq_common/platform/mac_misc.mm"] = "native_mac_darwin.cpp"
	names := make(map[string]string)
	var paths []string
	for path, name := range native {
		if previous, exists := names[strings.ToLower(name)]; exists {
			return result, fmt.Errorf("source-name collision: %s and %s", previous, path)
		}
		names[strings.ToLower(name)] = path
		paths = append(paths, path)
	}
	sort.Strings(paths)
	for _, path := range paths {
		content, err := read_text(filepath.Join(repo, filepath.FromSlash(path)))
		if err != nil {
			return result, err
		}
		hash := sha256.Sum256([]byte(content))
		result.Sources = append(result.Sources, source_file{path, hex.EncodeToString(hash[:])})
		var unresolved string
		content = include_pattern.ReplaceAllStringFunc(content, func(line string) string {
			parts := include_pattern.FindStringSubmatch(line)
			target := parts[2]
			// <string.h> and <assert.h> are system headers even when a local
			// header has the same basename. Only BqLog angle includes relocate.
			if strings.Contains(line, "<") && !strings.HasPrefix(target, "bq_common/") && !strings.HasPrefix(target, "bq_log/") {
				return line
			}
			for _, candidate := range []string{
				filepath.ToSlash(filepath.Clean(filepath.Join(filepath.Dir(path), target))),
				"include/" + target, "src/" + target,
			} {
				candidate = filepath.ToSlash(filepath.Clean(candidate))
				if mapped, exists := native[candidate]; exists {
					return parts[1] + "\"" + mapped + "\""
				}
			}
			if strings.HasPrefix(target, "bq_common/") || strings.HasPrefix(target, "bq_log/") {
				unresolved = target
			}
			return line
		})
		if unresolved != "" {
			return result, fmt.Errorf("%s has unresolved native include %s", path, unresolved)
		}
		if err = put(output, "impl/"+native[path], content); err != nil {
			return result, err
		}
	}

	wrapper := filepath.Join(repo, "wrapper/go/src/bq")
	err = filepath.WalkDir(wrapper, func(path string, entry fs.DirEntry, walk_err error) error {
		if walk_err != nil {
			return walk_err
		}
		if entry.IsDir() || (filepath.Ext(path) != ".go" && filepath.Ext(path) != ".txt") {
			return nil
		}
		content, err := read_text(path)
		if err != nil {
			return err
		}
		relative, _ := filepath.Rel(wrapper, path)
		relative = filepath.ToSlash(relative)
		content = strings.ReplaceAll(content, development_import, result.Module)
		if relative == "impl/invoker.go" {
			original := "#cgo CFLAGS: -I${SRCDIR}/../../../../../include -DBQ_GO"
			if strings.Count(content, original) != 1 {
				return fmt.Errorf("invoker cgo flags changed: update source packaging")
			}
			content = strings.Replace(content, original, "#cgo CFLAGS: -DBQ_GO", 1)
			content = strings.ReplaceAll(content, "#cgo LDFLAGS: -lBqLog\n", "")
			content = strings.ReplaceAll(content, "#cgo windows LDFLAGS: -static-libgcc\n", "")
			content = strings.ReplaceAll(content, "<bq_log/misc/bq_log_c_api.h>", "\""+native["include/bq_log/misc/bq_log_c_api.h"]+"\"")
		}
		return put(output, relative, content)
	})
	if err != nil {
		return result, err
	}
	flags := `// Code generated for the BqLog source distribution.
package impl

/*
#cgo CXXFLAGS: -std=c++17 -O2 -DNDEBUG -DBQ_DYNAMIC_LIB -DBQ_GO -fno-exceptions -fno-rtti
#cgo windows LDFLAGS: -ldbghelp -static-libgcc -static-libstdc++
#cgo linux LDFLAGS: -ldl -lpthread
#cgo freebsd LDFLAGS: -lexecinfo -lpthread
#include "native_build_check.h"
*/
import "C"
`
	check := `// Generated distribution supports the desktop targets tested by release CI.
#if !defined(_WIN32) && !defined(__linux__) && !defined(__APPLE__) && !defined(__FreeBSD__)
#error "This BqLog Go source distribution does not support this target"
#endif
#if defined(__ANDROID__)
#error "Use the platform BqLog build for Android"
#endif
`
	if err = put(output, "impl/source_build.go", flags); err != nil {
		return result, err
	}
	if err = put(output, "impl/native_build_check.h", check); err != nil {
		return result, err
	}
	if err = put(output, "impl/native_build_check.cpp", `#if !defined(BQ_DYNAMIC_LIB) || !defined(BQ_GO)
#error "BqLog source builds require BQ_DYNAMIC_LIB and BQ_GO"
#endif
`); err != nil {
		return result, err
	}
	if err = put(output, "go.mod", "module "+result.Module+"\n\ngo 1.21\n"); err != nil {
		return result, err
	}
	license, err := read_text(filepath.Join(repo, "LICENSE.txt"))
	if err != nil {
		return result, err
	}
	if err = put(output, "LICENSE", license); err != nil {
		return result, err
	}
	guide, err := read_text(filepath.Join(repo, "docs/INTEGRATION_GUIDE.md"))
	if err != nil {
		return result, err
	}
	readme, err := module_readme(guide, result.Module, version, commit)
	if err != nil {
		return result, err
	}
	if err = put(output, "README.md", readme); err != nil {
		return result, err
	}
	data, _ := json.MarshalIndent(result, "", "  ")
	if err = put(output, "SOURCE.json", string(data)+"\n"); err != nil {
		return result, err
	}
	return result, nil
}

func main() {
	repo := flag.String("repo", ".", "BqLog source repository")
	output := flag.String("out", "", "new output module directory")
	version := flag.String("version", "", "expected BqLog release version")
	commit := flag.String("commit", "", "source commit recorded in manifest")
	verify := flag.String("verify", "", "verify an existing generated module through a local Go proxy")
	flag.Parse()
	if *verify != "" {
		if err := verify_module(*verify); err != nil {
			fmt.Fprintln(os.Stderr, err)
			os.Exit(1)
		}
		return
	}
	if *output == "" {
		fmt.Fprintln(os.Stderr, "-out is required")
		os.Exit(1)
	}
	if *commit == "" {
		bytes, err := exec.Command("git", "-C", *repo, "rev-parse", "HEAD").Output()
		if err != nil {
			fmt.Fprintln(os.Stderr, err)
			os.Exit(1)
		}
		*commit = strings.TrimSpace(string(bytes))
	}
	result, err := prepare(*repo, *output, *version, *commit)
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
	fmt.Printf("Prepared %s@v%s in %s\n", result.Module, result.Version, *output)
}
