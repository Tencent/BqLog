#!/usr/bin/env python3
# Copyright (C) 2026 Tencent.
# BQLOG is licensed under the Apache License, Version 2.0.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.

import pathlib
import shutil
import subprocess
import sys
import tempfile
import unittest

repo_root = pathlib.Path(__file__).resolve().parents[2]
bridge_symbols = ["__api_go_log_write", "__api_go_log_write_1", "__api_go_log_write_2", "__api_go_log_write_4"]


class package_checks(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp_ = tempfile.TemporaryDirectory()
        cls.root_ = pathlib.Path(cls.temp_.name)
        cls.bash_ = shutil.which("bash")
        if sys.platform == "win32":
            cls.bash_ = str(pathlib.Path(shutil.which("git")).resolve().parents[1] / "bin/bash.exe")
        export = "__declspec(dllexport)" if sys.platform == "win32" else '__attribute__((visibility("default")))'
        prefix = "#define BQ_TEST_EXPORT %s\n" % export
        (cls.root_ / "good.c").write_text(prefix + "\n".join("BQ_TEST_EXPORT void %s(void) {}" % s for s in bridge_symbols), encoding="utf-8")
        (cls.root_ / "bad.c").write_text(prefix + 'BQ_TEST_EXPORT const char* names(void) { return "' + " ".join(bridge_symbols) + '"; }\n', encoding="utf-8")
        (cls.root_ / "CMakeLists.txt").write_text("cmake_minimum_required(VERSION 3.22)\nproject(package_fixture C)\nadd_library(good SHARED good.c)\nadd_library(bad SHARED bad.c)\n", encoding="utf-8")
        configure = ["cmake", "-S", str(cls.root_), "-B", str(cls.root_ / "build")]
        if sys.platform == "darwin":
            configure.append("-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64")
        subprocess.run(configure, check=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        subprocess.run(["cmake", "--build", str(cls.root_ / "build"), "--config", "Release"], check=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        suffix = ".dll" if sys.platform == "win32" else ".dylib" if sys.platform == "darwin" else ".so"
        cls.good_ = next((cls.root_ / "build").rglob("*good" + suffix))
        cls.bad_ = next((cls.root_ / "build").rglob("*bad" + suffix))

    @classmethod
    def tearDownClass(cls):
        cls.temp_.cleanup()

    def check_package(self, library, extension, module=True):
        package = self.root_ / self._testMethodName
        package.mkdir()
        if library:
            shutil.copyfile(library, package / ("BqLog" + extension))
        if module:
            (package / "bq.log.cppm").write_text("export module bq.log;\n", encoding="utf-8")
        return subprocess.run([self.bash_, str(repo_root / ".github/scripts/check_go_bridge_exports.sh"), str(package)], text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

    def test_node_exports(self):
        result = self.check_package(self.good_, ".node")
        self.assertEqual(result.returncode, 0, result.stdout)

    def test_names_are_not_exports(self):
        result = self.check_package(self.bad_, ".node")
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn("missing __api_go_log_write", result.stdout)

    def test_module_is_required(self):
        result = self.check_package(self.good_, ".node", module=False)
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn("no bq.log.cppm", result.stdout)

    def test_library_is_required(self):
        result = self.check_package(None, ".node")
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn("no BqLog dynamic library", result.stdout)


if __name__ == "__main__":
    unittest.main()
