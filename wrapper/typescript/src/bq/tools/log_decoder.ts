/*
 * Copyright (C) 2025 Tencent.
 * BQLOG is licensed under the Apache License, Version 2.0.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */

import { log_invoker } from "../impl/log_invoker";
import { string_holder } from "../def/string_holder";

/** Result of a decode call: success, end of file, or one of the failure codes. */
export enum appender_decode_result {
    success,				//decoding successful, you can call the corresponding function to obtain the decoded log text.
    eof,					//all the content is decoded
    failed_invalid_handle,	//the decoder handle is not valid
    failed_decode_error,	//the file is not a readable bqLog file
    failed_io_error			//the file could not be read
}

/** Decodes an encrypted or compressed bqLog file, one entry at a time. */
export class log_decoder {
    private decode_text_: string_holder = new string_holder();
    private result_: appender_decode_result = appender_decode_result.success;
    private handle_: number = 0xFFFFFFFF;

    /** Opens log_file_absolute_path for decoding; priv_key is only needed for encrypted logs. */
    public constructor(log_file_absolute_path: string, priv_key?: string) {
        let create_result = log_invoker.__api_log_decoder_create(log_file_absolute_path, priv_key);
        if (create_result >= 0) {
            this.handle_ = create_result;
            this.result_ = appender_decode_result.success;
        } else {
            this.result_ = create_result as appender_decode_result;
        }
        log_invoker.__api_attach_decoder_inst(this);
    }

    /** Decodes the next entry and returns the result code; the text goes to get_last_decoded_log_entry. */
    public decode(): appender_decode_result {
        if (this.result_ != appender_decode_result.success) {
            return this.result_;
        }
        let decode_result = log_invoker.__api_log_decoder_decode(this.handle_, this.decode_text_);
        this.result_ = decode_result as appender_decode_result;
        return this.result_;
    }

    /** Returns the result of the last decode call. */
    public get_last_decode_result(): appender_decode_result {
        return this.result_;
    }

    /** Returns the text produced by the last successful decode, or an empty string. */
    public get_last_decoded_log_entry(): string {
        if (this.decode_text_.value) {
            return this.decode_text_.value;
        } else {
            return "";
        }
    }

    /** Decodes a whole log file into output_file, and reports whether it succeeded. */
    public static decode_file(log_file_path: string, output_file: string, priv_key?: string): boolean {
        return log_invoker.__api_log_decode(log_file_path, output_file, priv_key);
    }
}