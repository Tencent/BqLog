/* Copyright (C) 2025 Tencent.
 * BQLOG is licensed under the Apache License, Version 2.0.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */
#pragma once
/*!
 * \file thread_nx.h
 *
 * \author pippocao
 *
 * simple substitute of std::thread.
 * we exclude STL and libc++ to reduce the final executable and library file size
 *
 * Nintendo Switch, official Nintendo SDK (nn::os) implementation.
 */

#include "bq_common/platform/thread/thread.h"
