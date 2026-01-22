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
import { test_manager } from "./test_manager.js";
import { test_log_1 } from "./test_log_1.js";
import { test_log_2 } from "./test_log_2.js";
import { set_bq_lib, bq } from "./bq_lib.js";
import { createRequire } from 'module';

// Polyfill require for some environments if needed, but we use import here.
const require = createRequire(import.meta.url);

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
        console.error("SORRY!!! TEST CASES FAILED");
        process.exit(-1);
    }
}

// ESM Entry Point
async function main() {
    console.log("Running TypeScript Wrapper Tests (ESM Mode)...");
    try {
        // Path to ESM build
        // Note: We need to use full extension .js for ESM imports in Node
        const libPath = "../../../../../wrapper/typescript/dist/esm/index.js";
        const mod = await import(libPath);
        set_bq_lib(mod.bq);

        process.env.BQLOG_TEST_MODE = "ESM";
        // Resolve absolute path for workers
        process.env.BQLOG_LIB_PATH = require.resolve(libPath);

        await run_tests();
    } catch (e) {
        console.error("Failed to load ESM library", e);
        process.exit(1);
    }
}

main();
