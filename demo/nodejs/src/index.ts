//Use This when your are using ESM
import { bq } from "bqlog"
import { demo_category_log } from "./demo_category_log.js";


//Use This if you are using CJS
//const { bq } = require("bqlog");   

console.log('cwd =', process.cwd());
console.log('PID =', process.pid);
console.log('Attach debugger, then press Enter to require…');


console.log(bq.log.get_version());

let config = `appenders_config.appender_0.type=console
appenders_config.appender_0.time_zone =default local time
appenders_config.appender_0.levels=[verbose, debug, info, warning, error, fatal]
appenders_config.appender_0.is_in_sandbox=false
appenders_config.appender_0.max_file_size=10000000
appenders_config.appender_0.expire_time_days=10
appenders_config.appender_0.capacity_limit=200000000

appenders_config.appender_1.type=text_file
appenders_config.appender_1.time_zone =default local time
appenders_config.appender_1.levels=[verbose, debug, info, warning, error, fatal]
appenders_config.appender_1.file_name=output/log
appenders_config.appender_1.is_in_sandbox=false
appenders_config.appender_1.max_file_size=1000000000
appenders_config.appender_1.expire_time_days=10
appenders_config.appender_1.capacity_limit=10000000000
appenders_config.appender_1.always_create_new_file=true

appenders_config.appender_3409.type=compressed_file
appenders_config.appender_3409.time_zone =default local time
appenders_config.appender_3409.levels=[verbose, debug, info, warning, error, fatal]
appenders_config.appender_3409.file_name=output/log
appenders_config.appender_3409.is_in_sandbox=false
appenders_config.appender_3409.max_file_size=1000000000
appenders_config.appender_3409.expire_time_days=10
appenders_config.appender_3409.capacity_limit=8000000000

log.buffer_size=65535
log.recovery=true
log.print_stack_levels=[debug,error,fatal]`;

/*
if (process.stdin.isTTY) {
    process.stdin.setRawMode(true);
}
process.stdin.resume();
process.stdin.once("data", () => {
    if (process.stdin.isTTY) {
        process.stdin.setRawMode(false);
    }
    process.stdin.pause();
});
*/

let dlog = demo_category_log.create_log("test_log2", config);
dlog.debug(dlog.cat.Factory.House, "valid:{}", 2.23423534523451253245234523451243412);
dlog.info(dlog.cat.Transport.Trains, "valid:{}", 33333);


bq.log.force_flush_all_logs();






