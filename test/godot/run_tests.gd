# Copyright (C) 2026 Tencent.
# BQLOG is licensed under the Apache License, Version 2.0.
#
# Automated headless test suite for BqLog Godot 4 GDExtension.
# Run with: godot --headless --path test/godot --script res://run_tests.gd
extends SceneTree

var _failed: int = 0
var _passed: int = 0
var _console_events: Array = []
var _reentrant_logger: BqLog = null
var _reentrant_calls: int = 0
var _done: bool = false
var _exit_code: int = 1


func _assert_true(cond: bool, msg: String) -> void:
	if cond:
		_passed += 1
	else:
		_failed += 1
		printerr("[FAIL] ", msg)


func _assert_eq(actual, expected, msg: String) -> void:
	if actual == expected:
		_passed += 1
	else:
		_failed += 1
		printerr("[FAIL] %s | expected=%s, actual=%s" % [msg, str(expected), str(actual)])


func _on_console_event(log_id: int, category_idx: int, level: int, content: String) -> void:
	_console_events.append({
		"log_id": log_id,
		"category_idx": category_idx,
		"level": level,
		"content": content,
	})


func _on_reentrant_console_event(_log_id: int, _category_idx: int, _level: int, _content: String) -> void:
	_reentrant_calls += 1
	# Trigger another synchronous log inside the console callback: must not recurse infinitely.
	if _reentrant_logger != null:
		_reentrant_logger.info("reentrant_inner_should_be_guarded")


func _initialize() -> void:
	print("=== BqLog Godot AutoTest Start ===")
	# Default redirect must match editor environment (OS.has_feature("editor")).
	_assert_eq(
		BqLog.is_godot_console_redirect_enabled(),
		OS.has_feature("editor") or Engine.is_editor_hint(),
		"default console redirect should only be enabled in editor"
	)
	# Disable default Godot console redirect during tests to keep stdout clean.
	BqLog.set_godot_console_redirect_enable(false)

	_test_basic_metadata()
	_test_create_and_lookup()
	_test_thread_modes()
	_test_all_types_and_snapshot()
	_test_reset_config_and_appender()
	_test_console_buffer_and_callback()
	_test_text_and_compressed_file_output()
	_test_category_log()
	_test_cpp_adapter_interop()
	_test_perf_and_zero_gc_smoke()

	BqLog.force_flush_all_logs()
	print("=== BqLog Godot AutoTest Done: passed=%d, failed=%d ===" % [_passed, _failed])
	_exit_code = 0 if _failed == 0 else 1
	_done = true


func _process(_delta: float) -> bool:
	if _done:
		quit(_exit_code)
		return true
	return false


func _test_basic_metadata() -> void:
	var ver := BqLog.get_version()
	_assert_true(not ver.is_empty(), "get_version() should not be empty")
	_assert_true(BqLog.get_platform_capabilities() >= 0, "get_platform_capabilities() should be non-negative")
	var base0 := BqLog.get_file_base_dir(0)
	_assert_true(not base0.is_empty(), "base_dir(0) should be initialized to user_data_dir")

	var temp_dir := OS.get_user_data_dir().path_join("bqlog_autotest_dir")
	DirAccess.make_dir_recursive_absolute(temp_dir)
	BqLog.reset_base_dir(0, temp_dir)
	_assert_eq(BqLog.get_file_base_dir(0), temp_dir, "reset_base_dir(0) should update base dir")


func _test_create_and_lookup() -> void:
	var cfg := """
		appenders_config.console.type=console
		appenders_config.console.levels=[all]
		log.thread_mode=sync
	"""
	var log_a := BqLog.create_log("godot_test_basic", cfg)
	_assert_true(log_a != null and log_a.is_valid(), "create_log should return valid BqLog")
	_assert_eq(log_a.get_name(), "godot_test_basic", "get_name() should match")
	_assert_true(log_a.get_id() != 0, "get_id() should be non-zero")

	var found := BqLog.get_log_by_name("godot_test_basic")
	_assert_true(found != null and found.is_valid(), "get_log_by_name should find existing log")
	_assert_eq(found.get_id(), log_a.get_id(), "get_log_by_name should return same log_id")

	var missing := BqLog.get_log_by_name("non_existent_log_xyz")
	_assert_true(missing != null and not missing.is_valid(), "missing log should be invalid")

	BqLog.enable_auto_crash_handle()
	BqLog.console(BqLog.LOG_LEVEL_INFO, "direct_device_console_smoke")


func _test_thread_modes() -> void:
	for mode: String in ["sync", "async", "independent"]:
		var cfg: String = """
			appenders_config.console.type=console
			appenders_config.console.levels=[all]
			log.thread_mode=%s
			snapshot.buffer_size=32768
			snapshot.levels=[all]
		""" % mode
		var logger := BqLog.create_log("godot_test_mode_" + mode, cfg)
		_assert_true(logger.is_valid(), "thread_mode=%s logger should be valid" % mode)
		for i in range(20):
			logger.info("mode_{}_entry_{}", mode, i)
		logger.force_flush()
		var snap := logger.take_snapshot("UTC+0")
		_assert_true(snap.contains("mode_%s_entry_0" % mode), "thread_mode=%s snapshot has first entry" % mode)
		_assert_true(snap.contains("mode_%s_entry_19" % mode), "thread_mode=%s snapshot has last entry" % mode)


func _test_all_types_and_snapshot() -> void:
	var cfg := """
		appenders_config.console.type=console
		appenders_config.console.levels=[all]
		log.thread_mode=sync
		snapshot.buffer_size=65536
		snapshot.levels=[all]
	"""
	var logger := BqLog.create_log("godot_test_types", cfg)
	_assert_true(logger.is_valid(), "types logger should be valid")

	_assert_true(logger.verbose("v_only"), "verbose no args")
	_assert_true(logger.debug("null={}, bool_t={}, bool_f={}", null, true, false), "debug null/bool")
	_assert_true(logger.info("int={}, neg={}, float={}", 9223372036854775807, -42, 3.14159), "info int/float")
	_assert_true(logger.warning("unicode={}, sname={}", "你好BqLog🚀", &"FastName"), "warning unicode/StringName")
	_assert_true(logger.error("v2={}, v3={}, col={}", Vector2(1.5, -2.5), Vector3(1, 2, 3), Color(0.5, 0.25, 1.0, 1.0)), "error math types")
	_assert_true(logger.fatal("dict={}", {"k": 1}), "fatal fallback stringify")

	# Test >32 args (heap fallback path in encoder)
	_assert_true(logger.info(
		"{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{}",
		1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
		17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34
	), "34 args should succeed")

	logger.force_flush()
	var snap := logger.take_snapshot("UTC+0")
	_assert_true(snap.contains("v_only"), "snapshot contains v_only")
	_assert_true(snap.contains("9223372036854775807"), "snapshot contains int64 max")
	_assert_true(snap.contains("你好BqLog🚀"), "snapshot contains utf32 unicode")
	_assert_true(snap.contains("FastName"), "snapshot contains StringName")
	_assert_true(snap.contains("(1, 2, 3)"), "snapshot contains Vector3")
	_assert_true(snap.contains("33,34"), "snapshot contains >32 args")


func _test_reset_config_and_appender() -> void:
	var cfg1 := """
		appenders_config.console.type=console
		appenders_config.console.levels=[error,fatal]
		log.thread_mode=sync
	"""
	var logger := BqLog.create_log("godot_test_reset", cfg1)
	_assert_true(not logger.info("filtered out"), "info should be filtered when only error/fatal enabled")
	_assert_true(logger.error("allowed error"), "error should pass")

	var cfg2 := """
		appenders_config.console.type=console
		appenders_config.console.levels=[info,error,fatal]
		log.thread_mode=sync
	"""
	_assert_true(logger.reset_config(cfg2), "reset_config should succeed")
	_assert_true(logger.info("now allowed"), "info should pass after reset_config")

	logger.set_appender_enable("console", false)
	logger.set_appender_enable("console", true)


func _test_console_buffer_and_callback() -> void:
	var cfg := """
		appenders_config.console.type=console
		appenders_config.console.levels=[info]
		log.thread_mode=sync
	"""
	var logger := BqLog.create_log("godot_test_console", cfg)

	# Callback test
	_console_events.clear()
	var cb := Callable(self, "_on_console_event")
	BqLog.register_console_callback(cb)
	logger.info("cb_marker_123")
	BqLog.unregister_console_callback(cb)
	var found_cb := false
	for ev in _console_events:
		if String(ev["content"]).contains("cb_marker_123"):
			found_cb = true
			break
	_assert_true(found_cb, "console callback should receive cb_marker_123")

	# Console buffer test
	_console_events.clear()
	BqLog.set_console_buffer_enable(true)
	logger.info("buf_marker_456")
	var fetched_count := 0
	while BqLog.fetch_and_remove_console_buffer(cb):
		fetched_count += 1
	BqLog.set_console_buffer_enable(false)
	_assert_true(fetched_count > 0, "console buffer should fetch at least one entry")
	var found_buf := false
	for ev in _console_events:
		if String(ev["content"]).contains("buf_marker_456"):
			found_buf = true
			break
	_assert_true(found_buf, "console buffer should contain buf_marker_456")

	# Reentrancy / infinite-loop guard test
	_reentrant_calls = 0
	_reentrant_logger = logger
	var reentrant_cb := Callable(self, "_on_reentrant_console_event")
	BqLog.register_console_callback(reentrant_cb)
	logger.info("reentrant_outer_trigger")
	BqLog.unregister_console_callback(reentrant_cb)
	_reentrant_logger = null
	_assert_eq(_reentrant_calls, 1, "reentrancy guard should prevent infinite console callback loop")


func _test_text_and_compressed_file_output() -> void:
	var cfg := """
		appenders_config.txt.type=text_file
		appenders_config.txt.file_name=file_out/godot_txt
		appenders_config.txt.levels=[all]

		appenders_config.comp.type=compressed_file
		appenders_config.comp.file_name=file_out/godot_comp
		appenders_config.comp.levels=[all]

		log.thread_mode=async
	"""
	var logger := BqLog.create_log("godot_test_files", cfg)
	_assert_true(logger.is_valid(), "file logger should be valid")
	for i in range(50):
		logger.info("file_line_#{}, text={}", i, "godot_bqlog_payload")
	logger.force_flush()

	var out_dir := BqLog.get_file_base_dir(0).path_join("file_out")
	var da := DirAccess.open(out_dir)
	_assert_true(da != null, "file_out directory should exist")
	var found_txt := false
	var found_comp := false
	var txt_content := ""
	if da != null:
		da.list_dir_begin()
		var fname := da.get_next()
		while fname != "":
			if fname.ends_with(".log") and fname.begins_with("godot_txt"):
				found_txt = true
				txt_content = FileAccess.get_file_as_string(out_dir.path_join(fname))
			elif fname.ends_with(".logcompr") and fname.begins_with("godot_comp"):
				found_comp = true
			fname = da.get_next()
		da.list_dir_end()
	_assert_true(found_txt, "text_file .log file should be created")
	_assert_true(txt_content.contains("file_line_#49, text=godot_bqlog_payload"), "text_file content should match")
	_assert_true(found_comp, "compressed .logcompr file should be created")


func _test_cpp_adapter_interop() -> void:
	if not ClassDB.class_exists(&"BqLogGodotCppTestHelper"):
		print("[INFO] Skipping C++ adapter test (built without BQLOG_GODOT_ENABLE_TEST_HELPER)")
		return
	var cfg := """
		appenders_config.console.type=console
		appenders_config.console.levels=[all]
		log.thread_mode=async
		snapshot.buffer_size=65536
		snapshot.levels=[all]
	"""
	var ok: bool = ClassDB.instantiate(&"BqLogGodotCppTestHelper").call("run_cpp_adapter_interop", "godot_cpp_shared_log", cfg)
	_assert_true(ok, "C++ adapter interop helper should succeed")

	var shared := BqLog.get_log_by_name("godot_cpp_shared_log")
	_assert_true(shared != null and shared.is_valid(), "GDScript should find log created by C++ bq::log")
	shared.info("gdscript_side_entry {}", 888)
	shared.force_flush()

	var snap := shared.take_snapshot("UTC+0")
	_assert_true(snap.contains("cpp_adapter: str=cpp_gd_str_你好, sn=cpp_gd_sname"), "snapshot contains C++ godot::String & StringName")
	_assert_true(snap.contains("cpp_fast_mode: str=cpp_gd_str_你好, num=777"), "snapshot contains C++ BQ_LOG_FAST_INFO")
	_assert_true(snap.contains("gdscript_side_entry 888"), "snapshot contains GDScript write on shared log")


func _test_category_log() -> void:
	var gen_path := "res://generated/test_category_log.gd"
	if not ResourceLoader.exists(gen_path):
		print("[INFO] Skipping generated category test (not generated at %s)" % gen_path)
		return
	var cls = load(gen_path)
	var cfg := """
		appenders_config.console.type=console
		appenders_config.console.levels=[all]
		appenders_config.console.categories_mask=[ModuleA.SystemA,ModuleB.Network]
		log.thread_mode=sync
		log.categories_mask=[ModuleA.SystemA,ModuleB.Network]
		snapshot.buffer_size=32768
		snapshot.levels=[all]
		snapshot.categories_mask=[ModuleA.SystemA,ModuleB.Network]
	"""
	var cat_log = cls.create_log("godot_test_cat", cfg)
	_assert_true(cat_log != null and cat_log.is_valid(), "generated category log should be valid")
	_assert_true(cat_log.get_categories_count() >= 5, "categories count should match")

	_assert_true(cat_log.info(cat_log.cat.ModuleA.SystemA, "allowed_cat_a {}", 101), "ModuleA.SystemA should be allowed")
	_assert_true(not cat_log.info(cat_log.cat.ModuleA.SystemB, "blocked_cat_b {}", 202), "ModuleA.SystemB should be filtered out")
	_assert_true(cat_log.warning(cat_log.cat.ModuleB.Network, "allowed_net {}", "ok"), "ModuleB.Network should be allowed")

	var found = cls.get_log_by_name("godot_test_cat")
	_assert_true(found != null and found.is_valid(), "category get_log_by_name should succeed")
	_assert_eq(found.get_id(), cat_log.get_id(), "category get_log_by_name should return same id")

	var snap: String = cat_log.take_snapshot("UTC+0")
	_assert_true(snap.contains("allowed_cat_a 101"), "category snapshot contains allowed_cat_a")
	_assert_true(not snap.contains("blocked_cat_b"), "category snapshot excludes blocked_cat_b")
	_assert_true(snap.contains("allowed_net ok"), "category snapshot contains allowed_net")


func _test_perf_and_zero_gc_smoke() -> void:
	var cfg := """
		appenders_config.comp.type=compressed_file
		appenders_config.comp.file_name=perf_out/godot_perf
		appenders_config.comp.levels=[warning,error,fatal]
		log.thread_mode=async
	"""
	var logger := BqLog.create_log("godot_test_perf", cfg)
	_assert_true(logger.is_valid(), "perf logger should be valid")

	var t0 := Time.get_ticks_usec()
	for i in range(50000):
		logger.debug("filtered out fast path {}", i)
	var filtered_us := Time.get_ticks_usec() - t0

	var t1 := Time.get_ticks_usec()
	for i in range(5000):
		logger.warning("active perf entry {}, vec={}", i, Vector3(1.0, 2.0, 3.0))
	var active_us := Time.get_ticks_usec() - t1
	logger.force_flush()

	print("[PERF] 50k filtered=%.2f ms, 5k active=%.2f ms" % [filtered_us / 1000.0, active_us / 1000.0])
	_assert_true(filtered_us < 2000000, "50k filtered calls should complete well under 2s")
	_assert_true(active_us < 2000000, "5k active calls should complete well under 2s")
