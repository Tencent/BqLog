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
Core log class for BqLog Python wrapper.
Provides Pythonic API matching Java/TypeScript/C# wrappers.
All function and variable names use snake_case.
"""

from bq.impl import log_invoker
from bq.defs.log_level import log_level
from bq.defs.log_category_base import log_category_base


class log:
    """BqLog log object. Use static methods to create/find logs and instance methods to write."""

    _callback = None

    def __init__(self, log_id=0):
        self._log_id = log_id
        self._name = ""
        self._categories_name_array = []
        if log_id != 0:
            name = log_invoker.get_log_name_by_id(log_id)
            if name is None:
                self._log_id = 0
                return
            self._name = name
            cat_count = log_invoker.get_log_categories_count(log_id)
            for i in range(cat_count):
                cat_name = log_invoker.get_log_category_name_by_index(log_id, i)
                if cat_name is not None:
                    self._categories_name_array.append(cat_name)

    # ==================== Static methods ====================

    @staticmethod
    def get_version():
        """Get bqLog library version string."""
        return log_invoker.get_log_version()

    @staticmethod
    def enable_auto_crash_handle():
        """
        Enable auto crash handler. If bqLog is asynchronous, a crash may cause
        buffered logs not to be persisted. This attempts a forced flush on crash.
        """
        log_invoker.enable_auto_crash_handler()

    @staticmethod
    def get_recovery_capability():
        """
        Whether crash recovery can work here, and what is missing when it cannot.
        Compare the result with bq.defs.recovery_capability.
        """
        return log_invoker.get_recovery_capability()

    @staticmethod
    def get_platform_capabilities():
        """
        What the linked library was built with; attach it to a bug report.
        Test the bits with bq.defs.platform_capability.
        """
        return log_invoker.get_platform_capabilities()

    @staticmethod
    def create_log(name, config):
        """
        Create a log object.

        Args:
            name: Log name. Empty string for auto-assigned name. Existing name
                  returns previous object with new config.
            config: Log configuration string.

        Returns:
            A log object. Check is_valid() to verify creation success.
        """
        if not config:
            return log(0)
        log_id = log_invoker.create_log(name, config, 0, None)
        return log(log_id)

    @staticmethod
    def get_log_by_name(log_name):
        """
        Get a log object by its name.

        Args:
            log_name: Name of the log to find.

        Returns:
            A log object. Check is_valid() if the log was not found.
        """
        if not log_name:
            return log(0)
        count = log_invoker.get_logs_count()
        for i in range(count):
            lid = log_invoker.get_log_id_by_index(i)
            name = log_invoker.get_log_name_by_id(lid)
            if name == log_name:
                return log(lid)
        return log(0)

    @staticmethod
    def force_flush_all_logs():
        """Synchronously flush the buffer of all log objects."""
        log_invoker.force_flush(0)

    @staticmethod
    def register_console_callback(callback):
        """
        Register a callback invoked whenever a console log message is output.

        Args:
            callback: A callable(log_id, category_idx, level, content) or None to unregister.
        """
        log._callback = callback
        if callback is not None:
            log_invoker.register_console_callback(log._inner_callback)
        else:
            log_invoker.register_console_callback(None)

    @staticmethod
    def unregister_console_callback(callback):
        """Unregister a previously registered console callback."""
        if log._callback is callback:
            log_invoker.register_console_callback(None)
            log._callback = None

    @staticmethod
    def set_console_buffer_enable(enable):
        """
        Enable or disable the console appender buffer.
        When enabled, console outputs are saved in a buffer until fetched.
        """
        log_invoker.set_console_buffer_enable(enable)

    @staticmethod
    def fetch_and_remove_console_buffer(on_console_callback):
        """
        Fetch and remove a log entry from the console appender buffer.

        Args:
            on_console_callback: A callable(log_id, category_idx, level, content).

        Returns:
            True if the buffer was not empty and a log entry was fetched.
        """
        return log_invoker.fetch_and_remove_console_buffer(on_console_callback)

    @staticmethod
    def console(level, content):
        """
        Output to console with specified log_level.
        This is not a log entry and cannot be caught by console callbacks.
        """
        log_invoker.log_device_console(level, content)

    @staticmethod
    def get_file_base_dir(base_dir_type=0):
        """Get file base directory path."""
        return log_invoker.get_file_base_dir(base_dir_type)

    @staticmethod
    def reset_base_dir(base_dir_type, dir_path):
        """Reset the base directory at runtime."""
        log_invoker.reset_base_dir(base_dir_type, dir_path)

    @staticmethod
    def _inner_callback(log_id, category_idx, level, content, length):
        """Internal callback bridge from C extension to Python callback."""
        if log._callback:
            log._callback(log_id, category_idx, level, content)

    # ==================== Instance methods ====================

    def _do_log(self, level, *args):
        """
        Internal log writing method.
        Supports optional category as first argument:
            log.info(format_str, ...)
            log.info(category, format_str, ...)
        Returns True if the log was successfully written.
        """
        if self._log_id == 0:
            return False

        category_idx = 0
        fmt_args_start = 0

        if len(args) >= 1 and isinstance(args[0], log_category_base):
            category_idx = args[0].index
            fmt_args_start = 1

        if len(args) <= fmt_args_start:
            return False

        # Fast path: check if this level is enabled before entering C extension
        if not log_invoker.is_enable_for(self._log_id, category_idx, level):
            return False

        format_str = args[fmt_args_start]
        if format_str is None:
            format_str = ""
        elif not isinstance(format_str, str):
            format_str = str(format_str)

        extra_args = args[fmt_args_start + 1:]
        return log_invoker.log_write(self._log_id, level, category_idx, format_str, *extra_args)

    def verbose(self, *args):
        """Write a verbose log entry."""
        return self._do_log(log_level.verbose, *args)

    def debug(self, *args):
        """Write a debug log entry."""
        return self._do_log(log_level.debug, *args)

    def info(self, *args):
        """Write an info log entry."""
        return self._do_log(log_level.info, *args)

    def warning(self, *args):
        """Write a warning log entry."""
        return self._do_log(log_level.warning, *args)

    def error(self, *args):
        """Write an error log entry."""
        return self._do_log(log_level.error, *args)

    def fatal(self, *args):
        """Write a fatal log entry."""
        return self._do_log(log_level.fatal, *args)

    def reset_config(self, config):
        """Modify the log configuration. Some fields like buffer_size cannot be modified."""
        if not config:
            return
        log_invoker.log_reset_config(self._name, config)

    def set_appender_enable(self, appender_name, enable):
        """Temporarily disable or enable a specific appender."""
        log_invoker.set_appender_enable(self._log_id, appender_name, enable)

    def force_flush(self):
        """Synchronously flush the buffer of this log object."""
        log_invoker.force_flush(self._log_id)

    def get_id(self):
        """Get the id of this log object."""
        return self._log_id

    def is_valid(self):
        """Whether this log object is valid."""
        return self._log_id != 0

    def get_name(self):
        """Get the name of this log."""
        return self._name

    def take_snapshot(self, time_zone_config=""):
        """
        Take a snapshot of the log buffer and decode it to text.
        Works only when snapshot is configured.

        Args:
            time_zone_config: Time display config, e.g. "localtime", "gmt", "UTC+8".

        Returns:
            The decoded snapshot buffer as a string.
        """
        return log_invoker.take_snapshot_string(self._log_id, time_zone_config or "")
