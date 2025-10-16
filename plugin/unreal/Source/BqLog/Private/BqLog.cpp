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

#if WITH_EDITOR
void UBqLog::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	IsCreateMode = (CreateType == EBqLogInitType::BqLog_Create);
}

void UBqLog::PostLoad()
{
	Super::PostLoad();
	IsCreateMode = (CreateType == EBqLogInitType::BqLog_Create);
}
#endif