/*
 * Copyright (C) 2026 Tencent.
 * BQLOG is licensed under the Apache License, Version 2.0.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */
import { test_base } from "./test_base.ts";
import { test_result } from "./test_result.ts";
import { test_manager } from "./test_manager.ts";
import { find_compressed_file } from "./test_log_decoder.ts";
import { bq } from "@pippocao/bqlog";
import * as path from "path";

// Text that bq hands back to JavaScript used to be cut at the first '\0'. Every path that returns
// log text has to return all of it.
const TEXT = "before\0after \u4e2d\u6587 \u{1F600}";

export class test_log_embedded_nul extends test_base {
    constructor(name: string) {
        super(name);
    }

    public async test(): Promise<test_result> {
        const result = new test_result();
        const log_inst = bq.log.create_log("embedded_nul_log", `log.thread_mode=sync
                        log.recovery=false
                        appenders_config.ConsoleAppender.type=console
                        appenders_config.ConsoleAppender.levels=[all]
                        appenders_config.FileAppender.type=compressed_file
                        appenders_config.FileAppender.file_name=Output/embedded_nul
                        appenders_config.FileAppender.base_dir_type=0
                        appenders_config.FileAppender.levels=[all]
                        snapshot.buffer_size=65536
                        snapshot.levels=[all]`);
        result.add_result(log_inst.is_valid(), "the embedded NUL test log is valid");
        const log_id = log_inst.get_id();

        bq.log.set_console_buffer_enable(false);
        let callback_text: string | null = null;
        bq.log.register_console_callback((id: bigint, category_idx: number, level: any, content: string) => {
            if (id == log_id) {
                callback_text = content;
            }
        });
        log_inst.info(TEXT);
        bq.log.register_console_callback(null);
        result.add_result(callback_text !== null && (callback_text as string).endsWith(TEXT),
            `console callback keeps the text after NUL, got ${JSON.stringify(callback_text)}`);

        bq.log.set_console_buffer_enable(true);
        log_inst.info(TEXT);
        const fetched = await test_manager.get_console_output();
        result.add_result(fetched !== null && fetched.endsWith(TEXT),
            `console buffer keeps the text after NUL, got ${JSON.stringify(fetched)}`);

        const snapshot = log_inst.take_snapshot("gmt");
        result.add_result(snapshot.endsWith(TEXT + "\n"),
            `snapshot keeps the text after NUL, got ${JSON.stringify(snapshot)}`);

        log_inst.force_flush();
        const input_path = find_compressed_file("Output", "embedded_nul");
        result.add_result(input_path !== null, "the compressed appender wrote a log file");
        if (input_path !== null) {
            const decoder = new bq.log_decoder(path.resolve(input_path));
            let last_entry: string | null = null;
            while (decoder.decode() === bq.appender_decode_result.success) {
                last_entry = decoder.get_last_decoded_log_entry();
            }
            result.add_result(last_entry !== null && (last_entry as string).includes(TEXT),
                `decoder keeps the text after NUL, got ${JSON.stringify(last_entry)}`);
        }
        return result;
    }
}
