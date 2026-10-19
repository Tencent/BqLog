package bq.def;
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

/**
 * Base class of the category tree the generator writes for one log. The wrapper reads the index
 * of a category from it, nothing else.
 */
public class log_category_base {
    /** Position of this category in its log, in the order the config file listed them. */
    protected long index;

    /** Constructor for the generated subclasses. */
    public log_category_base() {
    }
    
    /**
     * Index of a category, the number the native side needs to attach an entry to it.
     * @param category The category to read the index from.
     * @return The category's index.
     */
    public static long get_index(log_category_base category)
    {
        return category.index;
    }
}
