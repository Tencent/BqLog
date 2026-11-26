# Copyright (C) 2026 Tencent.
# BQLOG is licensed under the Apache License, Version 2.0.
extends Control

@onready var _title: Label = $VBox/Title
@onready var _output: TextEdit = $VBox/Output
@onready var _btn_samples: Button = $VBox/Buttons/BtnLogSamples
@onready var _btn_bench: Button = $VBox/Buttons/BtnBenchmark
@onready var _btn_snapshot: Button = $VBox/Buttons/BtnSnapshot

var _logger: BqLog


func _ready() -> void:
	_title.text = "BqLog for Godot 4 Demo (v%s) | BaseDir: %s" % [
		BqLog.get_version(),
		BqLog.get_file_base_dir(0)
	]

	var config := """
		appenders_config.console.type=console
		appenders_config.console.time_zone=default local time
		appenders_config.console.levels=[all]

		appenders_config.compressed.type=compressed_file
		appenders_config.compressed.time_zone=default local time
		appenders_config.compressed.file_name=bqlog_demo/demo
		appenders_config.compressed.levels=[all]

		log.thread_mode=async
		snapshot.buffer_size=65536
		snapshot.levels=[all]
	"""
	_logger = BqLog.create_log("godot_demo_log", config)

	_btn_samples.pressed.connect(_on_write_samples)
	_btn_bench.pressed.connect(_on_run_benchmark)
	_btn_snapshot.pressed.connect(_on_take_snapshot)

	_append_ui("BqLog initialized. valid=%s, id=%d, name=%s" % [
		str(_logger.is_valid()),
		_logger.get_id(),
		_logger.get_name()
	])


func _on_write_samples() -> void:
	_logger.verbose("Verbose message: {}", 1)
	_logger.debug("Debug flag: {}, score: {}", true, 98.5)
	_logger.info("Player {} moved to {} with color {}", "Hero_扁鹊", Vector3(1.5, 2.0, -3.25), Color(0.2, 0.8, 1.0, 1.0))
	_logger.warning("Low HP warning: {} / {}", 15, 100)
	_logger.error("Connection timeout after {} ms", 3000)
	_logger.force_flush()
	_append_ui("Wrote 5 sample log entries (check Godot Output & user://bqlog_demo/).")


func _on_run_benchmark() -> void:
	var count := 10000
	var mem_before := Performance.get_monitor(Performance.MEMORY_STATIC)
	var t0 := Time.get_ticks_usec()
	for i in range(count):
		_logger.info("Bench entry #{}, pos={}, val={}", i, Vector3(1.0, 2.0, 3.0), 3.14159)
	var t1 := Time.get_ticks_usec()
	_logger.force_flush()
	var mem_after := Performance.get_monitor(Performance.MEMORY_STATIC)
	var elapsed_ms := float(t1 - t0) / 1000.0
	_append_ui("Logged %d entries in %.2f ms (%.2f us/entry), static mem delta: %.1f KB" % [
		count, elapsed_ms, float(t1 - t0) / float(count), (mem_after - mem_before) / 1024.0
	])


func _on_take_snapshot() -> void:
	_logger.force_flush()
	var snap := _logger.take_snapshot("localtime")
	_append_ui("--- SNAPSHOT ---\n" + snap)


func _append_ui(line: String) -> void:
	_output.text += line + "\n"
	_output.scroll_vertical = _output.get_line_count()
