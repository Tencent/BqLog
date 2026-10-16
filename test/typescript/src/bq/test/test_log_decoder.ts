import { test_base } from "./test_base.ts";
import { test_result } from "./test_result.ts";
import { bq } from "@pippocao/bqlog";
import * as fs from "fs";
import * as path from "path";

function find_compressed_file(folder_path: string, file_prefix: string): string | null {
    if (!fs.existsSync(folder_path)) {
        return null;
    }
    const matches: string[] = [];
    for (const entry of fs.readdirSync(folder_path, { withFileTypes: true })) {
        const entry_path = path.join(folder_path, entry.name);
        if (entry.isDirectory()) {
            // The appender may nest the file under a directory named after it.
            const nested = find_compressed_file(entry_path, file_prefix);
            if (nested !== null) {
                matches.push(nested);
            }
        } else if (entry.name.startsWith(file_prefix) && entry.name.endsWith(".logcompr")) {
            matches.push(entry_path);
        }
    }
    matches.sort((left, right) => fs.statSync(right).mtimeMs - fs.statSync(left).mtimeMs);
    return matches.length > 0 ? matches[0] : null;
}

export class test_log_decoder extends test_base {
    constructor(name: string) {
        super(name);
    }

    public async test(): Promise<test_result> {
        const result = new test_result();
        const log_inst = bq.log.create_log("decoder_log", `log.thread_mode=sync
                        log.recovery=false
                        appenders_config.FileAppender.type=compressed_file
                        appenders_config.FileAppender.file_name=Output/decoder_in
                        appenders_config.FileAppender.base_dir_type=0
                        appenders_config.FileAppender.levels=[info]`);
        result.add_result(log_inst.is_valid(), "the decoder test log is valid");
        log_inst.info("decoder test marker {}", 1);
        log_inst.force_flush();

        const input_path = find_compressed_file("Output", "decoder_in");
        result.add_result(input_path !== null, "the compressed appender wrote a log file");
        if (input_path === null) {
            return result;
        }
        const input_size = fs.statSync(input_path).size;

        const output_path = "Output/decoder_out.txt";
        if (fs.existsSync(output_path)) {
            fs.unlinkSync(output_path);
        }

        const decode_ok = bq.log_decoder.decode_file(input_path, output_path);
        result.add_result(decode_ok, "decode_file reports success");
        result.add_result(fs.existsSync(output_path) && fs.statSync(output_path).size > 0,
            "decode_file wrote the output file");
        // decode_file used to hand the input path in twice, which overwrote the log file.
        result.add_result(fs.existsSync(input_path) && fs.statSync(input_path).size === input_size,
            "decode_file left the input file untouched");

        return result;
    }
}
