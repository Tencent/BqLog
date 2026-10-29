# Copyright (C) 2025 Tencent.
# BQLOG is licensed under the Apache License, Version 2.0.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.

"""
Internal log invoker that wraps the _bqlog C Extension module.
All functions use snake_case and map 1:1 to the C extension functions.
"""

import platform
import sys

try:
    from bq import _bqlog
except ImportError:
    raise ImportError(
        "BqLog native extension (_bqlog) not found.\n"
        "  Your platform: {} {} (Python {})\n"
        "  Pre-built wheels are available for:\n"
        "    - Windows x86_64 / ARM64\n"
        "    - Linux x86_64 / ARM64 (manylinux)\n"
        "    - macOS x86_64 / ARM64 (universal2)\n"
        "    - FreeBSD x86_64 \n"
        "  To build from source, see: https://github.com/Tencent/BqLog".format(
            platform.system(), platform.machine(), sys.version.split()[0]
        )
    )


def get_log_version():
    """Get BqLog version string."""
    return _bqlog.get_log_version()


def enable_auto_crash_handler():
    """Enable auto crash handler."""
    _bqlog.enable_auto_crash_handler()


def get_recovery_capability():
    """What this platform offers for crash recovery. Returns a recovery_capability value."""
    return _bqlog.get_recovery_capability()


def get_platform_capabilities():
    """Capability bits the library was built with. Compare with bq.defs.platform_capability."""
    return _bqlog.get_platform_capabilities()


def create_log(name, config, categories_count, categories):
    """Create a log object. Returns log_id (0 means failure)."""
    return _bqlog.create_log(name, config, categories_count, categories)


def log_reset_config(log_name, config):
    """Reset log configuration."""
    return _bqlog.log_reset_config(log_name, config)


def log_write(log_id, level, category_idx, format_str, *args):
    """Write a log entry. Returns True on success."""
    return _bqlog.log_write(log_id, level, category_idx, format_str, *args)


def set_appender_enable(log_id, appender_name, enable):
    """Enable or disable an appender."""
    _bqlog.set_appender_enable(log_id, appender_name, enable)


def get_logs_count():
    """Get the count of log objects."""
    return _bqlog.get_logs_count()


def get_log_id_by_index(index):
    """Get log id by index."""
    return _bqlog.get_log_id_by_index(index)


def get_log_name_by_id(log_id):
    """Get log name by id. Returns None if not found."""
    return _bqlog.get_log_name_by_id(log_id)


def get_log_categories_count(log_id):
    """Get log categories count."""
    return _bqlog.get_log_categories_count(log_id)


def get_log_category_name_by_index(log_id, index):
    """Get log category name by index."""
    return _bqlog.get_log_category_name_by_index(log_id, index)


def get_log_merged_log_level_bitmap_by_log_id(log_id):
    """Get merged log level bitmap."""
    return _bqlog.get_log_merged_log_level_bitmap_by_log_id(log_id)


def get_log_category_masks_array_by_log_id(log_id, category_count):
    """Get category masks array."""
    return _bqlog.get_log_category_masks_array_by_log_id(log_id, category_count)


def get_log_print_stack_level_bitmap_by_log_id(log_id):
    """Get print stack level bitmap."""
    return _bqlog.get_log_print_stack_level_bitmap_by_log_id(log_id)


def log_device_console(level, content):
    """Output to device console."""
    _bqlog.log_device_console(level, content)


def force_flush(log_id):
    """Force flush log buffer."""
    _bqlog.force_flush(log_id)


def get_file_base_dir(base_dir_type):
    """Get file base directory."""
    return _bqlog.get_file_base_dir(base_dir_type)


def reset_base_dir(base_dir_type, dir_path):
    """Reset base directory."""
    _bqlog.reset_base_dir(base_dir_type, dir_path)


def log_decoder_create(path, priv_key):
    """Create a log decoder. Returns (result_code, handle)."""
    return _bqlog.log_decoder_create(path, priv_key)


def log_decoder_decode(handle):
    """Decode a log entry. Returns (result_code, text)."""
    return _bqlog.log_decoder_decode(handle)


def log_decoder_destroy(handle):
    """Destroy a log decoder."""
    _bqlog.log_decoder_destroy(handle)


def log_decode(in_path, out_path, priv_key):
    """Decode a log file to text file. Returns True on success."""
    return _bqlog.log_decode(in_path, out_path, priv_key)


def register_console_callback(callback):
    """Register a console callback. Pass None to unregister."""
    _bqlog.register_console_callback(callback)


def unregister_console_callback():
    """Unregister the console callback."""
    _bqlog.unregister_console_callback()


def set_console_buffer_enable(enable):
    """Enable or disable console buffer."""
    _bqlog.set_console_buffer_enable(enable)


def fetch_and_remove_console_buffer(callback):
    """Fetch and remove a console buffer entry. Returns True if buffer was not empty."""
    return _bqlog.fetch_and_remove_console_buffer(callback)


def take_snapshot_string(log_id, time_zone_config):
    """Take snapshot string."""
    return _bqlog.take_snapshot_string(log_id, time_zone_config)


def get_stack_trace(skip_frame_count):
    """Get stack trace."""
    return _bqlog.get_stack_trace(skip_frame_count)


def is_enable_for(log_id, category_idx, level):
    """Check if log level is enabled for category. Fast path check."""
    return _bqlog.is_enable_for(log_id, category_idx, level)
