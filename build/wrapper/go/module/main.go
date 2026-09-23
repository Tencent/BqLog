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

const developmentImport = "github.com/Tencent/BqLog/wrapper/go/src/bq"

var (
	versionPattern = regexp.MustCompile(`BQ_LOG_VERSION\s*=\s*"([0-9]+\.[0-9]+\.[0-9]+(?:-[0-9A-Za-z.-]+)?)"`)
	includePattern = regexp.MustCompile(`(?m)^(\s*#\s*include\s*)[<"]([^>"\r\n]+)[>"]`)
)

type sourceFile struct {
	Path   string `json:"path"`
	SHA256 string `json:"sha256"`
}

type manifest struct {
	Version string       `json:"version"`
	Module  string       `json:"module"`
	Commit  string       `json:"source_commit"`
	Sources []sourceFile `json:"sources"`
}

func readText(path string) (string, error) {
	data, err := os.ReadFile(path)
	return strings.ReplaceAll(strings.TrimPrefix(string(data), "\ufeff"), "\r\n", "\n"), err
}

func modulePath(version string) string {
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

func sourceName(path string) string {
	name := "native_" + strings.ReplaceAll(path, "/", "_")
	if strings.HasSuffix(name, ".inc") {
		// cgo tracks headers in the package directory, including this entry list.
		name = strings.TrimSuffix(name, ".inc") + "_inc.h"
	}
	return name
}

func prepare(repo, output, expectedVersion, commit string) (manifest, error) {
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
	versionSource, err := readText(filepath.Join(repo, "src/bq_log/global/version.cpp"))
	if err != nil {
		return result, err
	}
	match := versionPattern.FindStringSubmatch(versionSource)
	if len(match) != 2 || (expectedVersion != "" && match[1] != expectedVersion) {
		return result, fmt.Errorf("BqLog source version does not match requested release %q", expectedVersion)
	}
	version := match[1]
	result = manifest{Version: version, Module: modulePath(version), Commit: commit}

	// Only source/header files enter the release. Every translation unit remains
	// separate; the development tree does not acquire forwarding .cc files.
	native := make(map[string]string)
	for _, tree := range []string{"include", "src"} {
		err = filepath.WalkDir(filepath.Join(repo, tree), func(path string, entry fs.DirEntry, walkErr error) error {
			if walkErr != nil {
				return walkErr
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
			native[relative] = sourceName(relative)
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
		content, err := readText(filepath.Join(repo, filepath.FromSlash(path)))
		if err != nil {
			return result, err
		}
		hash := sha256.Sum256([]byte(content))
		result.Sources = append(result.Sources, sourceFile{path, hex.EncodeToString(hash[:])})
		var unresolved string
		content = includePattern.ReplaceAllStringFunc(content, func(line string) string {
			parts := includePattern.FindStringSubmatch(line)
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
	err = filepath.WalkDir(wrapper, func(path string, entry fs.DirEntry, walkErr error) error {
		if walkErr != nil {
			return walkErr
		}
		if entry.IsDir() || (filepath.Ext(path) != ".go" && filepath.Ext(path) != ".txt") {
			return nil
		}
		content, err := readText(path)
		if err != nil {
			return err
		}
		relative, _ := filepath.Rel(wrapper, path)
		relative = filepath.ToSlash(relative)
		content = strings.ReplaceAll(content, developmentImport, result.Module)
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
	license, err := readText(filepath.Join(repo, "LICENSE.txt"))
	if err != nil {
		return result, err
	}
	if err = put(output, "LICENSE", license); err != nil {
		return result, err
	}
	readme := fmt.Sprintf("# BqLog Go %s\n\nGenerated from Tencent/BqLog commit %s. Do not edit generated sources.\n\n"+
		"Install: `go get %s@v%s`\n\n"+
		"Import: `import bq %q`\n\n"+
		"Requires Go 1.21+, CGO_ENABLED=1, and a C/C++17 compiler. "+
		"Native source compiles automatically; no prebuilt BqLog DLL/so or CMake step is needed. "+
		"The native objects link into the Go program. Desktop Windows, Linux, macOS and FreeBSD are supported.\n\n"+
		"Create a logger with `bq.Create_log(name, config, nil)`, call `log.Info(format, bq.Str(value))`, "+
		"and flush before exit with `log.Force_flush()`.\n", version, commit, result.Module, version, result.Module)
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
		if err := verifyModule(*verify); err != nil {
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
