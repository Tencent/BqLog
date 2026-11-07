# Copyright (C) 2026 Tencent.
# BQLOG is licensed under the Apache License, Version 2.0.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.

"""
Text that bq hands back to Python used to be cut at the first '\\0'. Every path
that returns log text has to return all of it.
"""

import os
from bq.test.test_base import test_base
from bq.test.test_result import test_result
from bq.test.test_manager import test_manager
from bq.log import log
from bq.tools.log_decoder import log_decoder, appender_decode_result

TEXT = "before\x00after \u4e2d\u6587 \U0001F600"


def _find_compressed_file(folder_path, file_prefix):
    newest = None
    if not os.path.isdir(folder_path):
        return None
    # The appender may nest the file under a directory named after it.
    for root, _, files in os.walk(folder_path):
        for file_name in files:
            if file_name.startswith(file_prefix) and file_name.endswith(".logcompr"):
                file_path = os.path.join(root, file_name)
                if newest is None or os.path.getmtime(file_path) > os.path.getmtime(newest):
                    newest = file_path
    return newest


class test_log_embedded_nul(test_base):
    def __init__(self, name):
        super().__init__(name)

    def test(self):
        result = test_result()
        log_inst = log.create_log("embedded_nul_log",
            "log.thread_mode=sync\n"
            "log.recovery=false\n"
            "appenders_config.ConsoleAppender.type=console\n"
            "appenders_config.ConsoleAppender.levels=[all]\n"
            "appenders_config.FileAppender.type=compressed_file\n"
            "appenders_config.FileAppender.file_name=Output/embedded_nul\n"
            "appenders_config.FileAppender.base_dir_type=0\n"
            "appenders_config.FileAppender.levels=[all]\n"
            "snapshot.buffer_size=65536\n"
            "snapshot.levels=[all]\n")
        result.add_result(log_inst.is_valid(), "the embedded NUL test log is valid")
        log_id = log_inst.get_id()

        log.set_console_buffer_enable(False)
        callback_text = []

        def on_console(cb_log_id, cb_category_idx, cb_level, cb_content):
            if cb_log_id == log_id:
                callback_text.append(cb_content)

        log.register_console_callback(on_console)
        log_inst.info(TEXT)
        log.register_console_callback(None)
        result.add_result(len(callback_text) == 1 and callback_text[0].endswith(TEXT),
            "console callback keeps the text after NUL")

        log.set_console_buffer_enable(True)
        log_inst.info(TEXT)
        fetched = test_manager.get_console_output()
        result.add_result(fetched is not None and fetched.endswith(TEXT),
            "console buffer keeps the text after NUL")

        snapshot = log_inst.take_snapshot("gmt")
        result.add_result(snapshot.endswith(TEXT + "\n"), "snapshot keeps the text after NUL")

        log_inst.force_flush()
        input_path = _find_compressed_file("Output", "embedded_nul")
        result.add_result(input_path is not None, "the compressed appender wrote a log file")
        if input_path is not None:
            decoder = log_decoder(os.path.abspath(input_path))
            last_entry = None
            while decoder.decode() == appender_decode_result.success:
                last_entry = decoder.get_last_decoded_log_entry()
            decoder.destroy()
            result.add_result(last_entry is not None and TEXT in last_entry,
                "decoder keeps the text after NUL")

        return result
