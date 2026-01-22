#pragma once
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
#include "test_base.h"

namespace bq {
    namespace test {
        class test_layout : public test_base {
        public:
            test_result test() override;

        private:
            test_result test_find_brace_and_copy();
            test_result test_find_brace_and_convert_u16();
            test_result test_throughput();
        };
    }
}
