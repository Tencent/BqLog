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
import { Worker, isMainThread, parentPort, workerData } from "worker_threads";
import { pathToFileURL } from "url";

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
        this.log_inst_sync = bq.log.get_log_by_name("sync_log");
        this.log_inst_async = bq.log.get_log_by_name("async_log");
        
        const result = new test_result();
        bq.log.register_console_callback(null);

        console.log("Starting Sync Test (NodeJS Workers)...");
        
        // Locate library path for workers from env set by test_main
        const libPath = process.env.BQLOG_LIB_PATH || require.resolve("../../../../../wrapper/typescript/dist/cjs/index.js");

        // Sync Log Stress
        await this.run_worker_pool("sync_log", 100, 5, 128, libPath);
        console.log("Sync Test Finished");

        // Async Log Stress
        await this.run_worker_pool("async_log", 32, 5, 2048, libPath);
        console.log("Async Test Finished");

        this.log_inst_async.force_flush();
        result.add_result(true, "");
        test_manager.register_default_console_callback();
        return result;
    }

    private run_worker_pool(logName: string, total_tasks: number, concurrent_limit: number, loops: number, libPath: string): Promise<void> {
        const mode = process.env.BQLOG_TEST_MODE || 'CJS';
        if (mode === 'ESM' && !libPath.startsWith('file:')) {
            libPath = pathToFileURL(libPath).href;
        }

        return new Promise((resolve, reject) => {
            let active = 0;
            let started = 0;
            let completed = 0;

            const start_workers = () => {
                while(active < concurrent_limit && started < total_tasks) {
                    active++;
                    started++;
                    
                    const worker = new Worker(`
                        const { parentPort, workerData } = require('worker_threads');
                        
                        let bq;
                        if (workerData.mode === 'ESM') {
                            // Async import in worker eval is tricky, we use a simple hack:
                            // We can't use top-level await in eval in all node versions.
                            // But we can use dynamic import().then().
                            import(workerData.libPath).then(mod => {
                                bq = mod.bq;
                                start();
                            });
                        } else {
                            // CJS
                            const mod = require(workerData.libPath);
                            bq = mod.bq;
                            start();
                        }

                        async function start() {
                            const log_inst = bq.log.get_log_by_name(workerData.logName);
                            const appender = workerData.appender;
                            const count = workerData.count;
                            
                            let log_content = "";
                            for(let i=0; i<count; i++) {
                                log_content += appender;
                                log_inst.info(log_content);
                                if (i % 100 === 0) {
                                    await new Promise(resolve => setImmediate(resolve));
                                }
                            }
                            parentPort.postMessage('done');
                        }
                    `, {
                        eval: true,
                        workerData: {
                            libPath: libPath,
                            logName: logName,
                            appender: this.appender,
                            count: loops,
                            mode: process.env.BQLOG_TEST_MODE || 'CJS'
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
