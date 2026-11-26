# Copyright (C) 2026 Tencent.
# BQLOG is licensed under the Apache License, Version 2.0.
extends RefCounted


func disable_redirect() -> void:
	bq_log.set_godot_console_redirect_enable(false)


func write_from_gdscript(log_name: String, val: int) -> bool:
	var shared := bq_log.get_log_by_name(log_name)
	if shared == null or not shared.is_valid():
		return false
	return shared.info("from_gdscript: value={}", val)
