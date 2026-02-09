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
/*!
 * bq_log.h UE Adapter
 *
 * \brief
 *
 * \author pippocao
 * \date 2025.10.14
 */
#include "BqLog.h"

void UBqLog::Ensure()
{
	if (need_renew_inst_)
	{
	    bq::string log_name = bq::string(TCHAR_TO_UTF8(*LogName.ToString()));
	    if (CreateType == EBqLogInitType::BqLog_Create)
	    {
	        log_id_ = bq::api::__api_create_log(log_name.c_str(), TCHAR_TO_UTF8(*LogConfig.ToString()), get_category_count(), get_category_names());
	    }
	    else if (CreateType == EBqLogInitType::BqLog_Get)
	    {
	        uint32_t log_count = bq::api::__api_get_logs_count();
	        for (uint32_t i = 0; i < log_count; ++i) {
	            auto id = bq::api::__api_get_log_id_by_index(i);
	            bq::_api_string_def log_name_tmp;
	            if (bq::api::__api_get_log_name_by_id(id, &log_name_tmp)) {
	                if (log_name == log_name_tmp.str) {
	                    log_id_ = id;
	                    break;
	                }
	            }
	        }
	    }
	    if (log_id_ != 0)
	    {
	        merged_log_level_bitmap_ = bq::api::__api_get_log_merged_log_level_bitmap_by_log_id(log_id_);
	        categories_mask_array_ = bq::api::__api_get_log_category_masks_array_by_log_id(log_id_);
	        print_stack_level_bitmap_ = bq::api::__api_get_log_print_stack_level_bitmap_by_log_id(log_id_);
	    }else
	    {
	        merged_log_level_bitmap_ = nullptr;
	        categories_mask_array_ = nullptr;
	        print_stack_level_bitmap_ = nullptr;
	    }
		need_renew_inst_ = false;
	}
}

#if WITH_EDITOR
void UBqLog::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	IsCreateMode = (CreateType == EBqLogInitType::BqLog_Create);
	need_renew_inst_ = true;
}
#endif

void UBqLog::PostLoad()
{
	Super::PostLoad();
	IsCreateMode = (CreateType == EBqLogInitType::BqLog_Create);
	need_renew_inst_ = true;
}