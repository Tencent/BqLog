import { parentPort, workerData } from "worker_threads";

async function start() {
    let bq;
    if (workerData.mode === 'ESM') {
        const mod = await import(workerData.lib_path);
        bq = mod.bq;
    } else {
        const mod = require(workerData.lib_path);
        bq = mod.bq;
    }

    const log_inst = bq.log.get_log_by_name(workerData.log_name);
    const appender = workerData.appender;
    const count = workerData.count;
    
    let log_content = "";
    for (let i = 0; i < count; i++) {
        log_content += appender;
        log_inst.info(log_content);
        if (i % 100 === 0) {
            await new Promise(resolve => setImmediate(resolve));
        }
    }
    parentPort!.postMessage('done');
}

start().catch(err => {
    console.error('Worker error:', err);
    process.exit(1);
});