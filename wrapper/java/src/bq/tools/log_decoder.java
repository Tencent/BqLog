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

/**
 * Decodes a log file written by bq.log, entry by entry or whole files at once.
 */
public class log_decoder implements AutoCloseable {
        /**
         * Outcome of a decode() call.
         */
    public enum appender_decode_result
    {
        /** Decoding succeeded; get_last_decoded_log_entry() holds the text. */
        success,				//decoding successful, you can call the corresponding function to obtain the decoded log text.
		/** Every entry has been decoded. */
		eof,					//all the content is decoded
		/** The decoder handle is not valid. */
		failed_invalid_handle,
		/** The file is corrupt, or the private key does not match. */
		failed_decode_error,
		/** The file could not be read. */
		failed_io_error;

        /**
         * Maps the result code the native decoder returns to its enum constant.
         * @param i The native result code.
         * @return The matching enum constant.
         */
        public static appender_decode_result from_int(int i) {
            return appender_decode_result.values()[i];
        }
    }

    private string_holder decode_text_ = new string_holder();
    private appender_decode_result result_ = appender_decode_result.success;
    private long handle_ = 0;

    /**
     * Opens a log file that was not encrypted.
     * @param log_file_absolute_path Path of the log file to decode.
     */
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

    /**
     * Opens a log file that was encrypted with a private key.
     * @param log_file_absolute_path Path of the log file to decode.
     * @param priv_key Key the file was encrypted with; pass an empty string when it was not encrypted.
     */
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
    /** Releases the native decoder handle. */
    public void close() {
        log_invoker.__api_log_decoder_destroy(handle_);
    }

    /**
     * Decodes the following entry of the file.
     * @return success, or why the entry could not be decoded.
     */
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

    /**
     * Result of the most recent decode() call.
     * @return The result of the last decode() call.
     */
    public appender_decode_result get_last_decode_result()
    {
        return result_;
    }

    /**
     * Text of the entry the last successful decode() produced.
     * @return The decoded text, or an empty string when there is none.
     */
    public String get_last_decoded_log_entry()
    {
        if(decode_text_.value != null)
        {
            return decode_text_.value;
        }else{
            return "";
        }
    }
    
    /**
     * Decodes a whole file into a text file.
     * @param log_file_path Path of the log file to decode.
     * @param output_file Path of the text file to write.
     * @return True when the file was decoded.
     */
    public static boolean decode_file(String log_file_path, String output_file)
    {
    	return log_invoker.__api_log_decode(log_file_path, output_file, "");
    }
    
    /**
     * Decodes a whole file into a text file, decrypting it with a private key.
     * @param log_file_path Path of the log file to decode.
     * @param output_file Path of the text file to write.
     * @param priv_key Key the file was encrypted with.
     * @return True when the file was decoded.
     */
    public static boolean decode_file(String log_file_path, String output_file, String priv_key)
    {
    	return log_invoker.__api_log_decode(log_file_path, output_file, priv_key);
    }
}
