package bq.tools;
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
import bq.def.*;
import bq.impl.*;

public class log_decoder implements AutoCloseable {
    public enum appender_decode_result
    {
        success,				//decoding successful, you can call the corresponding function to obtain the decoded log text.
		eof,					//all the content is decoded
		failed_invalid_handle,
		failed_decode_error,
		failed_io_error;

        public static appender_decode_result from_int(int i) {
            return appender_decode_result.values()[i];
        }
    }

    private string_holder decode_text_ = new string_holder();
    private appender_decode_result result_ = appender_decode_result.success;
    private long handle_ = 0;

    public log_decoder(String log_file_absolute_path)
    {
        long create_result = log_invoker.__api_log_decoder_create(log_file_absolute_path, "");
        if(create_result >= 0)
        {
            handle_ = create_result;
            result_ = appender_decode_result.success;
        }else{
            handle_ = 0xFFFFFFFF;
            result_ = appender_decode_result.from_int(-1 * (int)create_result);
        }
    }

    public log_decoder(String log_file_absolute_path, String priv_key)
    {
        long create_result = log_invoker.__api_log_decoder_create(log_file_absolute_path, (priv_key == null) ? "" : priv_key);
        if(create_result >= 0)
        {
            handle_ = create_result;
            result_ = appender_decode_result.success;
        }else{
            handle_ = 0xFFFFFFFF;
            result_ = appender_decode_result.from_int(-1 * (int)create_result);
        }
    }

    @Override
    public void close() {
        log_invoker.__api_log_decoder_destroy(handle_);
    }

	@Override
    protected void finalize() throws Throwable {
        log_invoker.__api_log_decoder_destroy(handle_);
    }

    public appender_decode_result decode()
    {
        if(result_ != appender_decode_result.success)
        {
            return result_;
        }
        int decode_result = log_invoker.__api_log_decoder_decode(handle_, decode_text_);
        result_ = appender_decode_result.from_int(decode_result);
        return result_;
    }

    public appender_decode_result get_last_decode_result()
    {
        return result_;
    }

    public String get_last_decoded_log_entry()
    {
        if(decode_text_.value != null)
        {
            return decode_text_.value;
        }else{
            return "";
        }
    }
    
    public static boolean decode_file(String log_file_path, String output_filey)
    {
    	return log_invoker.__api_log_decode(log_file_path, log_file_path, "");
    }
    
    public static boolean decode_file(String log_file_path, String output_file, String priv_key)
    {
    	return log_invoker.__api_log_decode(log_file_path, log_file_path, priv_key);
    }
}
