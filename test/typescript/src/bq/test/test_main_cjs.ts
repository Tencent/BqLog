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
import { test_manager } from "./test_manager.ts";
import { test_log_1 } from "./test_log_1.ts";
import { test_log_2 } from "./test_log_2.ts";
import { test_log_category } from "./test_log_category.ts";
import { bq } from "@pippocao/bqlog";

async function run_tests() {
    try {
        console.log(`Process PID: ${process.pid}`);
        const version = bq.log.get_version();
        console.log("BqLog Version: " + version);
        if (!version) {
            throw new Error("Failed to get version");
        }
        // The recovery capability query must be reachable from this wrapper as well.
        console.log("Recovery capability: " + bq.log.get_recovery_capability());

        test_manager.add_test(new test_log_1("Test Log Basic"));
        test_manager.add_test(new test_log_2("Test Log MultiThread"));
        test_manager.add_test(new test_log_category("Test Log Category"));

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

async function main() {
    console.log("Running TypeScript Wrapper Tests (CJS Mode)...");
    try {
        // @pippocao/bqlog is installed via npm (from tgz), loaded directly above
        process.env.BQLOG_TEST_MODE = "CJS";
        process.env.BQLOG_LIB_PATH = require.resolve("@pippocao/bqlog");

        await run_tests();

    } catch (e) {
        console.error("Failed to load CJS library", e);
        process.exit(1);
    }
}

main();
