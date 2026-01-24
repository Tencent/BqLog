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
import { bq } from "./bq_lib.ts";
import { test_base } from "./test_base.ts";
import { test_result } from "./test_result.ts";

export class test_manager {
    private static test_list: test_base[] = [];
    private static log_console_output: string = "";

    // Use any for level type to avoid complex type import issues
    private static default_callback(log_id: bigint, category_idx: number, level: any, content: string): void {
        if (log_id !== 0n) {
            test_manager.log_console_output = content;
        } else {
            console.log(content);
        }
    }

    public static add_test(test_obj: test_base): void {
        test_manager.test_list.push(test_obj);
    }

    public static register_default_console_callback(): void {
        // Explicitly disable console buffer to ensure direct callbacks
        // and avoid potential ring buffer initialization issues in NAPI environment.
        bq.log.set_console_buffer_enable(false);
        bq.log.register_console_callback(test_manager.default_callback as any);
    }
    public static async test(): Promise<boolean> {
        test_manager.register_default_console_callback();
        let success = true;
        for (const test_obj of test_manager.test_list) {
            const result = await test_obj.test();
            result.output(test_obj.get_name());
            success = success && result.is_all_pass();
        }
        return success;
    }

    public static get_console_output(): string {
        return test_manager.log_console_output;
    }
}
