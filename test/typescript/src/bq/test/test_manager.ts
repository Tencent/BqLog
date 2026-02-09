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
    private static fetch_count: number = 0;
    private static last_fetch_count: number = 0;
    private static fetch_timer: NodeJS.Timeout | null = null;

    public static add_test(test_obj: test_base): void {
        test_manager.test_list.push(test_obj);
    }

    public static async test(): Promise<boolean> {
        bq.log.set_console_buffer_enable(true);
        let last_fetch_count = 0;
        test_manager.fetch_timer = setInterval(() => {
            let new_fetch_count = test_manager.fetch_count;
            while(true) {
                const fetch_result = bq.log.fetch_and_remove_console_buffer((log_id, category_idx, log_level, content) => {
                    test_manager.log_console_output = content;
                });
                if (!fetch_result) {
                    break;
                }
            }
            if(new_fetch_count != last_fetch_count) {
                ++new_fetch_count;
                test_manager.fetch_count = new_fetch_count;
            }
            last_fetch_count = new_fetch_count;
        }, 1);

        let success = true;
        for (const test_obj of test_manager.test_list) {
            const result = await test_obj.test();
            result.output(test_obj.get_name());
            success = success && result.is_all_pass();
        }
        
        if (test_manager.fetch_timer) {
            clearInterval(test_manager.fetch_timer);
            test_manager.fetch_timer = null;
        }
        return success;
    }

    public static async get_console_output(): Promise<string> {
        const prev = test_manager.fetch_count;
        test_manager.fetch_count = prev + 1;
        while (test_manager.fetch_count !== prev + 2) {
            await new Promise(resolve => setTimeout(resolve, 1));
        }
        return test_manager.log_console_output;
    }
}
