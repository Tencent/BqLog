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
import { test_manager } from "./test_manager";

export class test_result {
    private errors: string[] = [];
    private pass_count: number = 0;

    public add_result(result: boolean, error_msg: string): void {
        if (result) {
            this.pass_count++;
        } else {
            this.errors.push(error_msg);
        }
    }

    public check_log_output_end_with(standard_log: string, error_msg: string): void {
        const output = test_manager.get_console_output();
        let result = false;
        if (output && output.trimEnd().endsWith(standard_log)) {
            result = true;
        }
        this.add_result(result, `${error_msg} [Expected ends with: ${standard_log}, Actual: ${output}]`);
    }

    public is_all_pass(): boolean {
        return this.errors.length === 0;
    }

    public output(test_name: string): void {
        if (this.is_all_pass()) {
            console.log("[PASS] " + test_name);
        } else {
            console.log("[FAIL] " + test_name);
            for (const err of this.errors) {
                console.log("    - " + err);
            }
        }
    }
}
