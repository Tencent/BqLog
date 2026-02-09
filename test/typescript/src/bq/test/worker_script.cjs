const { parentPort, workerData } = require('worker_threads');

async function start() {
    const mod = require(workerData.libPath);
    const bq = mod.bq;

    const log_inst = bq.log.get_log_by_name(workerData.logName);
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
    parentPort.postMessage('done');
}

start().catch(err => {
    console.error('Worker error:', err);
    process.exit(1);
});

