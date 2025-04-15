/*
 * Copyright (C) 2024 THL A29 Limited, a Tencent company.
 * BQLOG is licensed under the Apache License, Version 2.0.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */
#include "bq_common/bq_common.h"
#if BQ_IOS
#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
namespace bq {
	namespace platform {
        //According to test result
        //gettimeofday has higher performance than "mach_absolute_time"
        //and "CLOCK_REALTIME_COARSE clock_gettime" on ios.
        //TSC is not recommended because of different hardware architectures.
        uint64_t high_performance_epoch_ms()
        {
            struct timeval tv;
            gettimeofday(&tv, NULL);
            uint64_t ret = tv.tv_usec;
            ret /= 1000;
            ret += ((uint64_t)tv.tv_sec * 1000);
            return ret;
        }

        base_dir_initializer::base_dir_initializer()
        {
            NSString *documents_path = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES).firstObject;
            base_dir_0_ = [documents_path UTF8String];
            NSString* cache_path = NSSearchPathForDirectoriesInDomains(NSCachesDirectory, NSUserDomainMask, YES).firstObject;
		    base_dir_1_ = [cache_path UTF8String];
        }

		const bq::string& get_base_dir(bool is_sandbox)
		{
            return is_sandbox ? common_global_vars::get().base_dir_init_inst_.base_dir_0_ : common_global_vars::get().base_dir_init_inst_.base_dir_1_;
		}
    
        bq::string get_programe_home_path()
        {
            NSString* home_directory = NSHomeDirectory();
            return [home_directory UTF8String];
        }

		void ios_vprintf(const char* __restrict format, va_list args)
		{
			NSLogv([NSString stringWithUTF8String : format], args);
		}

		void ios_print(const char* __restrict content)
		{
            NSLog(@"%@", [NSString stringWithUTF8String : content]);
		}
    
        static thread_local bq::string stack_trace_current_str_;
        static thread_local bq::u16string stack_trace_current_str_u16_;
        void get_stack_trace(uint32_t skip_frame_count, const char*& out_str_ptr, uint32_t& out_char_count)
        {
            stack_trace_current_str_.clear();
            NSArray *symbols = [NSThread callStackSymbols];
            
            uint32_t valid_frame_count = 0;
            if (symbols) {
                for (NSString *trace in symbols)
                {
                    const char* trace_str = [trace UTF8String];
                    if(valid_frame_count == 0)
                    {
                        if(strstr(trace_str, "get_stack_trace"))
                        {
                            continue;
                        }
                        valid_frame_count = 1;
                    }
                    if (valid_frame_count++ <= skip_frame_count)
                    {
                        continue;
                    }
                    auto str_len = strlen(trace_str);
                    stack_trace_current_str_.push_back('\n');
                    stack_trace_current_str_.insert_batch(stack_trace_current_str_.end(), trace_str, (size_t)str_len);
                }
            }
            out_str_ptr = stack_trace_current_str_.begin();
            out_char_count = (uint32_t)stack_trace_current_str_.size();
        }

        void get_stack_trace_utf16(uint32_t skip_frame_count, const char16_t*& out_str_ptr, uint32_t& out_char_count)
        {
            const char* u8_str;
            uint32_t u8_char_count;
            get_stack_trace(skip_frame_count, u8_str, u8_char_count);
            stack_trace_current_str_u16_.clear();
            stack_trace_current_str_u16_.fill_uninitialized((u8_char_count << 1) + 1);
            size_t encoded_size = (size_t)bq::util::utf8_to_utf16(u8_str, u8_char_count, stack_trace_current_str_u16_.begin(), (uint32_t)stack_trace_current_str_u16_.size());
            assert(encoded_size < stack_trace_current_str_u16_.size());
            stack_trace_current_str_u16_.erase(stack_trace_current_str_u16_.begin() + encoded_size, stack_trace_current_str_u16_.size() - encoded_size);
            out_str_ptr = stack_trace_current_str_u16_.begin();
            out_char_count = (uint32_t)stack_trace_current_str_u16_.size();
        }
	}
}
#endif
