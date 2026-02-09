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
using System.Collections.Generic;

namespace bq
{
    public class category_log : log
    {
        protected category_log() : base()
        {
        }

        /// <summary>
        /// copy constructor
        /// </summary>
        /// <param name="rhs"></param>
        public category_log(log child_inst) : base(child_inst) 
        {
        }

        /// <summary>
        /// Get log categories count
        /// </summary>
        /// <returns></returns>
        public uint get_categories_count()
        {
            return (uint)categories_name_array_.Count;
        }

        /// <summary>
        /// Get names of all categories
        /// </summary>
        /// <returns></returns>
        public List<string> get_categories_name_array()
        {
            return categories_name_array_;
        }
    }
}