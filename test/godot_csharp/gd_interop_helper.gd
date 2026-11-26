# Copyright (C) 2026 Tencent.
# BQLOG is licensed under the Apache License, Version 2.0.
extends RefCounted


func disable_redirect() -> void:
	BqLog.set_godot_console_redirect_enable(false)


func write_from_gdscript(log_name: String, val: int) -> bool:
	var shared := BqLog.get_log_by_name(log_name)
	if shared == null or not shared.is_valid():
		return false
	return shared.info("from_gdscript: value={}", val)
