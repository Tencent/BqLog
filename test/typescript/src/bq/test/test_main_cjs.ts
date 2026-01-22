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
import { test_log_1 } from "./test_log_1";
import { test_log_2 } from "./test_log_2";
import { set_bq_lib, bq } from "./bq_lib";

async function run_tests() {
    try {
        const version = bq.log.get_version();
        console.log("BqLog Version: " + version);
        if (!version) {
            throw new Error("Failed to get version");
        }

        test_manager.add_test(new test_log_1("Test Log Basic"));
        test_manager.add_test(new test_log_2("Test Log MultiThread"));
        
        const result = await test_manager.test();
        if (result) {
            bq.log.console(bq.log_level.info, "--------------------------------");
            bq.log.console(bq.log_level.info, "CONGRATULATION!!! ALL TEST CASES IS PASSED");
            bq.log.console(bq.log_level.info, "--------------------------------");
        } else {
            bq.log.console(bq.log_level.error, "--------------------------------");
            bq.log.console(bq.log_level.error, "SORRY!!! TEST CASES FAILED");
            bq.log.console(bq.log_level.error, "--------------------------------");
            process.exit(-1);
        }
    } catch (t) {
        console.log(t);
        // Fallback console if bq log fails
        console.error("SORRY!!! TEST CASES FAILED");
        process.exit(-1);
    }
}

// CJS Entry Point
// We use dynamic require to load the CJS wrapper
async function main() {
    console.log("Running TypeScript Wrapper Tests (CJS Mode)...");
    try {
        // Path to CJS build
        const libPath = "../../../../../wrapper/typescript/dist/cjs/index.js";
        const { bq } = require(libPath);
        set_bq_lib(bq);
        
        // Hack for test_log_2 worker path resolution in CJS mode
        // We set a global variable or rely on process.env to tell workers where to look
        process.env.BQLOG_TEST_MODE = "CJS";
        process.env.BQLOG_LIB_PATH = require.resolve(libPath);

        console.log(`PID:${process.pid}`);

        if (process.stdin.isTTY) {
            process.stdin.setRawMode(true);
        }
        process.stdin.resume();
        process.stdin.once("data", () => {
            if (process.stdin.isTTY) {
                process.stdin.setRawMode(false);
            }
            process.stdin.pause();
            run_tests();
        });

    } catch (e) {
        console.error("Failed to load CJS library", e);
        process.exit(1);
    }
}

main();
