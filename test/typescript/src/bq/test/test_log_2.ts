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
import { bq } from "./bq_lib.ts";
import { test_manager } from "./test_manager.ts";
import { Worker } from "worker_threads";
import { pathToFileURL, fileURLToPath } from "url";
import * as path from "path";



export class test_log_2 extends test_base {

    private log_inst_sync: any; // Type inference difficult without direct import, using any for internal instance
    private log_inst_async: any;
    private appender: string = "";

    constructor(name: string) {
        super(name);
        for (let i = 0; i < 32; ++i) {
            this.appender += "a";
        }
        // Initialize logs here or in test method, but usually safe to init if they persist
        this.log_inst_sync = bq.log.get_log_by_name("sync_log");
        this.log_inst_async = bq.log.get_log_by_name("async_log");
    }

    public async test(): Promise<test_result> {
        this.log_inst_sync = bq.log.create_log("sync_log", `appenders_config.FileAppender.type=compressed_file
                        appenders_config.FileAppender.time_zone=localtime
                        appenders_config.FileAppender.max_file_size=100000000
                        appenders_config.FileAppender.file_name=Output/sync_log
                        appenders_config.FileAppender.levels=[info, info, error,info]
                    
                        log.thread_mode=sync`);
        this.log_inst_async = bq.log.create_log("async_log", `appenders_config.FileAppender.type=compressed_file
                        appenders_config.FileAppender.time_zone=localtime
                        appenders_config.FileAppender.max_file_size=100000000
                        appenders_config.FileAppender.file_name=Output/async_log
                        appenders_config.FileAppender.levels=[error,info]
                    `);
        
        const result = new test_result();

        console.log("Starting Sync Test (NodeJS Workers)...");
        
        const lib_path = process.env.BQLOG_LIB_PATH || require.resolve("../../../../../wrapper/typescript/dist/cjs/index.js");

        await this.run_worker_pool("sync_log", 128, 5, 128, lib_path);
        console.log("Sync Test Finished");

        console.log("Starting Async Test (NodeJS Workers)...");
        await this.run_worker_pool("async_log", 64, 5, 2048, lib_path);
        console.log("Async Test Finished");


        this.log_inst_async.force_flush();
        result.add_result(true, "");
        
        const log_inst_console = bq.log.create_log("console_log", `appenders_config.Appender1.type=console
                        appenders_config.Appender1.time_zone=localtime
                        appenders_config.Appender1.levels=[all]
                        log.thread_mode=sync
                    `);
        bq.log.set_console_buffer_enable(false);
        bq.log.register_console_callback((log_id: bigint, category_idx: number, log_level: any, content: string) => {
            if(log_id != 0n)
            {
                result.add_result(log_id == log_inst_console.get_id(), "console callback test 1");
                result.add_result(log_level == bq.log_level.debug, "console callback test 2");
                result.add_result(content.endsWith("ConsoleTest"), "console callback test 3");
            }
        });
        log_inst_console.debug("ConsoleTest");
        bq.log.register_console_callback(null);
        return result;
    }

    private run_worker_pool(log_name: string, total_tasks: number, concurrent_limit: number, loops: number, lib_path: string): Promise<void> {
        const mode = process.env.BQLOG_TEST_MODE || 'CJS';
        if (mode === 'ESM' && !lib_path.startsWith('file:')) {
            lib_path = pathToFileURL(lib_path).href;
        }

        const worker_script_path = path.join(process.cwd(), 'src/bq/test/worker_script.ts');



        return new Promise((resolve, reject) => {
            let active = 0;
            let started = 0;
            let completed = 0;

            const start_workers = () => {
                while(active < concurrent_limit && started < total_tasks) {
                    active++;
                    started++;
                    
                    const worker = new Worker(worker_script_path, {
                        workerData: {
                            lib_path: lib_path,
                            log_name: log_name,
                            appender: this.appender,
                            count: loops,
                            mode: mode
                        }
                    });

                    worker.on('message', () => {
                        active--;
                        completed++;
                        if (completed === total_tasks) {
                            resolve();
                        } else {
                            start_workers();
                        }
                    });
                    
                    worker.on('error', (err) => {
                        reject(err);
                    });
                }
            };
            
            start_workers();
        });
    }
}

