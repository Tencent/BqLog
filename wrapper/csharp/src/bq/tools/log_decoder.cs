/*
 * Copyright (C) 2024 Tencent.
 * BQLOG is licensed under the Apache License, Version 2.0.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */
using bq.impl;
using bq.tools;
using System;

namespace bq.tools
{
    public class log_decoder
    {
        public enum appender_decode_result
        {
            success,                //decoding successful, you can call the corresponding function to obtain the decoded log text.
            eof,                    //all the content is decoded
            failed_invalid_handle,
            failed_decode_error,
            failed_io_error
        }
        private string decode_text_ = "";
        private appender_decode_result result_ = appender_decode_result.success;
        private uint handle_ = 0xFFFFFFFF;

        public log_decoder(string log_file_absolute_path)
        {
            decode_text_ = "";
            byte[] utf8_path_str = utf8_encoder.get_utf8_array(log_file_absolute_path);
            unsafe
            {
                fixed (byte* utf8_path_c_str = utf8_path_str)
                {
                    uint handle_tmp;
                    result_ = bq.impl.log_invoker.__api_log_decoder_create(utf8_path_c_str, &handle_tmp);
                    if (result_ != appender_decode_result.success)
                    {
                        handle_ = 0xFFFFFFFF;
                    }
                    else
                    {
                        handle_ = handle_tmp;
                    }
                }
            }
        }

        ~log_decoder()
        {
            bq.impl.log_invoker.__api_log_decoder_destroy(handle_);
        }

        public appender_decode_result decode()
        {
            if (result_ != appender_decode_result.success)
            {
                return result_;
            }
            bq.def._api_string_def text;
            unsafe
            {
                result_ = log_invoker.__api_log_decoder_decode(handle_, &text);

                if (result_ == appender_decode_result.success)
                {
                    decode_text_ = new string(text.str, 0, (int)text.len, System.Text.Encoding.UTF8);
                }
                else
                {
                    decode_text_ = "";
                }
            }
            return result_;
        }

        public appender_decode_result get_last_decode_result()
        { 
            return result_;
        }

        public string get_last_decoded_log_item()
        {
            return decode_text_;
        }
    }
}